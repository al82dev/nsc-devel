#pragma once

#if !defined(_WIN32)
#error AsanSubprocess requires the Windows host used by the NativeSharp compiler suite.
#endif

#include <windows.h>

#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace nsc::suite::source_test_support
{
inline constexpr wchar_t kAsanProbeEnvironment[] = L"NSC_TEST_TS_SRC_049_PROBE";

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
inline constexpr bool kAddressSanitizerBuild = true;
#else
inline constexpr bool kAddressSanitizerBuild = false;
#endif
#elif defined(__SANITIZE_ADDRESS__) || defined(_CRT_USE_ADDRESS_SANITIZER)
inline constexpr bool kAddressSanitizerBuild = true;
#else
inline constexpr bool kAddressSanitizerBuild = false;
#endif

inline bool isAsanProbeProcess() noexcept
{
    wchar_t value[8]{};
    return GetEnvironmentVariableW(kAsanProbeEnvironment, value,
               static_cast<DWORD>(std::size(value))) != 0 && value[0] == L'1';
}

struct AsanSubprocessResult final
{
    bool launched = false;
    bool timedOut = false;
    DWORD exitCode = 0;
    std::string output;
    std::string error;
};

// Runs one case in the current test executable, captures both output streams, and bounds the
// sanitizer probe. The environment marker exists only while CreateProcess takes its snapshot.
inline AsanSubprocessResult runAsanProbe(std::string_view caseId)
{
    AsanSubprocessResult result;
    std::vector<wchar_t> executable(32768, L'\0');
    const DWORD executableLength = GetModuleFileNameW(
        nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (executableLength == 0 || executableLength >= executable.size())
    {
        result.error = "cannot locate current test executable";
        return result;
    }

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE readPipe = INVALID_HANDLE_VALUE;
    HANDLE writePipe = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0))
    {
        result.error = "cannot create sanitizer output pipe";
        return result;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    const DWORD previousLength = GetEnvironmentVariableW(kAsanProbeEnvironment, nullptr, 0);
    std::wstring previous;
    if (previousLength != 0)
    {
        previous.resize(previousLength);
        GetEnvironmentVariableW(
            kAsanProbeEnvironment, previous.data(), static_cast<DWORD>(previous.size()));
        if (!previous.empty() && previous.back() == L'\0') previous.pop_back();
    }
    SetEnvironmentVariableW(kAsanProbeEnvironment, L"1");

    std::wstring command = L"\"" + std::wstring(executable.data(), executableLength) +
        L"\" --case " + std::wstring(caseId.begin(), caseId.end());
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(executable.data(), mutableCommand.data(), nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);

    if (previousLength == 0) SetEnvironmentVariableW(kAsanProbeEnvironment, nullptr);
    else SetEnvironmentVariableW(kAsanProbeEnvironment, previous.c_str());
    CloseHandle(writePipe);

    if (!created)
    {
        CloseHandle(readPipe);
        result.error = "cannot launch sanitizer probe";
        return result;
    }
    result.launched = true;
    CloseHandle(process.hThread);

    const auto drainAvailable = [&]() {
        std::array<char, 4096> chunk{};
        DWORD available = 0;
        while (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available != 0)
        {
            DWORD bytes = 0;
            const DWORD requested = available < chunk.size()
                ? available : static_cast<DWORD>(chunk.size());
            if (!ReadFile(readPipe, chunk.data(), requested, &bytes, nullptr) || bytes == 0) break;
            result.output.append(chunk.data(), bytes);
        }
    };
    DWORD wait = WAIT_TIMEOUT;
    for (unsigned iteration = 0; iteration < 1200 && wait == WAIT_TIMEOUT; ++iteration)
    {
        drainAvailable();
        wait = WaitForSingleObject(process.hProcess, 50);
    }
    if (wait == WAIT_TIMEOUT)
    {
        result.timedOut = true;
        TerminateProcess(process.hProcess, 0x54534F55U);
        WaitForSingleObject(process.hProcess, 5000);
    }
    if (!GetExitCodeProcess(process.hProcess, &result.exitCode))
        result.error = "cannot read sanitizer probe exit code";
    CloseHandle(process.hProcess);

    drainAvailable();
    std::array<char, 4096> chunk{};
    DWORD bytes = 0;
    while (ReadFile(readPipe, chunk.data(), static_cast<DWORD>(chunk.size()), &bytes, nullptr) &&
        bytes != 0)
    {
        result.output.append(chunk.data(), bytes);
    }
    CloseHandle(readPipe);
    return result;
}
}
