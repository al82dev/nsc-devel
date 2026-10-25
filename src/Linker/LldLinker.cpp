#include "nativesharp/Linker/LldLinker.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <span>
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
    stream.seekg(0, std::ios::end);
    const std::streamoff fileSize = stream.tellg();
    if (fileSize < 0 || static_cast<std::uintmax_t>(fileSize) >
            std::numeric_limits<std::size_t>::max())
    {
        return {};
    }
    stream.seekg(0, std::ios::beg);
    if (!stream) return {};

    std::string text(static_cast<std::size_t>(fileSize), '\0');
    constexpr std::size_t ReadBlockSize = 64U * 1024U;
    std::size_t offset = 0;
    while (offset < text.size())
    {
        const std::size_t blockSize = std::min(ReadBlockSize, text.size() - offset);
        stream.read(text.data() + offset, static_cast<std::streamsize>(blockSize));
        const std::size_t readSize = static_cast<std::size_t>(stream.gcount());
        offset += readSize;
        if (readSize != blockSize)
        {
            text.resize(offset);
            break;
        }
    }
    return text;
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

std::string executableName(std::filesystem::path executable)
{
    std::string name = executable.filename().string();
    std::ranges::transform(name, name.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return name;
}

bool isNsLinkExecutable(const std::filesystem::path& executable)
{
    const std::string name = executableName(executable);
    return name == "nslink" || name == "nslink.exe";
}

std::string linkerProductName(const std::filesystem::path& executable)
{
    const std::string name = executableName(executable);
    if (name == "llvm-lib" || name == "llvm-lib.exe") return "llvm-lib";
    return "NSLink";
}

std::filesystem::path currentExecutablePath()
{
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length != 0 && length < buffer.size())
    {
        buffer.resize(length);
        return std::filesystem::path(buffer);
    }
#endif
    return {};
}

std::filesystem::path resolveConfiguredNsLink(
    std::string_view configured,
    std::string& error)
{
    const std::filesystem::path candidate(configured);
    if (!isNsLinkExecutable(candidate))
    {
        error = "NSC_LINKER must name nslink.exe: '" + std::string(configured) + "'";
        return {};
    }
    if (candidate.has_parent_path())
    {
        std::error_code filesystemError;
        if (std::filesystem::is_regular_file(candidate, filesystemError) && !filesystemError)
            return candidate;
        error = "NSC_LINKER executable does not exist: '" + utf8Path(candidate) + "'";
        return {};
    }

    std::vector<std::string> names{candidate.string()};
#ifdef _WIN32
    if (!candidate.has_extension()) names.push_back(candidate.string() + ".exe");
#endif
    for (const std::string& name : names)
    {
        for (const std::filesystem::path& pathCandidate : pathCandidates(name))
        {
            std::error_code filesystemError;
            if (std::filesystem::is_regular_file(pathCandidate, filesystemError) && !filesystemError)
                return pathCandidate;
        }
    }
    error = "NSC_LINKER executable '" + std::string(configured) + "' was not found in PATH";
    return {};
}

std::filesystem::path findNsLink(std::string& error)
{
    const std::string overridePath = getEnvironmentVariable("NSC_LINKER");
    if (!overridePath.empty())
        return resolveConfiguredNsLink(overridePath, error);

    const std::filesystem::path compiler = currentExecutablePath();
    if (!compiler.empty())
    {
        const std::filesystem::path sibling = compiler.parent_path() / "nslink.exe";
        std::error_code error;
        if (std::filesystem::is_regular_file(sibling, error) && !error)
        {
            return sibling;
        }
    }

    for (const std::string_view name : {std::string_view("nslink"), std::string_view("nslink.exe")})
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

    // llvm-lib is selected independently. Alternative PE linkers are not assumed to be
    // librarians: treating an archive as an object was the NSLink gate failure fixed in 0.9.
    return {};
}

