#pragma once

#include <Windows.h>

namespace Windows
{
    inline bool SetPrivileges(LPCWSTR privileges)
    {
        HANDLE hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;

        LUID luid;
        if (!LookupPrivilegeValueW(nullptr, privileges, &luid))
        {
            CloseHandle(hToken);
            return false;
        }

        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        bool status = !!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        CloseHandle(hToken);
        return status && (GetLastError() == ERROR_SUCCESS);
    }
}