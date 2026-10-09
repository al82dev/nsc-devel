#pragma once

#include "WriterWin32Handle.h"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

namespace nsc::suite::writer
{
class ADriveTemporaryEnvironment
{
public:
    // Redirects TEMP and TMP to the authorized A: workspace for one isolated writer case.
    ADriveTemporaryEnvironment()
        : oldTemp_(environmentValue(L"TEMP")),
          oldTmp_(environmentValue(L"TMP")),
          root_(std::filesystem::current_path())
    {
        if (root_.root_name() != L"A:")
        {
            throw std::runtime_error("transactional-writer tests require an A:-drive workspace");
        }
        if (!SetEnvironmentVariableW(L"TEMP", root_.c_str()))
        {
            throw std::runtime_error("cannot redirect transactional-writer temporaries to A:");
        }
        if (!SetEnvironmentVariableW(L"TMP", root_.c_str()))
        {
            restoreEnvironmentValue(L"TEMP", oldTemp_);
            throw std::runtime_error("cannot redirect transactional-writer temporaries to A:");
        }
    }

    ADriveTemporaryEnvironment(const ADriveTemporaryEnvironment&) = delete;
    ADriveTemporaryEnvironment& operator=(const ADriveTemporaryEnvironment&) = delete;

    // Restores the exact environment state observed at construction, including absent variables.
    ~ADriveTemporaryEnvironment()
    {
        restoreEnvironmentValue(L"TEMP", oldTemp_);
        restoreEnvironmentValue(L"TMP", oldTmp_);
    }

private:
    // Reads one environment value while preserving absent versus present values in O(n) time and memory.
    static std::optional<std::wstring> environmentValue(const wchar_t* name)
    {
        const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
        if (required == 0)
        {
            return std::nullopt;
        }
        std::wstring value(required, L'\0');
        const DWORD written = GetEnvironmentVariableW(name, value.data(), required);
        if (written == 0 || written >= required)
        {
            throw std::runtime_error("cannot read transactional-writer environment");
        }
        value.resize(written);
        return value;
    }

    // Restores one captured value without throwing during rollback or destruction.
    static void restoreEnvironmentValue(
        const wchar_t* name,
        const std::optional<std::wstring>& value) noexcept
    {
        SetEnvironmentVariableW(name, value ? value->c_str() : nullptr);
    }

    std::optional<std::wstring> oldTemp_;
    std::optional<std::wstring> oldTmp_;
    std::filesystem::path root_;
};
}