bool writeResponseFile(
    const std::filesystem::path& path,
    const std::vector<std::string>& arguments,
    std::string& error)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "could not create linker response file: " + utf8Path(path);
        return false;
    }
    for (const std::string& argument : arguments)
    {
        stream << argument << '\n';
    }
    if (!stream)
    {
        error = "failed while writing linker response file: " + utf8Path(path);
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

#ifdef _WIN32
bool openLinkerHandles(
    const std::filesystem::path& executable,
    const std::filesystem::path& logPath,
    SECURITY_ATTRIBUTES& securityAttributes,
    HANDLE& inputHandle,
    HANDLE& logHandle,
    std::string& error)
{
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;
    logHandle = CreateFileW(
        logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &securityAttributes,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logHandle == INVALID_HANDLE_VALUE)
    {
        error = "could not create " + linkerProductName(executable) + " log file: " +
            std::system_category().message(static_cast<int>(GetLastError()));
        return false;
    }
    inputHandle = CreateFileW(
        L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &securityAttributes,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (inputHandle != INVALID_HANDLE_VALUE) return true;
    const DWORD lastError = GetLastError();
    CloseHandle(logHandle);
    error = "could not open NUL for " + linkerProductName(executable) + " stdin: " +
        std::system_category().message(static_cast<int>(lastError));
    return false;
}

bool startLinkerProcess(
    const std::filesystem::path& executable,
    const std::filesystem::path& responsePath,
    const std::filesystem::path& workingDirectory,
    bool libraryMode,
    HANDLE inputHandle,
    HANDLE logHandle,
    PROCESS_INFORMATION& processInfo,
    std::string& error)
{
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = inputHandle;
    startupInfo.hStdOutput = logHandle;
    startupInfo.hStdError = logHandle;
    std::wstring commandLine = L"\"" + executable.wstring() + L"\"";
    if (libraryMode) commandLine += L" /lib";
    commandLine += L" @\"" + responsePath.wstring() + L"\"";
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');
    const BOOL created = CreateProcessW(
        executable.c_str(), mutableCommandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
        &startupInfo, &processInfo);
    const DWORD createError = created ? ERROR_SUCCESS : GetLastError();
    CloseHandle(inputHandle);
    CloseHandle(logHandle);
    if (created) return true;
    error = "could not start " + linkerProductName(executable) + ": " +
        std::system_category().message(static_cast<int>(createError));
    return false;
}

bool waitForLinkerProcess(
    const std::filesystem::path& executable,
    const std::filesystem::path& logPath,
    PROCESS_INFORMATION& processInfo,
    std::string& error)
{
    const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, INFINITE);
    if (waitResult != WAIT_OBJECT_0)
    {
        const DWORD waitError = GetLastError();
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        error = "waiting for " + linkerProductName(executable) + " failed: " +
            std::system_category().message(static_cast<int>(waitError));
        return false;
    }
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(processInfo.hProcess, &exitCode))
    {
        const DWORD exitError = GetLastError();
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        error = "could not obtain " + linkerProductName(executable) + " exit code: " +
            std::system_category().message(static_cast<int>(exitError));
        return false;
    }
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    const std::string diagnostic = trimTrailingWhitespace(readText(logPath));
    if (exitCode == 0) return true;
    std::ostringstream message;
    message << linkerProductName(executable) << " failed with process status " << exitCode;
    if (!diagnostic.empty()) message << ":\n" << diagnostic;
    error = message.str();
    return false;
}
#else
bool runPortableLinkerProcess(
    const std::filesystem::path& executable,
    const std::filesystem::path& responsePath,
    const std::filesystem::path& logPath,
    bool libraryMode,
    std::string& error)
{
    const std::string command =
        shellQuote(executable) + (libraryMode ? " /lib @" : " @") +
        shellQuote(responsePath) + " >" + shellQuote(logPath) + " 2>&1";
    const int status = std::system(command.c_str());
    const std::string diagnostic = trimTrailingWhitespace(readText(logPath));
    if (status == 0) return true;
    std::ostringstream message;
    message << linkerProductName(executable) << " failed with process status " << status;
    if (!diagnostic.empty()) message << ":\n" << diagnostic;
    error = message.str();
    return false;
}
#endif
}

LldLinker::LldLinker(std::filesystem::path executablePath) noexcept
    : executablePath_(std::move(executablePath))
{
}

