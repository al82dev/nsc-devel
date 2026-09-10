#include "nativesharp/Linker/LldLinker.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace nativesharp::linker
{
namespace
{
std::string readText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return {};
    }
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

std::string trimTrailingWhitespace(std::string text)
{
    while (!text.empty())
    {
        const char character = text.back();
        if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
        {
            break;
        }
        text.pop_back();
    }
    return text;
}

std::string getEnvironmentVariable(const char* name)
{
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;

    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
    {
        return {};
    }

    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
#endif
}

// NSL-ACT-001. Cuando NSC_KEEP_LINK_TEMPS esta puesta, los temporales del ENLACE no se borran.
//
// POR QUE EXISTE, y no es comodidad de quien depura este archivo. `nsc` escribe un fichero de
// respuesta, se lo pasa al enlazador, lee su salida y BORRA los dos en el mismo paso. Es correcto
// para una compilacion normal --nadie quiere basura en %TEMP%-- y deja un agujero concreto:
// cuando el enlace falla, la invocacion exacta que fallo ya no existe. Se tiene el mensaje de
// error y no el comando que lo produjo, asi que el fallo no es reproducible del todo.
//
// Quien desarrolla NSLink lo pidio por escrito (nsc_actions/001) despues de que el handover
// dijera que no se podia dar. Se podia: solo habia que no borrarlo.
//
// APAGADA POR OMISION. Sin la variable, el comportamiento es byte por byte el de siempre.
bool keepLinkTemporaries()
{
    return !getEnvironmentVariable("NSC_KEEP_LINK_TEMPS").empty();
}

bool containsQuote(const std::filesystem::path& path)
{
    return path.native().find(static_cast<std::filesystem::path::value_type>('"')) !=
           std::filesystem::path::string_type::npos;
}

std::string utf8Path(const std::filesystem::path& path)
{
    const std::u8string value = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

std::string responsePathArgument(std::string_view prefix, const std::filesystem::path& path)
{
    return std::string(prefix) + "\"" + utf8Path(path) + "\"";
}

std::string responseFileArgument(const std::filesystem::path& path)
{
    return "\"" + utf8Path(path) + "\"";
}

#ifndef _WIN32
std::string shellQuote(const std::filesystem::path& path)
{
    std::string result = "'";
    for (const char character : path.string())
    {
        if (character == '\'')
        {
            result += "'\"'\"'";
        }
        else
        {
            result.push_back(character);
        }
    }
    result.push_back('\'');
    return result;
}
#endif

std::vector<std::filesystem::path> pathCandidates(std::string_view fileName)
{
    std::vector<std::filesystem::path> result;
    const std::string pathText = getEnvironmentVariable("PATH");
    if (pathText.empty())
    {
        return result;
    }

#ifdef _WIN32
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif

    std::size_t begin = 0;
    while (begin <= pathText.size())
    {
        const std::size_t end = pathText.find(separator, begin);
        const std::string component = pathText.substr(
            begin,
            end == std::string::npos ? std::string::npos : end - begin);
        std::filesystem::path directory = component.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path(component);
        result.push_back(directory / std::string(fileName));
        if (end == std::string::npos)
        {
            break;
        }
        begin = end + 1;
    }
    return result;
}

std::filesystem::path findLldLink()
{
    const std::string overridePath = getEnvironmentVariable("NSC_LLD_LINK");
    if (!overridePath.empty())
    {
        const std::filesystem::path candidate(overridePath);
        if (std::filesystem::is_regular_file(candidate))
        {
            return candidate;
        }
        return {};
    }

    for (const std::string_view name : {std::string_view("lld-link"), std::string_view("lld-link.exe")})
    {
        for (const std::filesystem::path& candidate : pathCandidates(name))
        {
            std::error_code error;
            if (std::filesystem::is_regular_file(candidate, error) && !error)
            {
                return candidate;
            }
        }
    }
    return {};
}

bool isLldLinkExecutable(const std::filesystem::path& executable)
{
    std::string name = executable.filename().string();
    std::ranges::transform(name, name.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return name == "lld-link" || name == "lld-link.exe";
}

std::filesystem::path findLibrarian(const std::filesystem::path& linker)
{
    const std::string overridePath = getEnvironmentVariable("NSC_LLVM_LIB");
    if (!overridePath.empty())
    {
        return std::filesystem::path(overridePath);
    }

    const std::filesystem::path sibling = linker.parent_path() /
#ifdef _WIN32
        "llvm-lib.exe";
#else
        "llvm-lib";
#endif
    std::error_code filesystemError;
    if (std::filesystem::is_regular_file(sibling, filesystemError) && !filesystemError)
    {
        return sibling;
    }

    for (const std::string_view name : {std::string_view("llvm-lib"), std::string_view("llvm-lib.exe")})
    {
        for (const std::filesystem::path& candidate : pathCandidates(name))
        {
            filesystemError.clear();
            if (std::filesystem::is_regular_file(candidate, filesystemError) && !filesystemError)
            {
                return candidate;
            }
        }
    }

    // lld-link implements Microsoft's /lib mode. Alternative PE linkers are not assumed to be
    // librarians: treating an archive as an object was the NSLink gate failure fixed in 0.9.
    return isLldLinkExecutable(linker) ? linker : std::filesystem::path{};
}

bool writeResponseFile(
    const std::filesystem::path& path,
    const std::vector<std::string>& arguments,
    std::string& error)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "could not create lld-link response file: " + utf8Path(path);
        return false;
    }
    for (const std::string& argument : arguments)
    {
        stream << argument << '\n';
    }
    if (!stream)
    {
        error = "failed while writing lld-link response file: " + utf8Path(path);
        return false;
    }
    return true;
}

bool validateObjectFiles(
    std::span<const std::filesystem::path> objectFiles,
    std::string_view productName,
    std::string& error)
{
    if (objectFiles.empty())
    {
        error = std::string(productName) + " requires at least one COFF object file";
        return false;
    }
    for (const std::filesystem::path& path : objectFiles)
    {
        if (!std::filesystem::is_regular_file(path) || containsQuote(path))
        {
            error = std::string(productName) +
                " received a missing object file or unsupported object path";
            return false;
        }
    }
    return true;
}

bool validateLibraries(
    std::span<const std::filesystem::path> libraries,
    std::string_view productName,
    std::string& error)
{
    for (const std::filesystem::path& path : libraries)
    {
        if (!std::filesystem::is_regular_file(path) || containsQuote(path))
        {
            error = std::string(productName) +
                " received a missing import library or unsupported library path";
            return false;
        }
    }
    return true;
}
}

