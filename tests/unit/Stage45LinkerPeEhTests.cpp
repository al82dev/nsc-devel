#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Linker/PeExecutableVerifier.h"

#include "../support/TestContext.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::linker::PeExecutableInfo;
using nativesharp::linker::PeImportedSymbol;
using nativesharp::linker::PeExecutableVerifier;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

bool containsLibrary(const PeExecutableInfo& info, std::string_view expected)
{
    for (const std::string& library : info.importedLibraries)
    {
        if (library == expected)
        {
            return true;
        }
    }
    return false;
}

bool importsSymbol(
    const PeExecutableInfo& info,
    std::string_view library,
    std::string_view symbol)
{
    for (const PeImportedSymbol& imported : info.importedSymbols)
    {
        if (imported.library == library && imported.name == symbol)
        {
            return true;
        }
    }
    return false;
}

ExitCode emitProjectExe(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string& output,
    std::string& error,
    bool optimize)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    if (optimize)
    {
        const std::array<std::string_view, 5> args = {
            "-O2", "--emit-project-oop-exe", projectText, "-o", executableText};
        const ExitCode result = driver.run(args, out, err);
        output = out.str();
        error = err.str();
        return result;
    }
    const std::array<std::string_view, 4> args = {
        "--emit-project-oop-exe", projectText, "-o", executableText};
    const ExitCode result = driver.run(args, out, err);
    output = out.str();
    error = err.str();
    return result;
}

void testEndToEndEhPe(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "EhPeApp";
    writeProject(directory, "EhPeApp", R"NS(
namespace NativeSharp.Core
{
    public class Exception { }
    public interface IDisposable { void Dispose(); }
}
using NativeSharp.Core;

class BaseProblem : Exception { }
class DerivedProblem : BaseProblem { }

class Resource : IDisposable
{
    public void Dispose() { int cleaned = 1; }
}

class Program
{
    public static int Main()
    {
        using (Resource resource = new Resource())
        {
            try
            {
                throw new DerivedProblem();
            }
            catch (DerivedProblem derived)
            {
                return 11;
            }
            catch (BaseProblem baseProblem)
            {
                return 22;
            }
            finally
            {
                int finalized = 33;
            }
        }
        return 44;
    }
}
)NS");

    const std::filesystem::path executable = directory / "EhPeApp.exe";
    std::string output;
    std::string error;
    const ExitCode result = emitProjectExe(
        directory / "EhPeApp.nsproj", executable, output, error, false);
    tests.expect(result == ExitCode::Success,
        "Stage 45.12 driver emits a complete OOP Win64 EH executable");
    tests.expect(error.empty(),
        "successful OOP EH executable generation keeps stderr empty");
    tests.expect(output.find("project OOP PE executable emission: ok") != std::string::npos,
        "driver reports successful project OOP PE emission");
    tests.expect(output.find("entry-symbol: __nsc_method_EhPeApp$3A$3AProgram__Main") != std::string::npos,
        "driver exposes the selected mangled static Main entry symbol");
    tests.expect(std::filesystem::is_regular_file(executable) &&
            std::filesystem::file_size(executable) > 1024,
        "Stage 45.12 produces a non-trivial PE executable artifact");

    PeExecutableVerifier verifier;
    tests.expect(verifier.verify(executable),
        "PE verifier accepts the linked OOP EH executable");
    if (!verifier.verify(executable))
    {
        return;
    }
    const PeExecutableInfo& info = verifier.info();
    tests.expect(info.machine == 0x8664,
        "final OOP EH executable targets IMAGE_FILE_MACHINE_AMD64");
    tests.expect(info.optionalHeaderMagic == 0x020b,
        "final OOP EH executable is PE32+");
    tests.expect(info.subsystem == 3,
        "final OOP EH executable uses the Windows console subsystem");
    tests.expect(info.entryPointRva != 0,
        "final OOP EH executable has a native entry point");
    tests.expect(info.clrDirectoryRva == 0,
        "final OOP EH executable contains no CLR runtime header");
    tests.expect(info.exceptionDirectoryRva != 0,
        "Win64 EH executable exposes a non-zero PE exception directory");
    tests.expect(info.exceptionDirectorySize >= 12 && (info.exceptionDirectorySize % 12) == 0,
        "PE exception directory contains complete AMD64 RUNTIME_FUNCTION entries");
    tests.expect(containsLibrary(info, "KERNEL32.dll"),
        "final executable imports NativeSharp runtime OS services from KERNEL32.dll");
    tests.expect(containsLibrary(info, "ucrtbase.dll"),
        "final EH executable imports the Windows UCRT language-specific handler support");
    tests.expect(!containsLibrary(info, "vcruntime140.dll"),
        "final executable does not require the Visual C++ redistributable DLL");
    tests.expect(importsSymbol(info, "ucrtbase.dll", "__C_specific_handler"),
        "PE import table resolves LLVM Win64 personality __C_specific_handler");
    tests.expect(importsSymbol(info, "ucrtbase.dll", "memset"),
        "PE import table resolves LLVM object zero-initialization helper memset");
    tests.expect(importsSymbol(info, "KERNEL32.dll", "RaiseException"),
        "PE import table resolves NativeSharp throw transport through RaiseException");
    tests.expect(importsSymbol(info, "KERNEL32.dll", "TlsAlloc") &&
            importsSymbol(info, "KERNEL32.dll", "TlsGetValue") &&
            importsSymbol(info, "KERNEL32.dll", "TlsSetValue"),
        "PE import table resolves all NativeSharp exception TLS primitives");
    tests.expect(importsSymbol(info, "KERNEL32.dll", "ExitProcess"),
        "PE import table resolves the native process exit primitive");
    tests.expect(output.find("exception-directory-rva: 0") == std::string::npos,
        "driver reports a materialized Win64 exception directory");
}

