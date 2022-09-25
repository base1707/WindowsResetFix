#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <filesystem>
#include <Windows.h>

namespace Windows
{
    inline bool SetPrivileges(LPCWSTR privileges)
    {
        HANDLE hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;

        LUID takeOwnershipValue;
        if (!LookupPrivilegeValue(0, privileges, &takeOwnershipValue))
        {
            CloseHandle(hToken);
            return false;
        }

        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = takeOwnershipValue;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        bool status = !!AdjustTokenPrivileges(hToken, false, &tp, sizeof(TOKEN_PRIVILEGES), 0, 0);
        CloseHandle(hToken);
        return status;
    }
}

namespace Filesystem
{
    // # bytes -> hash -> string
    inline std::string GetHashFromFile(std::filesystem::path path) noexcept
    {
        if (path.empty())
            return {};

        std::ifstream file{ path, std::ios::binary };
        if (!file.is_open())
            return {};

        auto ParseFile = [](std::ifstream& file)
        {
            return std::string{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        };

        std::string result = std::to_string(std::hash<std::string>{}(ParseFile(file)));
        file.close();

        return result;
    }

    // # Read next n-chars from a file containing a hash
    inline std::string ReadHash(std::ifstream& source, std::size_t count = 20)
    {
        if (!source.is_open())
            return {};

        std::string buffer;
        auto iter = std::istreambuf_iterator<char>(source.rdbuf());
        std::copy_n(iter, count, std::back_inserter(buffer));
        ++iter;

        return buffer;
    }

    // # Write next n-chars to the file containing a hash
    inline bool WriteHash(std::ofstream& source, std::string hash, std::size_t count = 20)
    {
        if (!source.is_open() || hash.empty())
            return false;

        if (hash.size() > count)
            hash.resize(count);

        source << hash;
        return true;
    }
}