#include "Service.h"

CWRService::CWRService() : name(SERVICE::NAME), status{}, statusHandle(nullptr)
{
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE;
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = SERVICE_START_PENDING;
    status.dwWin32ExitCode = NO_ERROR;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint = 0;
    status.dwWaitHint = 0;
}

std::shared_ptr<CWRService> CWRService::GetInstance()
{
    static std::shared_ptr<CWRService> instance(new CWRService());
    return instance;
}

void WINAPI CWRService::SvcMain(DWORD argc, TCHAR* argv[])
{
    auto instance = GetInstance();
    instance->statusHandle = RegisterServiceCtrlHandlerEx(
        instance->name,
        ServiceCtrlHandler,
        nullptr
    );

    if (!instance->statusHandle)
    {
        instance->WriteToEventLog(L"RegisterServiceCtrlHandlerEx failed", EVENTLOG_ERROR_TYPE);
        return;
    }

    instance->Start(argc, argv);
}

DWORD WINAPI CWRService::ServiceCtrlHandler(DWORD code, DWORD eventType, void*, void*)
{
    auto instance = GetInstance();

    switch (code)
    {
        case SERVICE_CONTROL_SESSIONCHANGE:
        {
            if (eventType == WTS_SESSION_LOGON)
                instance->WriteToEventLog(L"New login session detected");
        } break;

        case SERVICE_CONTROL_INTERROGATE:
        {
            SetServiceStatus(instance->statusHandle, &instance->status);
        } break;

        case SERVICE_CONTROL_STOP:
        {
            instance->Stop();
        } break;

        case SERVICE_CONTROL_PAUSE:
        {
            instance->Pause();
        } break;

        case SERVICE_CONTROL_CONTINUE:
        {
            instance->Continue();
        } break;

        case SERVICE_CONTROL_SHUTDOWN:
        {
            instance->Shutdown();
        } break;

        default:
        {
        } break;
    }
    return NO_ERROR;
}

bool CWRService::Install()
{
    TCHAR modulePath[MAX_PATH];
    if (!GetModuleFileName(nullptr, modulePath, MAX_PATH))
    {
        WriteToEventLog(L"GetModuleFileName failed", EVENTLOG_ERROR_TYPE);
        return false;
    }

    CString path(modulePath);
    path.Remove(L'\"');
    path = L'\"' + path + L'\"';

    CServiceHandle scManager(OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (!scManager.IsValid())
    {
        CString errorMsg;
        errorMsg.Format(L"OpenSCManager failed (Error: %d)", GetLastError());
        WriteToEventLog(errorMsg, EVENTLOG_ERROR_TYPE);
        return false;
    }

    CServiceHandle service(CreateServiceW(
        static_cast<SC_HANDLE>(scManager),
        SERVICE::NAME,
        SERVICE::NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        path,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    ));

    if (!service.IsValid())
    {
        CString errorMsg;
        errorMsg.Format(L"CreateService failed (Error: %d)", GetLastError());
        WriteToEventLog(errorMsg, EVENTLOG_ERROR_TYPE);
        return false;
    }

    SERVICE_DESCRIPTION description;
    CString descText = L"Служба для защиты системных утилит (utilman, find, osk, sethc) от подмены.";
    description.lpDescription = descText.GetBuffer();
    descText.ReleaseBuffer();
    if (!ChangeServiceConfig2(static_cast<SC_HANDLE>(service), SERVICE_CONFIG_DESCRIPTION, &description))
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(L"Service description error: 0x%08X", err);
        WriteToEventLog(msg, EVENTLOG_WARNING_TYPE);
    }

    SC_ACTION actions[] = { { SC_ACTION_RESTART, 60000 } };
    SERVICE_FAILURE_ACTIONS failureActions = { 86400, nullptr, nullptr, 1, actions };
    if (!ChangeServiceConfig2(static_cast<SC_HANDLE>(service), SERVICE_CONFIG_FAILURE_ACTIONS, &failureActions))
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(L"Failure actions error: 0x%08X", err);
        WriteToEventLog(msg, EVENTLOG_WARNING_TYPE);
    }

    WriteToEventLog(L"Service installed successfully");
    return true;
}

bool CWRService::Uninstall()
{
    CServiceHandle scManager(OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scManager.IsValid())
    {
        WriteToEventLog(L"OpenSCManager failed", EVENTLOG_ERROR_TYPE);
        return false;
    }

    CServiceHandle service(OpenService(static_cast<SC_HANDLE>(scManager), name, SERVICE_STOP | DELETE));
    if (!service.IsValid())
    {
        WriteToEventLog(L"OpenService failed", EVENTLOG_ERROR_TYPE);
        return false;
    }

    SERVICE_STATUS status{};
    if (ControlService(static_cast<SC_HANDLE>(service), SERVICE_CONTROL_STOP, &status))
    {
        while (QueryServiceStatus(static_cast<SC_HANDLE>(service), &status))
        {
            if (status.dwCurrentState == SERVICE_STOPPED) break;
            Sleep(500);
        }
    }

    if (!DeleteService(static_cast<SC_HANDLE>(service)))
    {
        WriteToEventLog(L"DeleteService failed", EVENTLOG_ERROR_TYPE);
        return false;
    }

    WriteToEventLog(L"Service uninstalled");
    return true;
}