std::unique_ptr<LldLinker> LldLinker::discover(std::string& error)
{
    error.clear();
    const std::filesystem::path executable = findNsLink(error);
    if (executable.empty())
    {
        if (error.empty())
            error = "NSLink was not found beside nsc.exe or in PATH; set NSC_LINKER to nslink.exe";
        return nullptr;
    }
    if (containsQuote(executable))
    {
        error = "NSLink executable path contains an unsupported quote character";
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
    bool libraryMode,
    const std::filesystem::path& workingDirectory) const
{
    error.clear();
    if (containsQuote(responsePath) || containsQuote(logPath))
    {
        error = "linker temporary paths contain an unsupported quote character";
        return false;
    }

#ifdef _WIN32
    SECURITY_ATTRIBUTES securityAttributes{};
    HANDLE inputHandle = INVALID_HANDLE_VALUE;
    HANDLE logHandle = INVALID_HANDLE_VALUE;
    if (!openLinkerHandles(
            executablePath_, logPath, securityAttributes, inputHandle, logHandle, error))
    {
        return false;
    }
    PROCESS_INFORMATION processInfo{};
    if (!startLinkerProcess(
            executablePath_, responsePath, workingDirectory, libraryMode,
            inputHandle, logHandle, processInfo, error))
    {
        return false;
    }
    return waitForLinkerProcess(executablePath_, logPath, processInfo, error);
#else
    return runPortableLinkerProcess(executablePath_, responsePath, logPath, libraryMode, error);
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
        error = linkerProductName(executablePath_) +
            " reported success but did not create the requested import library";
        return false;
    }
    return true;
}

namespace
{
bool validateStaticLibraryRequest(
    std::span<const std::filesystem::path> objectFiles,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& linkerPath,
    std::filesystem::path& librarianPath,
    std::string& error)
{
    if (!validateObjectFiles(objectFiles, "static-library generation", error)) return false;
    if (outputPath.empty() || containsQuote(outputPath))
    {
        error = "static-library output path is empty or contains an unsupported quote character";
        return false;
    }
    librarianPath = findLibrarian(linkerPath);
    if (librarianPath.empty())
    {
        error = "static-library generation requires llvm-lib when NSC_LINKER selects a "
            "linker without /lib mode; set NSC_LLVM_LIB to the librarian executable";
        return false;
    }
    if (std::filesystem::is_regular_file(librarianPath) && !containsQuote(librarianPath))
        return true;
    error = "NSC_LLVM_LIB does not name a usable librarian executable: " +
        librarianPath.string();
    return false;
}

/// EN: Returns the directory shared by every object file, or an empty path when the objects do not
///     all live in one directory.
///     Parameters: `objectFiles`, the archive inputs in archive order; relative paths are resolved
///     against the current directory. Precondition: none; an empty span yields an empty path.
///     Returns: the common absolute parent directory, or an empty path. No side effects; reads no
///     files; thread safe. Complexity: O(n) path conversions for n objects.
///     Algorithm: compares each absolute parent with the first one, spelled exactly as
///     `appendStaticLibraryObjects` spells it, so both helpers agree on which objects are shortened.
/// ES: Devuelve el directorio que comparten todos los objetos, o una ruta vacia cuando no viven en
///     un solo directorio.
///     Parametros: `objectFiles`, las entradas del archivo en su orden; las rutas relativas se
///     resuelven contra el directorio actual. Precondicion: ninguna; un span vacio da ruta vacia.
///     Retorno: el directorio padre absoluto comun, o una ruta vacia. Sin efectos laterales; no lee
///     archivos; seguro entre hilos. Complejidad: O(n) conversiones de ruta para n objetos.
///     Algoritmo: compara el padre absoluto de cada objeto con el del primero, escrito igual que en
///     `appendStaticLibraryObjects`, para que ambos helpers coincidan en que objetos se acortan.
#ifdef _WIN32
std::filesystem::path commonObjectDirectory(
    std::span<const std::filesystem::path> objectFiles)
{
    std::filesystem::path common;
    for (const std::filesystem::path& path : objectFiles)
    {
        std::error_code absoluteError;
        const std::filesystem::path parent =
            std::filesystem::absolute(path, absoluteError).parent_path();
        if (absoluteError || parent.empty())
        {
            return {};
        }
        if (common.empty())
        {
            common = parent;
        }
        else if (parent != common)
        {
            return {};
        }
    }
    return common;
}
#endif

/// EN: Chooses the librarian working directory, which decides the archive member names.
///     Parameters: `outputPath`, the archive to create; `objectFiles`, its inputs.
///     Returns: the objects' common directory when the output path is absolute and every object
///     shares one directory (members become stable basenames such as `program-0.obj`); otherwise
///     the output's parent directory as before (only objects inside it are shortened); an empty
///     path on non-Windows hosts or for a relative output. No side effects; thread safe.
///     Why: partitioned code generation writes its objects to a fresh temporary directory whose
///     name embeds a clock stamp. Passing those absolute paths made every archive differ byte by
///     byte between two builds of identical input (HECHO, incremental audit 2026-10-22).
/// ES: Elige el directorio de trabajo del bibliotecario, que decide los nombres de los miembros.
///     Parametros: `outputPath`, el archivo a crear; `objectFiles`, sus entradas.
///     Retorno: el directorio comun de los objetos cuando la salida es absoluta y todos comparten
///     un directorio (los miembros quedan con nombres base estables como `program-0.obj`); en otro
///     caso el padre de la salida como antes (solo se acortan los objetos que viven en el); ruta
///     vacia fuera de Windows o con salida relativa. Sin efectos laterales; seguro entre hilos.
///     Motivo: la generacion particionada escribe sus objetos en un directorio temporal nuevo cuyo
///     nombre lleva una marca de reloj. Pasar esas rutas absolutas hacia que cada archivo difiriera
///     byte a byte entre dos builds de la misma entrada (HECHO, auditoria incremental 2026-10-22).
std::filesystem::path staticLibraryWorkingDirectory(
    const std::filesystem::path& outputPath,
    std::span<const std::filesystem::path> objectFiles)
{
#ifdef _WIN32
    // llvm-lib preserves the spelling of an input path as the COFF archive member name.  Use
    // basenames from a common working directory so deterministic archives do not leak build-tree
    // paths into downstream PDB module records.
    if (!outputPath.is_absolute())
    {
        return {};
    }
    const std::filesystem::path objectDirectory = commonObjectDirectory(objectFiles);
    return objectDirectory.empty() ? outputPath.parent_path() : objectDirectory;
#else
    (void)objectFiles;
    return {};
#endif
}

void appendStaticLibraryObjects(
    std::vector<std::string>& arguments,
    std::span<const std::filesystem::path> objectFiles,
    const std::filesystem::path& workingDirectory)
{
    for (const std::filesystem::path& path : objectFiles)
    {
#ifdef _WIN32
        std::error_code objectError;
        const std::filesystem::path absoluteObject = std::filesystem::absolute(path, objectError);
        if (!objectError && !workingDirectory.empty() &&
            absoluteObject.parent_path() == workingDirectory)
        {
            arguments.push_back(responseFileArgument(absoluteObject.filename()));
        }
        else
#endif
        {
            arguments.push_back(responseFileArgument(path));
        }
    }
}

void cleanupLinkerTemporaries(
    const std::filesystem::path& responsePath,
    const std::filesystem::path& logPath,
    std::error_code& cleanupError)
{
    if (keepLinkTemporaries()) return;
    std::filesystem::remove(responsePath, cleanupError);
    std::filesystem::remove(logPath, cleanupError);
}

#ifdef _WIN32
std::string foldedLinkInputName(const std::filesystem::path& name)
{
    std::string folded = utf8Path(name);
    std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });
    return folded;
}
#endif

