#pragma once

#include <vector>
#include <memory>
#include <atlstr.h>
#include <WtsApi32.h>

#include "API.h"

namespace SERVICE
{
    static std::vector<LPCSTR> TARGET_PATHS =
    {
        "C:\\Windows\\System32\\utilman.exe",
        "C:\\Windows\\System32\\find.exe",
        "C:\\Windows\\System32\\osk.exe"
    };

    constexpr LPCSTR HASH_PATH = "C:\\Windows\\System32\\BASE1707.hash";
    constexpr LPCWSTR NAME = L"FixWR";
}

class IService
{
public:
    virtual ~IService() = default;

public:
    virtual bool Install() = 0;
    virtual bool Uninstall() = 0;

public:
    virtual bool Run() = 0;
    virtual void OnStart(DWORD argc, TCHAR* argv[]) = 0;

private:
    virtual void SetStatus(DWORD dwState, DWORD dwErrCode, DWORD dwWait) = 0;

public:
    virtual void Start(DWORD argc, TCHAR* argv[]) = 0;
    virtual void Stop() = 0;
    virtual void Pause() = 0;
    virtual void Continue() = 0;
    virtual void Shutdown() = 0;

private:
    virtual void WriteToEventLog(const CString& msg, WORD type) = 0;
};

class CWRService final : public IService
{
private:
    CString name;

private:
    SERVICE_STATUS status;
    SERVICE_STATUS_HANDLE statusHandle;

public:
    CWRService(const CWRService&) = delete;
    CWRService& operator=(const CWRService&) = delete;

    CWRService(CWRService&&) = delete;
    CWRService& operator=(CWRService&&) = delete;

    CWRService();

public:
    static std::shared_ptr<CWRService> GetInstance();
    static void WINAPI SvcMain(DWORD argc, TCHAR* argv[]);
    static DWORD WINAPI ServiceCtrlHandler(DWORD Code, DWORD, void*, void*);

public:
    bool Install() override;
    bool Uninstall() override;

public:
    bool Run() override;
    void OnStart(DWORD argc, TCHAR* argv[]) override;

private:
    void SetStatus(DWORD dwState, DWORD dwErrCode = NO_ERROR, DWORD dwWait = 0) override;

public:
    void Start(DWORD argc, TCHAR* argv[]) override;
    void Stop() override;
    void Pause() override;
    void Continue() override;
    void Shutdown() override;

private:
    void WriteToEventLog(const CString& msg, WORD type = EVENTLOG_INFORMATION_TYPE) override;
};

class CServiceHandle final
{
private:
    SC_HANDLE handle;

public:
    CServiceHandle(const CServiceHandle&) = delete;
    CServiceHandle& operator=(const CServiceHandle&) = delete;

    CServiceHandle(CServiceHandle&&) = delete;
    CServiceHandle& operator=(CServiceHandle&&) = delete;

    CServiceHandle(SC_HANDLE serviceHandle);
    ~CServiceHandle();

public:
    operator SC_HANDLE();
};