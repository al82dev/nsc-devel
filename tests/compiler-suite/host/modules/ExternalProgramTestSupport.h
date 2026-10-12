#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace nsc::suite::external_program_support
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
inline std::wstring quote(std::wstring_view value)
{
    std::wstring result = L"\"";
    for (const wchar_t character : value)
    {
        if (character == L'\"') result.push_back(L'\\');
        result.push_back(character);
    }
    result.push_back(L'\"');
    return result;
}

inline std::mutex& environmentMutex()
{
    static std::mutex mutex;
    return mutex;
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
}

// Runs one explicitly selected executable with captured stdout/stderr and optional stdin. There
// is intentionally no deadline: the Builder owns cancellation of the complete process subtree.
inline Result run(const std::filesystem::path& executable,
    const std::vector<std::wstring>& arguments, std::string_view input,
    const std::filesystem::path& workingDirectory,
    const std::vector<std::pair<std::wstring, std::wstring>>& environment = {})
{
    Result result;
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE stdoutRead = nullptr;
    HANDLE stdoutWrite = nullptr;
    HANDLE stdinRead = nullptr;
    HANDLE stdinWrite = nullptr;
    if (!CreatePipe(&stdoutRead, &stdoutWrite, &attributes, 0) ||
        !SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0) ||
        !CreatePipe(&stdinRead, &stdinWrite, &attributes, 0) ||
        !SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0))
    {
        if (stdoutRead != nullptr) CloseHandle(stdoutRead);
        if (stdoutWrite != nullptr) CloseHandle(stdoutWrite);
        if (stdinRead != nullptr) CloseHandle(stdinRead);
        if (stdinWrite != nullptr) CloseHandle(stdinWrite);
        result.error = "pipe creation failed: " + std::to_string(GetLastError());
        return result;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = stdinRead;
    startup.hStdOutput = stdoutWrite;
    startup.hStdError = stdoutWrite;

    std::wstring command = detail::quote(executable.wstring());
    for (const std::wstring& argument : arguments)
        command += L" " + detail::quote(argument);
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    PROCESS_INFORMATION process{};
    DWORD createError = ERROR_SUCCESS;
    {
        std::lock_guard lock(detail::environmentMutex());
        std::vector<std::unique_ptr<detail::ScopedEnvironment>> overrides;
        overrides.reserve(environment.size());
        for (const auto& [name, value] : environment)
            overrides.push_back(std::make_unique<detail::ScopedEnvironment>(name, value));
        result.started = CreateProcessW(executable.c_str(), mutableCommand.data(), nullptr,
            nullptr, TRUE, CREATE_NO_WINDOW, nullptr, workingDirectory.c_str(), &startup,
            &process) != FALSE;
        if (!result.started) createError = GetLastError();
    }

    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);
    if (!result.started)
    {
        CloseHandle(stdinWrite);
        CloseHandle(stdoutRead);
        result.error = "CreateProcessW failed: " + std::to_string(createError);
        return result;
    }

    std::thread reader([&result, stdoutRead] {
        char buffer[4096];
        DWORD count = 0;
        while (ReadFile(stdoutRead, buffer, sizeof(buffer), &count, nullptr) && count != 0)
            result.output.append(buffer, count);
        CloseHandle(stdoutRead);
    });

    std::size_t offset = 0;
    while (offset < input.size())
    {
        DWORD written = 0;
        const DWORD remaining = static_cast<DWORD>((std::min)(
            input.size() - offset, static_cast<std::size_t>(0xffffffffU)));
        if (!WriteFile(stdinWrite, input.data() + offset, remaining, &written, nullptr))
        {
            const DWORD writeError = GetLastError();
            if (writeError != ERROR_BROKEN_PIPE)
                result.error = "stdin WriteFile failed: " + std::to_string(writeError);
            break;
        }
        offset += written;
    }
    CloseHandle(stdinWrite);

    if (WaitForSingleObject(process.hProcess, INFINITE) == WAIT_FAILED)
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
