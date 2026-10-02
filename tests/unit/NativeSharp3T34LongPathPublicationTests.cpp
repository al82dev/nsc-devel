#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/DiagnosticBundle.h"
#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <Windows.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace fs = std::filesystem;
using nativesharp::ExitCode;

fs::path extended(const fs::path& input)
{
    const std::wstring value = fs::absolute(input).native();
    if (value.starts_with(L"\\\\?\\")) return input;
    if (value.starts_with(L"\\\\")) return fs::path(L"\\\\?\\UNC\\" + value.substr(2));
    return fs::path(L"\\\\?\\" + value);
}

bool regular(const fs::path& path)
{
    const DWORD attributes = ::GetFileAttributesW(extended(path).c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string readFile(const fs::path& path)
{
    HANDLE file = ::CreateFileW(extended(path).c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size{};
    if (::GetFileSizeEx(file, &size) == 0 || size.QuadPart < 0 || size.QuadPart > 1024 * 1024)
    {
        ::CloseHandle(file);
        return {};
    }
    std::string result(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = result.empty() || ::ReadFile(file, result.data(),
        static_cast<DWORD>(result.size()), &read, nullptr) != 0;
    ::CloseHandle(file);
    if (!ok) return {};
    result.resize(read);
    return result;
}

void writeFile(const fs::path& path, std::string_view text)
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

ExitCode run(const std::vector<std::string>& owned, std::string& output, std::string& error)
{
    std::vector<std::string_view> arguments;
    for (const std::string& value : owned) arguments.push_back(value);
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode code = nativesharp::driver::Driver{}.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return code;
}

fs::path longDirectory(const fs::path& root)
{
    fs::path result = root;
    for (unsigned index = 0; index != 8; ++index)
        result /= "native-sharp-long-output-component-0123456789";
    return result;
}
}

int main()
{
    using nativesharp::basic::ArtifactTransaction;
    using nativesharp::basic::OutputFailureKind;
    using nativesharp::tests::TestContext;
    TestContext tests;

    const fs::path root = fs::path(NSC_BINARY_DIR) / "ns3_t34_long_path_publication";
    std::error_code ignored;
    fs::remove_all(extended(root), ignored);
    fs::create_directories(root, ignored);
    const fs::path outputs = longDirectory(root / "outputs");
    tests.expect(outputs.native().size() > 260U,
        "fixture final output directory exceeds the legacy Windows MAX_PATH boundary");

    const std::string runtime =
        (fs::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const fs::path executableProjectRoot = root / "exe";
    writeFile(executableProjectRoot / "LongExe.nsproj",
        "<Project><PropertyGroup><Name>LongExe</Name><OutputType>Exe</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(executableProjectRoot / "Main.ns",
        "public class Program { public static int Main() { return 0; } }");

    const fs::path libraryProjectRoot = root / "lib";
    writeFile(libraryProjectRoot / "LongLibrary.nsproj",
        "<Project><PropertyGroup><Name>LongLibrary</Name><OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\"/></ItemGroup></Project>");
    writeFile(libraryProjectRoot / "Api.ns",
        "public class Api { public static int Answer() { return 42; } }");

    std::string output;
    std::string error;
    const fs::path object = outputs / "LongExe.obj";
    const ExitCode objectCode = run({"--emit-project-oop-obj",
        (executableProjectRoot / "LongExe.nsproj").string(), "-O0", "-o", object.string()}, output, error);
    tests.expect(objectCode == ExitCode::Success && regular(object),
        "object publishes through a long-path transaction (" + error + output + ")");

    output.clear();
    error.clear();
    const fs::path executable = outputs / "LongExe.exe";
    const fs::path pdb = outputs / "LongExe.pdb";
    const ExitCode executableCode = run({"--emit-project-oop-exe",
        (executableProjectRoot / "LongExe.nsproj").string(), "-O0", "-g", "-o", executable.string()},
        output, error);
    tests.expect(executableCode == ExitCode::Success && regular(executable) && regular(pdb),
        "executable and PDB publish as one long-path artifact set (" + error + output + ")");

    output.clear();
    error.clear();
    const fs::path library = outputs / "LongLibrary.lib";
    const ExitCode libraryCode = run({"--emit-project-oop-lib",
        (libraryProjectRoot / "LongLibrary.nsproj").string(), "-O0", "-o", library.string()}, output, error);
    tests.expect(libraryCode == ExitCode::Success && regular(library) &&
            regular(outputs / "LongLibrary.h") && regular(outputs / "LongLibrary.nsmeta"),
        "library, host header, and NSMETA publish as one long-path artifact set (" + error + output + ")");

    const fs::path diagnostic = outputs / "failure.nsdiag";
    const std::string projectArgument = (executableProjectRoot / "LongExe.nsproj").string();
    const std::array<std::string_view, 2> diagnosticArguments = {
        "--check-project-oop-mir", projectArgument};
    std::string diagnosticError;
    const bool diagnosticWritten = nativesharp::driver::writeDiagnosticBundle(
        executableProjectRoot / "LongExe.nsproj", diagnostic, ExitCode::CompilationError,
        "stdout", "stderr", {}, diagnosticArguments, diagnosticError);
    tests.expect(diagnosticWritten && regular(diagnostic) &&
            readFile(diagnostic).starts_with("NSDIAG 1\n"),
        "diagnostic log uses the same long-path-safe transactional publisher (" + diagnosticError + ")");

    tests.expect(
        nativesharp::basic::classifyWindowsOutputError(ERROR_FILENAME_EXCED_RANGE) == OutputFailureKind::PathLimit &&
        nativesharp::basic::classifyWindowsOutputError(ERROR_PATH_NOT_FOUND) == OutputFailureKind::MissingComponent &&
        nativesharp::basic::classifyWindowsOutputError(ERROR_ACCESS_DENIED) == OutputFailureKind::PermissionDenied &&
        nativesharp::basic::classifyWindowsOutputError(ERROR_SHARING_VIOLATION) == OutputFailureKind::FileLocked &&
        nativesharp::basic::classifyWindowsOutputError(ERROR_DISK_FULL) == OutputFailureKind::InsufficientSpace,
        "preflight diagnostics distinguish path, component, permission, lock, and disk-space failures");

    const fs::path rollbackRoot = root / "rollback";
    fs::create_directories(rollbackRoot, ignored);
    const fs::path first = rollbackRoot / "first.txt";
    const fs::path second = rollbackRoot / "second.txt";
    writeFile(first, "old-first");
    writeFile(second, "old-second");
    ArtifactTransaction transaction;
    fs::path firstStage;
    fs::path secondStage;
    std::string transactionError;
    const bool staged = transaction.add(first, firstStage, transactionError) &&
        transaction.add(second, secondStage, transactionError);
    tests.expect(staged && firstStage.filename() == first.filename() &&
            secondStage.filename() == second.filename() &&
            firstStage.parent_path() != secondStage.parent_path(),
        "transaction stages preserve artifact basenames in isolated directories");
    if (staged)
    {
        writeFile(firstStage, "new-first");
        writeFile(secondStage, "new-second");
    }
    HANDLE locked = ::CreateFileW(second.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    const bool committed = staged && transaction.commit(transactionError);
    if (locked != INVALID_HANDLE_VALUE) ::CloseHandle(locked);
    tests.expect(!committed && transactionError.find("file-locked") != std::string::npos &&
            transactionError.find("rollback-complete") != std::string::npos &&
            readFile(first) == "old-first" && readFile(second) == "old-second",
        "a locked member aborts the set and restores every previous artifact");

    const fs::path occupiedDestination = root / "occupied.obj";
    fs::create_directory(occupiedDestination, ignored);
    ArtifactTransaction occupiedTransaction;
    fs::path occupiedStage;
    std::string occupiedError;
    const bool occupiedStaged =
        occupiedTransaction.add(occupiedDestination, occupiedStage, occupiedError);
    if (occupiedStaged) writeFile(occupiedStage, "must-not-replace-directory");
    const bool occupiedCommitted =
        occupiedStaged && occupiedTransaction.commit(occupiedError);
    tests.expect(!occupiedCommitted && fs::is_directory(occupiedDestination) &&
            fs::is_empty(occupiedDestination) &&
            occupiedError.find("destination-not-regular") != std::string::npos,
        "transaction refuses to replace an occupied directory with a file");

    const fs::path unicodeDestination =
        root / L"paquete_\u00e1\u00e9_\u8fd0\u884c" / L"resultado.txt";
    ArtifactTransaction unicodeTransaction;
    fs::path unicodeStage;
    std::string unicodeError;
    const bool unicodeStaged =
        unicodeTransaction.add(unicodeDestination, unicodeStage, unicodeError);
    if (unicodeStaged) writeFile(unicodeStage, "unicode-ok");
    const bool unicodeCommitted = unicodeStaged && unicodeTransaction.commit(unicodeError);
    tests.expect(unicodeCommitted && readFile(unicodeDestination) == "unicode-ok",
        "transaction creates and publishes into a UTF-16 output directory (" + unicodeError + ")");

    fs::remove_all(extended(root), ignored);
    return tests.result();
}