/// EN: Gives every object handed to NSLink a stable spelling inside a private link directory.
///     Parameters: `outputPath`, the PE product; `objectFiles`, the inputs in link order;
///     `directory` receives `<outputPath>.nslink-inputs`; `objectNames` receives each input's name
///     relative to that directory: its basename, or `<ordinal>-<basename>` when two inputs share
///     one (compared without case, as Windows does). Returns: false with `error` when the
///     directory or one input cannot be prepared. Side effects: replaces the directory and
///     hard-links every object into it, copying when a hard link is impossible (another volume).
///     Complexity: O(n^2) name comparisons for n objects; links have a handful of objects.
///     Why: NSLink spells each input path into the PDB module table and hashes it into the PDB
///     GUID that the RSDS record of the PE repeats. The objects live in temporary directories
///     whose names carry a clock stamp, so two O0 -g builds of identical input produced different
///     PDBs and PEs (HECHO, optimization part 2 item 6, 2026-10-25).
/// ES: Da a cada objeto que recibe NSLink un nombre estable dentro de un directorio de enlace
///     privado.
///     Parametros: `outputPath`, el producto PE; `objectFiles`, las entradas en orden de enlace;
///     `directory` recibe `<outputPath>.nslink-inputs`; `objectNames` recibe el nombre de cada
///     entrada relativo a ese directorio: su nombre base, o `<ordinal>-<nombre base>` cuando dos
///     entradas comparten uno (sin distinguir mayusculas, como Windows). Retorno: false con
///     `error` cuando no se puede preparar el directorio o una entrada. Efectos laterales:
///     reemplaza el directorio y crea un enlace duro de cada objeto dentro de el; copia cuando el
///     enlace duro no es posible (otro volumen). Complejidad: O(n^2) comparaciones de nombre para
///     n objetos; un enlace tiene pocos objetos.
///     Motivo: NSLink escribe la ruta de cada entrada en la tabla de modulos del PDB y la mete en
///     el hash del GUID del PDB, que el registro RSDS del PE repite. Los objetos viven en
///     directorios temporales cuyo nombre lleva una marca de reloj, asi que dos builds O0 -g de la
///     misma entrada producian PDB y PE distintos (HECHO, optimizacion parte 2 punto 6,
///     2026-10-25).
bool prepareStableLinkInputs(
    const std::filesystem::path& outputPath,
    std::span<const std::filesystem::path> objectFiles,
    std::filesystem::path& directory,
    std::vector<std::filesystem::path>& objectNames,
    std::string& error)
{
#ifndef _WIN32
    // The portable launcher has no working directory, so the inputs keep their own paths.
    // El lanzador portable no tiene directorio de trabajo; las entradas conservan sus rutas.
    (void)outputPath;
    (void)error;
    directory.clear();
    objectNames.assign(objectFiles.begin(), objectFiles.end());
    return true;
#else
    directory = outputPath;
    directory += ".nslink-inputs";
    objectNames.clear();
    std::error_code filesystemError;
    std::filesystem::remove_all(directory, filesystemError);
    filesystemError.clear();
    std::filesystem::create_directories(directory, filesystemError);
    if (filesystemError)
    {
        error = "could not create the linker input directory '" + utf8Path(directory) + "': " +
            filesystemError.message();
        return false;
    }
    std::vector<std::string> usedNames;
    for (std::size_t ordinal = 0; ordinal < objectFiles.size(); ++ordinal)
    {
        const std::filesystem::path baseName = objectFiles[ordinal].filename();
        std::filesystem::path name = baseName;
        for (std::size_t attempt = 0;
            std::find(usedNames.begin(), usedNames.end(), foldedLinkInputName(name)) != usedNames.end();
            ++attempt)
        {
            const std::string prefix = std::to_string(ordinal) + "-" +
                (attempt == 0 ? std::string() : std::to_string(attempt) + "-");
            name = std::filesystem::path(prefix);
            name += baseName;
        }
        usedNames.push_back(foldedLinkInputName(name));
        const std::filesystem::path target = directory / name;
        filesystemError.clear();
        std::filesystem::create_hard_link(objectFiles[ordinal], target, filesystemError);
        if (filesystemError)
        {
            filesystemError.clear();
            std::filesystem::copy_file(objectFiles[ordinal], target, filesystemError);
        }
        if (filesystemError)
        {
            error = "could not stage linker input '" + utf8Path(objectFiles[ordinal]) + "': " +
                filesystemError.message();
            return false;
        }
        objectNames.push_back(name);
    }
    return true;
#endif
}

