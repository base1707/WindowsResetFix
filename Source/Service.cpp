#include "Service.h"

CWRService::CWRService() : name{ SERVICE::NAME }, status{}, statusHandle{}
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
    static std::shared_ptr<CWRService> instance{ new CWRService() };
    return instance;
}

void WINAPI CWRService::SvcMain(DWORD argc, TCHAR* argv[])
{
    auto instance = GetInstance();

    if (instance->statusHandle = RegisterServiceCtrlHandlerEx(instance->name, ServiceCtrlHandler, nullptr))
    {
        instance->Start(argc, argv);
    }
    else
    {
        instance->WriteToEventLog(L"RegisterServiceCtrlHandlerEx() failed!", EVENTLOG_ERROR_TYPE);
    }
}

DWORD WINAPI CWRService::ServiceCtrlHandler(DWORD Code, DWORD, void*, void*)
{
    auto instance = GetInstance();

    switch (Code)
    {
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

        // Unsupported
        default:
        {
        } break;
    }
    return 0;
}

bool CWRService::Install()
{
    CString path;
    TCHAR* modulePath = path.GetBufferSetLength(MAX_PATH);

    if (GetModuleFileName(nullptr, modulePath, MAX_PATH) == 0)
        return false;

    path.ReleaseBuffer();
    path.Remove(L'\"');
    path = L'\"' + path + L'\"';

    CServiceHandle controlManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
    if (!controlManager)
        return false;

    CServiceHandle handle = CreateService(controlManager, name, name, SERVICE_QUERY_STATUS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, path, nullptr, nullptr, nullptr, nullptr, nullptr);
    return !!handle;
}

bool CWRService::Uninstall()
{
    CServiceHandle controlManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!controlManager)
        return false;

    CServiceHandle handle = OpenService(controlManager, name, SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE);
    if (!handle)
        return false;

    SERVICE_STATUS status{};
    if (ControlService(handle, SERVICE_CONTROL_STOP, &status))
    {
        while (QueryServiceStatus(handle, &status))
        {
            if (status.dwCurrentState != SERVICE_STOP_PENDING)
                break;
        }

        if (status.dwCurrentState != SERVICE_STOPPED)
            return false;
    }
    return !!DeleteService(handle);
}

bool CWRService::Run()
{
    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        { const_cast<CString&>(GetInstance()->name).GetBuffer(), SvcMain },
        { nullptr, nullptr }
    };

    return StartServiceCtrlDispatcher(ServiceTable) == TRUE;
}

void CWRService::OnStart(DWORD, TCHAR* [])
{
    static const std::size_t HASH_SIZE = 20;
    std::ifstream file{ SERVICE::HASH_PATH, std::ios::binary };
    std::string buffer;

    // Reading
    if (file.is_open())
    {
        auto alarmMode = [&]()
        {
            file.close();

            // Legacy API, sorry
            if (Windows::SetPrivileges(SE_SHUTDOWN_NAME))
            {
                ExitWindowsEx(EWX_FORCE | EWX_SHUTDOWN, 0);
            }
            else
            {
                WriteToEventLog(L"ExitWindowsEx() failed!", EVENTLOG_ERROR_TYPE);
            }
        };

        for (auto& it : SERVICE::TARGET_PATHS)
        {
            buffer = Filesystem::GetHashFromFile(it);
            if (buffer != Filesystem::ReadHash(file, HASH_SIZE))
            {
                alarmMode();
                return;
            }
        }
    }
    // Writing
    else
    {
        std::ofstream hashFile{ SERVICE::HASH_PATH, std::ios::binary};
        if (!hashFile.is_open())
        {
            WriteToEventLog("hashFile broken!", EVENTLOG_ERROR_TYPE);
            return;
        }

        for (auto& it : SERVICE::TARGET_PATHS)
        {
            buffer = Filesystem::GetHashFromFile(it);
            if (!Filesystem::WriteHash(hashFile, buffer, HASH_SIZE))
            {
                WriteToEventLog("hashFile broken!", EVENTLOG_ERROR_TYPE);
                return;
            }
        }
    }
}

void CWRService::SetStatus(DWORD dwState, DWORD dwErrCode, DWORD dwWait)
{
    status.dwCurrentState = dwState;
    status.dwWin32ExitCode = dwErrCode;
    status.dwWaitHint = dwWait;

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
    if (HANDLE hSource = RegisterEventSource(nullptr, name))
    {
        const TCHAR* msgData[2] = { name, msg };
        ReportEvent(hSource, type, 0, 0, nullptr, 2, 0, msgData, nullptr);
        DeregisterEventSource(hSource);
    }
}

CServiceHandle::CServiceHandle(SC_HANDLE serviceHandle) : handle(serviceHandle) 
{}

CServiceHandle::~CServiceHandle()
{
    if (handle)
    {
        CloseServiceHandle(handle);
    }
}

CServiceHandle::operator SC_HANDLE()
{
    return handle;
}