LldLinker::LldLinker(std::filesystem::path executablePath) noexcept
    : executablePath_(std::move(executablePath))
{
}

std::unique_ptr<LldLinker> LldLinker::discover(std::string& error)
{
    error.clear();
    const std::filesystem::path executable = findLldLink();
    if (executable.empty())
    {
        error = "lld-link was not found in PATH; set NSC_LLD_LINK to its executable path";
        return nullptr;
    }
    if (containsQuote(executable))
    {
        error = "lld-link executable path contains an unsupported quote character";
        return nullptr;
    }
    return std::unique_ptr<LldLinker>(new LldLinker(executable));
}

const std::filesystem::path& LldLinker::executablePath() const noexcept
{
    return executablePath_;
}

std::filesystem::path LldLinker::librarianExecutablePath() const
{
    return findLibrarian(executablePath_);
}

bool LldLinker::runResponseFile(
    const std::filesystem::path& responsePath,
    const std::filesystem::path& logPath,
    std::string& error,
    bool libraryMode) const
{
    error.clear();
    if (containsQuote(responsePath) || containsQuote(logPath))
    {
        error = "lld-link temporary paths contain an unsupported quote character";
        return false;
    }

#ifdef _WIN32
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE logHandle = CreateFileW(
        logPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        &securityAttributes,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (logHandle == INVALID_HANDLE_VALUE)
    {
        error = "could not create lld-link log file: " +
            std::system_category().message(static_cast<int>(GetLastError()));
        return false;
    }

    HANDLE inputHandle = CreateFileW(
        L"NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &securityAttributes,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (inputHandle == INVALID_HANDLE_VALUE)
    {
        const DWORD lastError = GetLastError();
        CloseHandle(logHandle);
        error = "could not open NUL for lld-link stdin: " +
            std::system_category().message(static_cast<int>(lastError));
        return false;
    }

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = inputHandle;
    startupInfo.hStdOutput = logHandle;
    startupInfo.hStdError = logHandle;

    PROCESS_INFORMATION processInfo{};
    std::wstring commandLine = L"\"" + executablePath_.wstring() + L"\"";
    if (libraryMode)
    {
        commandLine += L" /lib";
    }
    commandLine += L" @\"" + responsePath.wstring() + L"\"";
    std::vector<wchar_t> mutableCommandLine(
        commandLine.begin(),
        commandLine.end());
    mutableCommandLine.push_back(L'\0');

    const BOOL created = CreateProcessW(
        executablePath_.c_str(),
        mutableCommandLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo);

    const DWORD createError = created ? ERROR_SUCCESS : GetLastError();
    CloseHandle(inputHandle);
    CloseHandle(logHandle);

    if (!created)
    {
        error = "could not start lld-link: " +
            std::system_category().message(static_cast<int>(createError));
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, INFINITE);
    if (waitResult != WAIT_OBJECT_0)
    {
        const DWORD waitError = GetLastError();
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        error = "waiting for lld-link failed: " +
            std::system_category().message(static_cast<int>(waitError));
        return false;
    }

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(processInfo.hProcess, &exitCode))
    {
        const DWORD exitError = GetLastError();
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        error = "could not obtain lld-link exit code: " +
            std::system_category().message(static_cast<int>(exitError));
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);

    const std::string diagnostic = trimTrailingWhitespace(readText(logPath));
    if (exitCode != 0)
    {
        std::ostringstream message;
        message << "lld-link failed with process status " << exitCode;
        if (!diagnostic.empty())
        {
            message << ":\n" << diagnostic;
        }
        error = message.str();
        return false;
    }
    return true;
#else
    const std::string command =
        shellQuote(executablePath_) + (libraryMode ? " /lib @" : " @") +
        shellQuote(responsePath) + " >" + shellQuote(logPath) + " 2>&1";
    const int status = std::system(command.c_str());
    const std::string diagnostic = trimTrailingWhitespace(readText(logPath));
    if (status != 0)
    {
        std::ostringstream message;
        message << "lld-link failed with process status " << status;
        if (!diagnostic.empty())
        {
            message << ":\n" << diagnostic;
        }
        error = message.str();
        return false;
    }
    return true;
#endif
}

bool LldLinker::createImportLibrary(
    std::string_view dllName,
    std::span<const std::string_view> symbols,
    const std::filesystem::path& outputPath,
    std::string& error) const
{
    error.clear();
    if (dllName.empty() || symbols.empty())
    {
        error = "import-library generation requires a DLL name and at least one symbol";
        return false;
    }
    if (outputPath.empty() || containsQuote(outputPath))
    {
        error = "import-library output path is empty or contains an unsupported quote character";
        return false;
    }

    std::filesystem::path definitionPath = outputPath;
    std::filesystem::path responsePath = outputPath;
    std::filesystem::path logPath = outputPath;
    std::filesystem::path dummyPath = outputPath;
    definitionPath += ".def";
    responsePath += ".rsp";
    logPath += ".log";
    dummyPath += ".dummy.dll";

    std::ofstream definition(definitionPath, std::ios::binary | std::ios::trunc);
    if (!definition)
    {
        error = "could not create import-library module-definition file";
        return false;
    }
    definition << "LIBRARY " << dllName << "\nEXPORTS\n";
    for (const std::string_view symbol : symbols)
    {
        if (symbol.empty() || symbol.find_first_of("\r\n\"") != std::string_view::npos)
        {
            error = "import-library symbol contains unsupported characters";
            return false;
        }
        definition << symbol << '\n';
    }
    definition.close();
    if (!definition)
    {
        error = "failed while writing import-library module-definition file";
        return false;
    }

    const std::vector<std::string> arguments = {
        "/dll",
        "/noentry",
        "/machine:x64",
        responsePathArgument("/def:", definitionPath),
        responsePathArgument("/implib:", outputPath),
        responsePathArgument("/out:", dummyPath)};
    if (!writeResponseFile(responsePath, arguments, error))
    {
        return false;
    }

    const bool success = runResponseFile(responsePath, logPath, error);
    std::error_code cleanupError;
    std::filesystem::remove(definitionPath, cleanupError);
    if (!keepLinkTemporaries())
    {
        std::filesystem::remove(responsePath, cleanupError);
        std::filesystem::remove(logPath, cleanupError);
    }
    std::filesystem::remove(dummyPath, cleanupError);
    if (!success)
    {
        std::filesystem::remove(outputPath, cleanupError);
        return false;
    }
    if (!std::filesystem::is_regular_file(outputPath))
    {
        error = "lld-link reported success but did not create the requested import library";
        return false;
    }
    return true;
}

bool LldLinker::createStaticLibrary(
    std::span<const std::filesystem::path> objectFiles,
    const std::filesystem::path& outputPath,
    std::string& error) const
{
    error.clear();
    if (!validateObjectFiles(objectFiles, "static-library generation", error))
    {
        return false;
    }
    if (outputPath.empty() || containsQuote(outputPath))
    {
        error = "static-library output path is empty or contains an unsupported quote character";
        return false;
    }

    const std::filesystem::path librarianPath = findLibrarian(executablePath_);
    if (librarianPath.empty())
    {
        error = "static-library generation requires llvm-lib when NSC_LLD_LINK selects a "
            "linker without /lib mode; set NSC_LLVM_LIB to the librarian executable";
        return false;
    }
    if (!std::filesystem::is_regular_file(librarianPath) || containsQuote(librarianPath))
    {
        error = "NSC_LLVM_LIB does not name a usable librarian executable: " +
            librarianPath.string();
        return false;
    }

    std::filesystem::path responsePath = outputPath;
    std::filesystem::path logPath = outputPath;
    responsePath += ".lib.rsp";
    logPath += ".lib.log";
    std::vector<std::string> arguments = {
        "/machine:x64",
        responsePathArgument("/out:", outputPath)};
    for (const std::filesystem::path& path : objectFiles)
    {
        arguments.push_back(responseFileArgument(path));
    }
    if (!writeResponseFile(responsePath, arguments, error))
    {
        return false;
    }

    std::error_code cleanupError;
    std::filesystem::remove(outputPath, cleanupError);
    LldLinker librarian(librarianPath);
    const bool success = librarian.runResponseFile(
        responsePath,
        logPath,
        error,
        isLldLinkExecutable(librarianPath));
    if (!keepLinkTemporaries())
    {
        std::filesystem::remove(responsePath, cleanupError);
        std::filesystem::remove(logPath, cleanupError);
    }
    if (!success)
    {
        std::filesystem::remove(outputPath, cleanupError);
        return false;
    }
    if (!std::filesystem::is_regular_file(outputPath))
    {
        error = "lld-link reported success but did not create the requested static library";
        return false;
    }
    return true;
}

bool LldLinker::linkDynamicLibrary(
    std::span<const std::filesystem::path> objectFiles,
    std::span<const std::filesystem::path> importLibraries,
    std::span<const std::string> exportSymbols,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    std::string& error) const
{
    error.clear();
    if (!validateObjectFiles(objectFiles, "DLL linking", error) ||
        !validateLibraries(importLibraries, "DLL linking", error))
    {
        return false;
    }
    if (outputPath.empty() || importLibraryPath.empty() ||
        containsQuote(outputPath) || containsQuote(importLibraryPath))
    {
        error = "DLL or import-library output path is empty or contains an unsupported quote character";
        return false;
    }
    for (const std::string& symbol : exportSymbols)
    {
        if (symbol.empty() || symbol.find_first_of("\r\n\"") != std::string::npos)
        {
            error = "DLL export symbol contains unsupported characters";
            return false;
        }
    }

    std::filesystem::path responsePath = outputPath;
    std::filesystem::path logPath = outputPath;
    responsePath += ".dll.rsp";
    logPath += ".dll.log";
    std::vector<std::string> arguments = {
        "/dll",
        "/noentry",
        "/nodefaultlib",
        "/machine:x64",
        "/dynamicbase",
        "/nxcompat",
        "/highentropyva",
        responsePathArgument("/out:", outputPath)};
    if (!exportSymbols.empty())
        arguments.push_back(responsePathArgument("/implib:", importLibraryPath));
    for (const std::string& symbol : exportSymbols)
    {
        arguments.push_back("/export:" + symbol);
    }
    for (const std::filesystem::path& path : objectFiles)
    {
        arguments.push_back(responseFileArgument(path));
    }
    for (const std::filesystem::path& path : importLibraries)
    {
        arguments.push_back(responseFileArgument(path));
    }
    if (!writeResponseFile(responsePath, arguments, error))
    {
        return false;
    }

    std::error_code cleanupError;
    std::filesystem::remove(outputPath, cleanupError);
    std::filesystem::remove(importLibraryPath, cleanupError);
    const bool success = runResponseFile(responsePath, logPath, error);
    if (!keepLinkTemporaries())
    {
        std::filesystem::remove(responsePath, cleanupError);
        std::filesystem::remove(logPath, cleanupError);
    }
    if (!success)
    {
        std::filesystem::remove(outputPath, cleanupError);
        std::filesystem::remove(importLibraryPath, cleanupError);
        return false;
    }
    if (!std::filesystem::is_regular_file(outputPath))
    {
        error = "lld-link reported success but did not create the requested DLL";
        std::filesystem::remove(outputPath, cleanupError);
        std::filesystem::remove(importLibraryPath, cleanupError);
        return false;
    }
    if (exportSymbols.empty())
    {
        // With no exported PE symbols there is no import library for lld-link to
        // synthesize. LibraryReference still needs a validated COFF companion for
        // the NSMETA-only product, so archive one producer object without inventing
        // an import thunk or a callable export.
        const std::span<const std::filesystem::path> companionObject = objectFiles.first(1);
        if (!createStaticLibrary(companionObject, importLibraryPath, error))
        {
            std::filesystem::remove(outputPath, cleanupError);
            return false;
        }
    }
    else if (!std::filesystem::is_regular_file(importLibraryPath))
    {
        error = "lld-link reported success but did not create the requested import library";
        std::filesystem::remove(outputPath, cleanupError);
        return false;
    }
    return true;
}

bool LldLinker::linkExecutable(
    std::span<const std::filesystem::path> objectFiles,
    std::span<const std::filesystem::path> importLibraries,
    const std::filesystem::path& outputPath,
    WindowsExecutableSubsystem subsystem,
    std::string& error) const
{
    return linkExecutable(objectFiles, importLibraries, outputPath, subsystem, {}, error);
}

bool LldLinker::linkExecutable(
    std::span<const std::filesystem::path> objectFiles,
    std::span<const std::filesystem::path> importLibraries,
    const std::filesystem::path& outputPath,
    WindowsExecutableSubsystem subsystem,
    const std::filesystem::path& pdbPath,
    std::string& error) const
{
    error.clear();
    if (!validateObjectFiles(objectFiles, "PE linking", error) ||
        !validateLibraries(importLibraries, "PE linking", error))
    {
        return false;
    }
    if (outputPath.empty() || containsQuote(outputPath))
    {
        error = "PE output path is empty or contains an unsupported quote character";
        return false;
    }
    if (!pdbPath.empty() && containsQuote(pdbPath))
    {
        error = "PDB output path contains an unsupported quote character";
        return false;
    }

    std::filesystem::path responsePath = outputPath;
    std::filesystem::path logPath = outputPath;
    responsePath += ".link.rsp";
    logPath += ".link.log";
    std::vector<std::string> arguments = {
        "/nodefaultlib",
        "/entry:__nativesharp_start",
        subsystem == WindowsExecutableSubsystem::Windows
            ? "/subsystem:windows"
            : "/subsystem:console",
        "/machine:x64",
        "/dynamicbase",
        "/nxcompat",
        "/highentropyva",
        responsePathArgument("/out:", outputPath)};
    if (!pdbPath.empty())
    {
        arguments.push_back("/debug:full");
        arguments.push_back(responsePathArgument("/pdb:", pdbPath));
        arguments.push_back("/pdbaltpath:%_PDB%");
    }
    for (const std::filesystem::path& path : objectFiles)
    {
        arguments.push_back(responseFileArgument(path));
    }
    for (const std::filesystem::path& path : importLibraries)
    {
        arguments.push_back(responseFileArgument(path));
    }
    if (!writeResponseFile(responsePath, arguments, error))
    {
        return false;
    }

    std::error_code cleanupError;
    std::filesystem::remove(outputPath, cleanupError);
    if (!pdbPath.empty()) std::filesystem::remove(pdbPath, cleanupError);
    const bool success = runResponseFile(responsePath, logPath, error);
    if (!keepLinkTemporaries())
    {
        std::filesystem::remove(responsePath, cleanupError);
        std::filesystem::remove(logPath, cleanupError);
    }
    if (!success)
    {
        std::filesystem::remove(outputPath, cleanupError);
        if (!pdbPath.empty()) std::filesystem::remove(pdbPath, cleanupError);
        return false;
    }
    if (!std::filesystem::is_regular_file(outputPath))
    {
        error = "lld-link reported success but did not create the requested PE executable";
        return false;
    }
    if (!pdbPath.empty() && !std::filesystem::is_regular_file(pdbPath))
    {
        std::filesystem::remove(outputPath, cleanupError);
        error = "lld-link reported success but did not create the requested PDB";
        return false;
    }
    return true;
}
}