/// EN: Returns how `/pdb:` names the PDB: its file name inside the link directory on Windows,
///     the full path where the portable launcher cannot choose a working directory.
/// ES: Devuelve como nombra `/pdb:` al PDB: su nombre de archivo dentro del directorio de enlace en
///     Windows, la ruta completa donde el lanzador portable no puede elegir directorio de trabajo.
std::filesystem::path linkerPdbSpelling(const std::filesystem::path& pdbPath)
{
#ifdef _WIN32
    return pdbPath.filename();
#else
    return pdbPath;
#endif
}

/// EN: Publishes the PDB that NSLink wrote inside the link directory and removes the directory.
///     Parameters: `directory`, from `prepareStableLinkInputs`; `pdbPath`, the requested PDB or
///     empty; `linked`, whether NSLink succeeded. Returns: `linked`, or false with `error` when
///     the PDB cannot be moved to `pdbPath`. Side effects: renames (or copies) the PDB and removes
///     the directory unless NSC_KEEP_LINK_TEMPS is set.
///     Why: NSLink records its `/pdb:` spelling inside the PDB, so it receives the bare file name
///     relative to the link directory instead of the transactional staging path.
/// ES: Publica el PDB que NSLink escribio dentro del directorio de enlace y borra el directorio.
///     Parametros: `directory`, de `prepareStableLinkInputs`; `pdbPath`, el PDB pedido o vacio;
///     `linked`, si NSLink tuvo exito. Retorno: `linked`, o false con `error` cuando el PDB no se
///     puede mover a `pdbPath`. Efectos laterales: renombra (o copia) el PDB y borra el directorio
///     salvo con NSC_KEEP_LINK_TEMPS.
///     Motivo: NSLink guarda dentro del PDB lo que recibe en `/pdb:`, asi que recibe el nombre
///     simple relativo al directorio de enlace en lugar de la ruta de staging transaccional.
bool finishStableLinkInputs(
    const std::filesystem::path& directory,
    const std::filesystem::path& pdbPath,
    bool linked,
    std::string& error)
{
    if (directory.empty()) return linked;
    std::error_code filesystemError;
    if (linked && !pdbPath.empty())
    {
        const std::filesystem::path written = directory / pdbPath.filename();
        std::filesystem::rename(written, pdbPath, filesystemError);
        if (filesystemError)
        {
            filesystemError.clear();
            std::filesystem::copy_file(written, pdbPath,
                std::filesystem::copy_options::overwrite_existing, filesystemError);
        }
        if (filesystemError)
        {
            error = "could not publish the linked PDB '" + utf8Path(pdbPath) + "': " +
                filesystemError.message();
            linked = false;
        }
    }
    std::error_code cleanupError;
    if (!keepLinkTemporaries()) std::filesystem::remove_all(directory, cleanupError);
    return linked;
}
}

