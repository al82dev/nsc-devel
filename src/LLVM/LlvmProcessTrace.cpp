#include "nativesharp/LLVM/LlvmProcessTrace.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace nativesharp::llvm_backend
{
namespace
{
std::atomic_bool processTraceActive{false};

#if defined(_WIN32)
std::string windowsFailure(std::string_view operation)
{
    std::ostringstream text;
    text << operation << " failed with Win32 error " << ::GetLastError();
    return text.str();
}

HANDLE duplicateStandardHandle(DWORD identifier, DWORD nullAccess)
{
    const HANDLE source = ::GetStdHandle(identifier);
    HANDLE duplicate = INVALID_HANDLE_VALUE;
    if (source != nullptr && source != INVALID_HANDLE_VALUE &&
        ::DuplicateHandle(
            ::GetCurrentProcess(), source, ::GetCurrentProcess(), &duplicate, 0, TRUE,
            DUPLICATE_SAME_ACCESS))
    {
        return duplicate;
    }

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    return ::CreateFileW(
        L"NUL", nullAccess, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
}

std::wstring currentExecutablePath()
{
    std::vector<wchar_t> buffer(1024);
    for (;;)
    {
        const DWORD length =
            ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return {};
        if (length < buffer.size() - 1) return std::wstring(buffer.data(), length);
        if (buffer.size() >= 32768) return {};
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring widenAscii(std::string_view text)
{
    return std::wstring(text.begin(), text.end());
}
#endif
}

void setLlvmProcessTraceActive(bool active) noexcept
{
    processTraceActive.store(active, std::memory_order_relaxed);
}

bool llvmProcessTraceActive() noexcept
{
    return processTraceActive.load(std::memory_order_relaxed);
}

bool llvmProcessTraceRequired(std::span<const std::string_view> arguments) noexcept
{
    bool optimizedPipeline = false;
    bool traceRequested = false;
    for (const std::string_view argument : arguments)
    {
        optimizedPipeline = optimizedPipeline ||
            argument == "-O1" || argument == "-O2" || argument == "-O3";
        traceRequested = traceRequested || argument == "--llvm-pass-trace";
    }
    return optimizedPipeline && traceRequested;
}

bool runCurrentExecutableWithCapturedStandardError(
    std::string_view childArgument,
    LlvmTraceChildResult& result)
{
    result = LlvmTraceChildResult{};
#if defined(_WIN32)
    const std::wstring executable = currentExecutablePath();
    if (executable.empty())
    {
        result.launchError = windowsFailure("GetModuleFileNameW");
        return false;
    }

    wchar_t temporaryDirectory[MAX_PATH + 1]{};
    const DWORD directoryLength = ::GetTempPathW(MAX_PATH, temporaryDirectory);
    if (directoryLength == 0 || directoryLength > MAX_PATH)
    {
        result.launchError = windowsFailure("GetTempPathW");
        return false;
    }

    wchar_t temporaryFile[MAX_PATH + 1]{};
    if (::GetTempFileNameW(temporaryDirectory, L"nst", 0, temporaryFile) == 0)
    {
        result.launchError = windowsFailure("GetTempFileNameW");
        return false;
    }
    const std::filesystem::path capturePath(temporaryFile);

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    const HANDLE capturedError = ::CreateFileW(
        temporaryFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, &security,
        CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (capturedError == INVALID_HANDLE_VALUE)
    {
        result.launchError = windowsFailure("CreateFileW(stderr capture)");
        std::error_code ignored;
        std::filesystem::remove(capturePath, ignored);
        return false;
    }

    const HANDLE childInput = duplicateStandardHandle(STD_INPUT_HANDLE, GENERIC_READ);
    const HANDLE childOutput = duplicateStandardHandle(STD_OUTPUT_HANDLE, GENERIC_WRITE);
    if (childInput == INVALID_HANDLE_VALUE || childOutput == INVALID_HANDLE_VALUE)
    {
        result.launchError = windowsFailure("duplicate standard handles");
        if (childInput != INVALID_HANDLE_VALUE) ::CloseHandle(childInput);
        if (childOutput != INVALID_HANDLE_VALUE) ::CloseHandle(childOutput);
        ::CloseHandle(capturedError);
        std::error_code ignored;
        std::filesystem::remove(capturePath, ignored);
        return false;
    }

    std::wstring commandLine(::GetCommandLineW());
    commandLine += L' ';
    commandLine += widenAscii(childArgument);
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = childInput;
    startup.hStdOutput = childOutput;
    startup.hStdError = capturedError;
    PROCESS_INFORMATION process{};

    const BOOL created = ::CreateProcessW(
        executable.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
        &startup, &process);
    const DWORD creationError = created ? ERROR_SUCCESS : ::GetLastError();
    ::CloseHandle(childInput);
    ::CloseHandle(childOutput);
    ::CloseHandle(capturedError);

    if (!created)
    {
        ::SetLastError(creationError);
        result.launchError = windowsFailure("CreateProcessW");
        std::error_code ignored;
        std::filesystem::remove(capturePath, ignored);
        return false;
    }

    const DWORD waitResult = ::WaitForSingleObject(process.hProcess, INFINITE);
    if (waitResult != WAIT_OBJECT_0)
    {
        result.exitCode = 1;
        result.launchError = windowsFailure("WaitForSingleObject");
        ::CloseHandle(process.hThread);
        ::CloseHandle(process.hProcess);
        std::error_code ignored;
        std::filesystem::remove(capturePath, ignored);
        // El hijo YA fue creado. Devolver true impide que el llamador repita una compilacion que
        // pudo haber escrito su producto antes de que fallara la espera.
        return true;
    }
    DWORD exitCode = 0;
    if (!::GetExitCodeProcess(process.hProcess, &exitCode))
    {
        result.exitCode = 1;
        result.launchError = windowsFailure("GetExitCodeProcess");
        ::CloseHandle(process.hThread);
        ::CloseHandle(process.hProcess);
        std::error_code ignored;
        std::filesystem::remove(capturePath, ignored);
        return true;
    }
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);

    // Modo texto a proposito: el hijo escribio CRLF y el padre volvera a escribir el diagnostico
    // por su propio stream de texto. Leer en binario conservaria el CR y produciria `\r\r\n` al
    // reenviarlo.
    std::ifstream captured(capturePath);
    if (!captured)
    {
        result.exitCode = exitCode;
        result.launchError = "could not read the child standard-error capture";
        std::error_code ignored;
        std::filesystem::remove(capturePath, ignored);
        return true;
    }
    std::ostringstream contents;
    contents << captured.rdbuf();
    result.standardError = contents.str();
    result.exitCode = exitCode;
    captured.close();

    std::error_code ignored;
    std::filesystem::remove(capturePath, ignored);
    return true;
#else
    (void)childArgument;
    result.launchError = "process-boundary LLVM tracing is only required on Windows";
    return false;
#endif
}
}
