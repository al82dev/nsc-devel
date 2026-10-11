#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace nsc::suite::isolated_process_support
{
struct Result
{
    bool started = false;
    DWORD exitCode = 0;
    std::string output;
    std::string error;
};

namespace detail
{
inline std::wstring executablePath()
{
    std::vector<wchar_t> buffer(512);
    for (;;)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
            static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return {};
        }
        if (length + 1 < buffer.size())
        {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

inline std::wstring widenAscii(std::string_view value)
{
    return std::wstring(value.begin(), value.end());
}

inline std::wstring quote(std::wstring_view value)
{
    std::wstring result;
    result.reserve(value.size() + 2);
    result.push_back(L'"');
    for (wchar_t character : value)
    {
        if (character == L'"') result.push_back(L'\\');
        result.push_back(character);
    }
    result.push_back(L'"');
    return result;
}

class ScopedEnvironment final
{
public:
    ScopedEnvironment(std::wstring name, std::wstring value) : name_(std::move(name))
    {
        SetLastError(ERROR_SUCCESS);
        const DWORD required = GetEnvironmentVariableW(name_.c_str(), nullptr, 0);
        existed_ = required != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
        if (required != 0)
        {
            std::vector<wchar_t> buffer(required);
            GetEnvironmentVariableW(name_.c_str(), buffer.data(), required);
            previous_.assign(buffer.data());
        }
        SetEnvironmentVariableW(name_.c_str(), value.c_str());
    }

    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;

    ~ScopedEnvironment()
    {
        SetEnvironmentVariableW(name_.c_str(), existed_ ? previous_.c_str() : nullptr);
    }

private:
    std::wstring name_;
    std::wstring previous_;
    bool existed_ = false;
};

inline std::mutex& environmentMutex()
{
    static std::mutex mutex;
    return mutex;
}
}

inline std::string probeMode()
{
    const DWORD required = GetEnvironmentVariableA("NSC_COMPONENT_TEST_PROBE", nullptr, 0);
    if (required == 0) return {};
    std::vector<char> buffer(required);
    GetEnvironmentVariableA("NSC_COMPONENT_TEST_PROBE", buffer.data(), required);
    return std::string(buffer.data());
}

// Starts a fresh copy of the component-test executable for a single case.  A reader thread
// drains the shared stdout/stderr pipe while the process runs. Execution has no automatic
// deadline; cancellation is owned by the outer controller and terminates the process tree.
inline Result runCurrentExecutable(std::string_view caseId, std::string_view probe,
    const std::vector<std::pair<std::wstring, std::wstring>>& environment = {})
{
    Result result;
    const std::wstring executable = detail::executablePath();
    if (executable.empty())
    {
        result.error = "GetModuleFileNameW failed";
        return result;
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &attributes, 0))
    {
        result.error = "CreatePipe failed";
        return result;
    }
    if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        result.error = "SetHandleInformation failed";
        return result;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;

    PROCESS_INFORMATION process{};
    std::wstring command = detail::quote(executable) + L" --case " +
        detail::quote(detail::widenAscii(caseId));
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    DWORD createError = ERROR_SUCCESS;

    {
        // CreateProcess inherits the process environment, so serialize the short override window
        // and restore every value immediately after the child has captured it.
        std::lock_guard lock(detail::environmentMutex());
        detail::ScopedEnvironment mode(L"NSC_COMPONENT_TEST_PROBE", detail::widenAscii(probe));
        std::vector<std::unique_ptr<detail::ScopedEnvironment>> overrides;
        overrides.reserve(environment.size());
        for (const auto& [name, value] : environment)
            overrides.push_back(std::make_unique<detail::ScopedEnvironment>(name, value));

        result.started = CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr, nullptr,
            TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE;
        if (!result.started) createError = GetLastError();
    }

    CloseHandle(writePipe);
    if (!result.started)
    {
        CloseHandle(readPipe);
        result.error = "CreateProcessW failed: " + std::to_string(createError);
        return result;
    }

    std::thread reader([&result, readPipe] {
        char buffer[4096];
        DWORD read = 0;
        while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read != 0)
            result.output.append(buffer, read);
        CloseHandle(readPipe);
    });

    const DWORD wait = WaitForSingleObject(process.hProcess, INFINITE);
    if (wait == WAIT_FAILED)
    {
        result.error = "WaitForSingleObject failed: " + std::to_string(GetLastError());
        TerminateProcess(process.hProcess, 0xEF);
        WaitForSingleObject(process.hProcess, INFINITE);
    }

    GetExitCodeProcess(process.hProcess, &result.exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    reader.join();
    return result;
}
}