bool LldLinker::createStaticLibrary(
    std::span<const std::filesystem::path> objectFiles,
    const std::filesystem::path& outputPath,
    std::string& error) const
{
    error.clear();
    std::filesystem::path librarianPath;
    if (!validateStaticLibraryRequest(
            objectFiles, outputPath, executablePath_, librarianPath, error)) return false;
    std::filesystem::path responsePath = outputPath;
    std::filesystem::path logPath = outputPath;
    responsePath += ".lib.rsp";
    logPath += ".lib.log";
    std::vector<std::string> arguments = {
        "/machine:x64",
        responsePathArgument("/out:", outputPath)};
    const std::filesystem::path workingDirectory =
        staticLibraryWorkingDirectory(outputPath, objectFiles);
    appendStaticLibraryObjects(arguments, objectFiles, workingDirectory);
    if (!writeResponseFile(responsePath, arguments, error)) return false;
    std::error_code cleanupError;
    std::filesystem::remove(outputPath, cleanupError);
    LldLinker librarian(librarianPath);
    const bool success = librarian.runResponseFile(
        responsePath,
        logPath,
        error,
        false,
        workingDirectory);
    cleanupLinkerTemporaries(responsePath, logPath, cleanupError);
    if (!success)
    {
        std::filesystem::remove(outputPath, cleanupError);
        return false;
    }
    if (!std::filesystem::is_regular_file(outputPath))
    {
        error = "llvm-lib reported success but did not create the requested static library";
        return false;
    }
    return true;
}

