#include <iostream>
#include "Service.h"

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "bcrypt.lib")

int _tmain(int argc, TCHAR* argv[])
{
    auto instance = CWRService::GetInstance();

    if (argc == 1)
    {
        if (instance->Run())
            return 0;
        else
            return 1;
    }
    else if (argc == 2)
    {
        std::wstring buffer{ argv[1] };
        if (buffer == L"install")
        {
            std::wcout << L"# Installing service..." << std::endl;
            if (instance->Install())
            {
                std::wcout << L"# Service installed!" << std::endl;
                return 0;
            }
            else
            {
                std::wcout << L"! [ERROR]: Installing failed: " << GetLastError() << std::endl;
                return 4;
            }
        }
        else if (buffer == L"uninstall")
        {
            std::wcout << L"# Uninstalling service..." << std::endl;
            if (instance->Uninstall())
            {
                std::wcout << L"# Service uninstalled!" << std::endl;
                return 0;
            }
            else
            {
                std::wcout << L"! [ERROR]: Uninstalling failed: " << GetLastError() << std::endl;
                return 4;
            }
        }
        else
        {
            std::wcout << L"! [ERROR]: Incorrect argument. Use 'install' or 'uninstall'" << std::endl;
            return 1;
        }
    }
    else
    {
        std::wcout << L"! [ERROR]: Incorrect arguments count!" << std::endl;
        return 1;
    }
}