bool CWRService::Run()
{
    CString serviceName = name;
    SERVICE_TABLE_ENTRY dispatchTable[] =
    {
        { serviceName.GetBuffer(), SvcMain },
        { nullptr, nullptr }
    };
    serviceName.ReleaseBuffer();

    if (!StartServiceCtrlDispatcher(dispatchTable))
    {
        WriteToEventLog(L"StartServiceCtrlDispatcher failed", EVENTLOG_ERROR_TYPE);
        return false;
    }
    return true;
}

void CWRService::OnStart(DWORD, TCHAR* [])
{
    try
    {
        std::vector<std::vector<BYTE>> validHashes;
        std::ifstream hashFile(SERVICE::HASH_PATH, std::ios::binary);

        if (hashFile)
        {
            for (const auto& target : SERVICE::TARGET_PATHS)
            {
                auto currentHash = GetFileHash(target);
                std::vector<BYTE> storedHash(HASH_SIZE);

                if (!hashFile.read(reinterpret_cast<char*>(storedHash.data()), storedHash.size()) || currentHash != storedHash)
                {
                    TriggerAlarm();
                    return;
                }
            }

            if (hashFile.get() != EOF)
            {
                WriteToEventLog(L"Hash file corrupted", EVENTLOG_WARNING_TYPE);
                TriggerAlarm();
                return;
            }

            WriteToEventLog(L"Hash is ok!", EVENTLOG_SUCCESS);
        }
        else
        {
            std::ofstream newHashFile(SERVICE::HASH_PATH, std::ios::binary);
            if (!newHashFile)
            {
                WriteToEventLog(L"Failed to create hash file", EVENTLOG_ERROR_TYPE);
                return;
            }

            for (const auto& target : SERVICE::TARGET_PATHS)
            {
                auto hash = GetFileHash(target);
                newHashFile.write(reinterpret_cast<const char*>(hash.data()), hash.size());
            }

            WriteToEventLog(L"Hash file created", EVENTLOG_SUCCESS);
        }
    }
    catch (const std::exception& ex)
    {
        CString msg;
        msg.Format(L"Critical error: %S", ex.what());
        WriteToEventLog(msg, EVENTLOG_ERROR_TYPE);
    }
}

void CWRService::TriggerAlarm()
{
    if (Windows::SetPrivileges(SE_SHUTDOWN_NAME))
    {
        WriteToEventLog(L"Trigger!", EVENTLOG_ERROR_TYPE);
        ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 0);
    }
    else
    {
        WriteToEventLog(L"Failed to acquire shutdown privileges", EVENTLOG_ERROR_TYPE);
    }
}

void CWRService::SetStatus(DWORD state, DWORD errorCode, DWORD waitHint)
{
    status.dwCurrentState = state;
    status.dwWin32ExitCode = errorCode;
    status.dwWaitHint = waitHint;
    SetServiceStatus(statusHandle, &status);
}

void CWRService::Start(DWORD argc, TCHAR* argv[])
{
    SetStatus(SERVICE_START_PENDING);
    OnStart(argc, argv);
    SetStatus(SERVICE_RUNNING);
}

void CWRService::Stop()
{
    SetStatus(SERVICE_STOP_PENDING);
    SetStatus(SERVICE_STOPPED);
}

void CWRService::Pause()
{
    SetStatus(SERVICE_PAUSE_PENDING);
    SetStatus(SERVICE_PAUSED);
}

void CWRService::Continue()
{
    SetStatus(SERVICE_CONTINUE_PENDING);
    SetStatus(SERVICE_RUNNING);
}

void CWRService::Shutdown()
{
    SetStatus(SERVICE_STOPPED);
}

void CWRService::WriteToEventLog(const CString& msg, WORD type)
{
    if (HANDLE eventSrc = RegisterEventSource(nullptr, name))
    {
        const TCHAR* strings[2] = { name, msg };
        ReportEvent(eventSrc, type, 0, 0, nullptr, 2, 0, strings, nullptr);
        DeregisterEventSource(eventSrc);
    }
}

std::vector<BYTE> CWRService::GetFileHash(const std::wstring& path)
{
    constexpr LPCWSTR ALGORITHM = BCRYPT_SHA256_ALGORITHM;
    BCRYPT_ALG_HANDLE hAlg = NULL;
    if (BCryptOpenAlgorithmProvider(&hAlg, ALGORITHM, NULL, 0) != 0)
    {
        WriteToEventLog(L"Algorithm provider error", EVENTLOG_ERROR_TYPE);
        return {};
    }

    BCRYPT_HASH_HANDLE hHash = NULL;
    if (BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) != 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        WriteToEventLog(L"Hash creation error", EVENTLOG_ERROR_TYPE);
        return {};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        WriteToEventLog(L"File open error", EVENTLOG_ERROR_TYPE);
        return {};
    }

    constexpr size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0)
    {
        if (BCryptHashData(hHash, reinterpret_cast<PUCHAR>(buffer), static_cast<ULONG>(file.gcount()), 0) != 0)
        {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            WriteToEventLog(L"Hashing error", EVENTLOG_ERROR_TYPE);
            return {};
        }
    }

    DWORD hashSize = 0;
    DWORD resultSize = 0;
    if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize), &resultSize, 0) != 0)
    {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        WriteToEventLog(L"Hash size error", EVENTLOG_ERROR_TYPE);
        return {};
    }

    std::vector<BYTE> result(hashSize);
    NTSTATUS status = BCryptFinishHash(hHash, result.data(), hashSize, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    if (status != 0)
    {
        WriteToEventLog(L"Hash finalization error", EVENTLOG_ERROR_TYPE);
        return {};
    }
    return result;
}