namespace
{
bool validateDynamicLibraryRequest(
    std::span<const std::filesystem::path> objectFiles,
    std::span<const std::filesystem::path> importLibraries,
    std::span<const std::string> exportSymbols,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    const std::filesystem::path& pdbPath,
    std::string& error)
{
    if (!validateObjectFiles(objectFiles, "DLL linking", error) ||
        !validateLibraries(importLibraries, "DLL linking", error)) return false;
    if (outputPath.empty() || importLibraryPath.empty() ||
        containsQuote(outputPath) || containsQuote(importLibraryPath) ||
        (!pdbPath.empty() && containsQuote(pdbPath)))
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
    return true;
}

std::vector<std::string> dynamicLibraryArguments(
    std::span<const std::filesystem::path> objectFiles,
    std::span<const std::filesystem::path> importLibraries,
    std::span<const std::string> exportSymbols,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    const std::filesystem::path& pdbPath)
{
    std::vector<std::string> arguments = {
        "/dll", "/noentry", "/nodefaultlib", "/machine:x64", "/dynamicbase", "/nxcompat",
        "/highentropyva", responsePathArgument("/out:", outputPath)};
    // NSLink ya fija TimeDateStamp=0 y deliberadamente rechaza switches desconocidos. /Brepro
    // sólo pertenece a lld-link; allí elimina el reloj de pared sin reescribir el PE después.
    if (!pdbPath.empty())
    {
        arguments.push_back("/debug:full");
        // NSLink runs inside the private link directory and records this spelling in the PDB.
        // NSLink corre dentro del directorio de enlace privado y guarda esta escritura en el PDB.
        arguments.push_back(responsePathArgument("/pdb:", linkerPdbSpelling(pdbPath)));
        arguments.push_back("/pdbaltpath:%_PDB%");
    }
    if (!exportSymbols.empty())
        arguments.push_back(responsePathArgument("/implib:", importLibraryPath));
    for (const std::string& symbol : exportSymbols)
        arguments.push_back("/export:" + symbol);
    for (const std::filesystem::path& path : objectFiles)
        arguments.push_back(responseFileArgument(path));
    for (const std::filesystem::path& path : importLibraries)
        arguments.push_back(responseFileArgument(path));
    return arguments;
}

void removeDynamicLibraryOutputs(
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    const std::filesystem::path& pdbPath,
    std::error_code& cleanupError)
{
    std::filesystem::remove(outputPath, cleanupError);
    std::filesystem::remove(importLibraryPath, cleanupError);
    if (!pdbPath.empty()) std::filesystem::remove(pdbPath, cleanupError);
}

bool verifyDynamicLibraryProducts(
    const LldLinker& linker,
    std::span<const std::filesystem::path> objectFiles,
    std::span<const std::string> exportSymbols,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    const std::filesystem::path& pdbPath,
    std::string& error)
{
    std::error_code cleanupError;
    if (!std::filesystem::is_regular_file(outputPath))
    {
        error = linkerProductName(linker.executablePath()) +
            " reported success but did not create the requested DLL";
        removeDynamicLibraryOutputs(outputPath, importLibraryPath, pdbPath, cleanupError);
        return false;
    }
    if (!pdbPath.empty() && !std::filesystem::is_regular_file(pdbPath))
    {
        error = linkerProductName(linker.executablePath()) +
            " reported success but did not create the requested DLL PDB";
        removeDynamicLibraryOutputs(outputPath, importLibraryPath, pdbPath, cleanupError);
        return false;
    }
    if (exportSymbols.empty())
    {
        // With no exported PE symbols there is no import library for the linker to
        // synthesize. LibraryReference still needs a validated COFF companion for
        // the NSMETA-only product, so archive one producer object without inventing
        // an import thunk or a callable export.
        const std::span<const std::filesystem::path> companionObject = objectFiles.first(1);
        if (linker.createStaticLibrary(companionObject, importLibraryPath, error)) return true;
        std::filesystem::remove(outputPath, cleanupError);
        if (!pdbPath.empty()) std::filesystem::remove(pdbPath, cleanupError);
        return false;
    }
    if (std::filesystem::is_regular_file(importLibraryPath)) return true;
    error = linkerProductName(linker.executablePath()) +
        " reported success but did not create the requested import library";
    std::filesystem::remove(outputPath, cleanupError);
    if (!pdbPath.empty()) std::filesystem::remove(pdbPath, cleanupError);
    return false;
}
}

bool LldLinker::linkDynamicLibrary(
    std::span<const std::filesystem::path> objectFiles,
    std::span<const std::filesystem::path> importLibraries,
    std::span<const std::string> exportSymbols,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& importLibraryPath,
    const std::filesystem::path& pdbPath,
    std::string& error) const
{
    error.clear();
    if (!validateDynamicLibraryRequest(
            objectFiles, importLibraries, exportSymbols,
            outputPath, importLibraryPath, pdbPath, error)) return false;
    std::filesystem::path responsePath = outputPath;
    std::filesystem::path logPath = outputPath;
    responsePath += ".dll.rsp";
    logPath += ".dll.log";
    std::filesystem::path linkDirectory;
    std::vector<std::filesystem::path> stableObjects;
    if (!prepareStableLinkInputs(outputPath, objectFiles, linkDirectory, stableObjects, error))
        return false;
    const std::vector<std::string> arguments = dynamicLibraryArguments(
        stableObjects, importLibraries, exportSymbols, outputPath, importLibraryPath, pdbPath);
    std::error_code cleanupError;
    if (!writeResponseFile(responsePath, arguments, error))
        return finishStableLinkInputs(linkDirectory, {}, false, error);
    removeDynamicLibraryOutputs(outputPath, importLibraryPath, pdbPath, cleanupError);
    bool success = runResponseFile(responsePath, logPath, error, false, linkDirectory);
    success = finishStableLinkInputs(linkDirectory, pdbPath, success, error);
    cleanupLinkerTemporaries(responsePath, logPath, cleanupError);
    if (!success)
    {
        removeDynamicLibraryOutputs(outputPath, importLibraryPath, pdbPath, cleanupError);
        return false;
    }
    return verifyDynamicLibraryProducts(
        *this, objectFiles, exportSymbols, outputPath, importLibraryPath, pdbPath, error);
}