void testEntryPointContract(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path missing = root / "MissingMain";
    writeProject(missing, "MissingMain", R"NS(
class Program
{
    public static int Run() { return 1; }
}
)NS");
    const std::filesystem::path missingExe = missing / "MissingMain.exe";
    std::string output;
    std::string error;
    tests.expect(emitProjectExe(
            missing / "MissingMain.nsproj", missingExe, output, error, false) ==
            ExitCode::CompilationError,
        "OOP executable generation rejects a project without static int Main()");
    tests.expect(error.find(
            "requires exactly one int Main() function or static method with no parameters") !=
            std::string::npos,
        "missing project entry point reports the namespace-or-method contract");
    tests.expect(!std::filesystem::exists(missingExe),
        "missing-entry failure leaves no stale executable");

    const std::filesystem::path ambiguous = root / "AmbiguousMain";
    writeProject(ambiguous, "AmbiguousMain", R"NS(
class First { public static int Main() { return 1; } }
class Second { public static int Main() { return 2; } }
)NS");
    const std::filesystem::path ambiguousExe = ambiguous / "AmbiguousMain.exe";
    output.clear();
    error.clear();
    tests.expect(emitProjectExe(
            ambiguous / "AmbiguousMain.nsproj", ambiguousExe, output, error, false) ==
            ExitCode::CompilationError,
        "OOP executable generation rejects multiple static int Main() methods");
    tests.expect(error.find("multiple int Main() entry points") != std::string::npos,
        "ambiguous project entry point reports a deterministic diagnostic");
    tests.expect(!std::filesystem::exists(ambiguousExe),
        "ambiguous-entry failure leaves no stale executable");
}

void testO2Linkage(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path directory = root / "OptimizedEh";
    writeProject(directory, "OptimizedEh", R"NS(
namespace NativeSharp.Core { public class Exception { } }
using NativeSharp.Core;
class Problem : Exception { }
class Program
{
    public static int Main()
    {
        try { throw new Problem(); }
        catch (Problem problem) { return 7; }
    }
}
)NS");
    const std::filesystem::path executable = directory / "OptimizedEh.exe";
    std::string output;
    std::string error;
    tests.expect(emitProjectExe(
            directory / "OptimizedEh.nsproj", executable, output, error, true) == ExitCode::Success,
        "O2 project OOP EH executable links through the same Stage 45.12 path");
    tests.expect(error.empty() && output.find("optimization: O2") != std::string::npos,
        "O2 executable generation reports the requested LLVM optimization level");
    PeExecutableVerifier verifier;
    tests.expect(verifier.verify(executable),
        "optimized OOP EH executable remains structurally valid PE32+");
    if (verifier.verify(executable))
    {
        tests.expect(verifier.info().exceptionDirectoryRva != 0 &&
                importsSymbol(verifier.info(), "ucrtbase.dll", "__C_specific_handler"),
            "O2 preserves Win64 unwind metadata and the SEH personality import");
    }
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "nativesharp_stage45_12_linker_pe_eh";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root, cleanupError);

    testEndToEndEhPe(tests, root);
    testEntryPointContract(tests, root);
    testO2Linkage(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