namespace
{
bool validateExecutableRequest(
    std::span<const std::filesystem::path> objectFiles,
    std::span<const std::filesystem::path> importLibraries,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& pdbPath,
    std::string& error)
{
    if (!validateObjectFiles(objectFiles, "PE linking", error) ||
        !validateLibraries(importLibraries, "PE linking", error)) return false;
    if (outputPath.empty() || containsQuote(outputPath))
    {
        error = "PE output path is empty or contains an unsupported quote character";
        return false;
    }
    if (pdbPath.empty() || !containsQuote(pdbPath)) return true;
    error = "PDB output path contains an unsupported quote character";
    return false;
}

std::vector<std::string> executableArguments(
    std::span<const std::filesystem::path> objectFiles,
    std::span<const std::filesystem::path> importLibraries,
    const std::filesystem::path& outputPath,
    WindowsExecutableSubsystem subsystem,
    const std::filesystem::path& pdbPath)
{
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
        // NSLink runs inside the private link directory and records this spelling in the PDB.
        // NSLink corre dentro del directorio de enlace privado y guarda esta escritura en el PDB.
        arguments.push_back(responsePathArgument("/pdb:", linkerPdbSpelling(pdbPath)));
        arguments.push_back("/pdbaltpath:%_PDB%");
    }
    for (const std::filesystem::path& path : objectFiles)
        arguments.push_back(responseFileArgument(path));
    for (const std::filesystem::path& path : importLibraries)
        arguments.push_back(responseFileArgument(path));
    return arguments;
}

void removeExecutableOutputs(
    const std::filesystem::path& outputPath,
    const std::filesystem::path& pdbPath,
    std::error_code& cleanupError)
{
    std::filesystem::remove(outputPath, cleanupError);
    if (!pdbPath.empty()) std::filesystem::remove(pdbPath, cleanupError);
}

bool verifyExecutableProducts(
    const std::filesystem::path& executable,
    const std::filesystem::path& outputPath,
    const std::filesystem::path& pdbPath,
    std::string& error)
{
    if (!std::filesystem::is_regular_file(outputPath))
    {
        error = linkerProductName(executable) +
            " reported success but did not create the requested PE executable";
        return false;
    }
    if (pdbPath.empty() || std::filesystem::is_regular_file(pdbPath)) return true;
    std::error_code cleanupError;
    std::filesystem::remove(outputPath, cleanupError);
    error = linkerProductName(executable) +
        " reported success but did not create the requested PDB";
    return false;
}
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
    if (!validateExecutableRequest(
            objectFiles, importLibraries, outputPath, pdbPath, error)) return false;
    std::filesystem::path responsePath = outputPath;
    std::filesystem::path logPath = outputPath;
    responsePath += ".link.rsp";
    logPath += ".link.log";
    std::filesystem::path linkDirectory;
    std::vector<std::filesystem::path> stableObjects;
    if (!prepareStableLinkInputs(outputPath, objectFiles, linkDirectory, stableObjects, error))
        return false;
    const std::vector<std::string> arguments = executableArguments(
        stableObjects, importLibraries, outputPath, subsystem, pdbPath);
    std::error_code cleanupError;
    if (!writeResponseFile(responsePath, arguments, error))
        return finishStableLinkInputs(linkDirectory, {}, false, error);
    removeExecutableOutputs(outputPath, pdbPath, cleanupError);
    bool success = runResponseFile(responsePath, logPath, error, false, linkDirectory);
    success = finishStableLinkInputs(linkDirectory, pdbPath, success, error);
    cleanupLinkerTemporaries(responsePath, logPath, cleanupError);
    if (!success)
    {
        removeExecutableOutputs(outputPath, pdbPath, cleanupError);
        return false;
    }
    return verifyExecutableProducts(executablePath_, outputPath, pdbPath, error);
}
}
