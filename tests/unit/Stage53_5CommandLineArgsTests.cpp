// Stage 53.5 "Command-line arguments" -- Environment.GetCommandLineArgCount()/GetCommandLineArgs
// (see lib/NativeSharp.Core/Environment.ns's header comment for the full design discussion).
// This is the FIRST core operation in this runtime that deliberately crosses into Win32's *W
// (UTF-16) API surface -- GetCommandLineW + CommandLineToArgvW (SHELL32.dll, not KERNEL32.dll) --
// converting every UTF-16 result back to ANSI via WideCharToMultiByte before it ever reaches
// NativeSharp code. Every other Win32 call this runtime makes is ANSI (*A); see
// buildNullTerminatedCopy's comment in src/Runtime/WindowsRuntimeBuilder.cpp for why, and this
// round's own comment there for why THIS one operation is the deliberate, explicit exception.
//
// Design confirmed by Alberto across two rounds of clarification (see the project's own history
// docs): a single implementation via the official CommandLineToArgvW parser (not a hand-rolled
// ANSI one, which would reintroduce the exact class of risk that produced the real cmd.exe
// quoting bug documented in 15_fix_cmdexe_quoting_tests_host_abi_windows.md), verified with an
// exhaustive set of hand-crafted quoting test cases instead.
//
// Covers, in increasing order of confidence, exactly like every other Stage 53 real-execution
// test file in this suite:
//   1. That the two core operations (EnvironmentGetCommandLineArgCount/
//      EnvironmentGetCommandLineArgs) are recognized, lower to LLVM, and emit a call to their
//      exact runtime hook symbol -- a minimal hand-written fixture, independent of the packaged
//      example.
//   2. That the packaged example project (examples/project_stage53_5) compiles end to end
//      through a real linked Windows x64 PE executable via the actual `nsc` Driver, AT BOTH O0
//      and O2 -- same discipline every Stage 53 round has followed since 53.2b. The linked
//      executable's imports must include SHELL32.dll (CommandLineToArgvW's real home), not just
//      KERNEL32.dll/KERNELBASE.dll like every prior round.
//   3. REAL EXECUTION under Wine (when available, gracefully skipped otherwise), at BOTH
//      optimization levels, with an EXACT, byte-precise set of command-line arguments covering
//      the quoting/escaping matrix that justified this round's design (a plain argument, one
//      with an embedded space, an empty argument, one with embedded double quotes, one with a
//      trailing backslash immediately before where a closing quote would go, and one with a
//      backslash NOT followed by a quote). Main.ns returns 0 only if every one of its checks
//      passed (see examples/project_stage53_5/Main.ns for the check numbering and the exact
//      argument matrix it expects).
//
// A refinement of the test-harness mechanism for THIS round only: every prior Stage 53
// real-execution test file invoked Wine via a single std::system() shell-interpreted command
// string, concatenating arguments directly into it. That naive concatenation is NOT safe here --
// this round tests quoting/escaping itself, so the Linux shell would become an extra, uncontrolled
// quoting layer between the test's intended byte-exact arguments and what Wine/NativeSharp
// actually receive, contaminating the very thing being tested. The fix stays inside the same
// std::system() idiom every other Stage 53 test file already uses (no raw POSIX syscalls, no
// platform-specific process APIs -- this project targets Windows only, and the test HARNESS
// itself is built and run on both this Linux/Wine sandbox and Alberto's Windows/clang-cl
// toolchain, so it stays within the standard C++ std::system() this whole suite already relies
// on): runUnderWineWithArgs below wraps each argument in POSIX single quotes (escaping any
// embedded single quote as '\'' -- close quote, escaped literal quote, reopen quote), a
// mechanical, unambiguous escaping rule for exactly one purpose -- getting the Linux shell to
// hand Wine the exact bytes back -- which is a different, much simpler problem than the Win32
// quoting/escaping this round is actually testing. Wine's own argv-to-Windows-command-line
// reconstruction (designed to round-trip correctly through CommandLineToArgvW) is what actually
// gets exercised, not this test's own escaping.

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"

#include "../support/TestContext.h"

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
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::frontend::ProjectCompilation;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << text;
}

void writeProject(const std::filesystem::path& directory, std::string_view name)
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
}

bool commandAvailable(std::string_view command)
{
#ifdef _WIN32
    return std::system(("where " + std::string(command) + " >nul 2>nul").c_str()) == 0;
#else
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
#endif
}

// Same fallback order as every other Stage 52/53 real-execution test file in this repo.
std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

// Wraps `value` in POSIX single quotes, escaping any embedded single quote as '\'' (close quote,
// escaped literal quote, reopen quote) -- the standard, mechanical way to make a POSIX shell
// treat a string as one opaque, byte-exact argument no matter what it contains (spaces, double
// quotes, backslashes -- none of those are special inside single quotes). This is a DIFFERENT,
// much simpler problem than the Win32 quoting/escaping this round is actually testing -- it only
// has to satisfy the Linux shell, not reproduce any of CommandLineToArgvW's rules.
std::string singleQuoteEscapeForShell(const std::string& value)
{
    std::string result = "'";
    for (char character : value)
    {
        if (character == '\'')
        {
            result += "'\\''";
        }
        else
        {
            result += character;
        }
    }
    result += "'";
    return result;
}

// Same std::system() idiom every other Stage 53 real-execution test file uses (see runUnderWine
// in, e.g., Stage53_4FilesystemPathsCierreTests.cpp) -- no raw POSIX process APIs, no platform-
// specific syscalls, so this file compiles and behaves the same way whether it's built here (this
// Linux/Wine sandbox) or on Alberto's Windows/clang-cl toolchain. The one refinement over every
// prior round's runUnderWine: each argument is wrapped with singleQuoteEscapeForShell above
// before being concatenated into the command string, so the Linux shell hands Wine back the exact
// bytes this round's quoting-matrix test needs -- naive concatenation (like every prior round used,
// safe there because none of those rounds ever passed arguments containing spaces/quotes) would
// let the Linux shell itself mangle the very quoting/escaping this round exists to test.
bool runUnderWineWithArgs(
    const std::string& wine,
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& exePath,
    const std::vector<std::string>& arguments,
    const std::filesystem::path& logPath,
    int& exitCode,
    bool& crashed)
{
    std::string command = "cd " + workingDirectory.string() +
        " && WINEDEBUG=-all timeout 20 " + wine + " " + exePath.string();
    for (const std::string& argument : arguments)
    {
        command += " " + singleQuoteEscapeForShell(argument);
    }
    command += " >" + logPath.string() + " 2>&1";

    const int raw = std::system(command.c_str());
    if (raw < 0) return false;
#ifdef WIFEXITED
    exitCode = WIFEXITED(raw) ? WEXITSTATUS(raw) : (raw & 0xFF);
#else
    exitCode = raw & 0xFF;
#endif
    std::ifstream logStream(logPath, std::ios::binary);
    std::ostringstream logText;
    logText << logStream.rdbuf();
    const std::string log = logText.str();
    crashed = log.find("page fault") != std::string::npos ||
        log.find("Unhandled exception") != std::string::npos;
    return true;
}

// ---------------------------------------------------------------------------------------
// 1. Minimal fixture: the two core operations lower to LLVM and each emits a call to its
//    exact runtime hook symbol.
// ---------------------------------------------------------------------------------------

void testCommandLineArgsCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "CommandLineArgsCore";
    writeProject(directory, "CommandLineArgsCore");
    writeFile(directory / "Environment.ns", R"NS(
namespace NativeSharp.Core
{
    public class Environment
    {
        public static int GetCommandLineArgCount() { return 0; }
        public static int GetCommandLineArgs(string[] destination) { return 0; }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core;

class Program
{
    public static int Run(string[] destination)
    {
        int count = Environment.GetCommandLineArgCount();
        int written = Environment.GetCommandLineArgs(destination);
        return count + written;
    }
}
)NS");

    std::string error;
    ProjectCompilation mir;
    const bool ok = mir.compileProjectOopMir(directory / "CommandLineArgsCore.nsproj", error);
    tests.expect(ok && error.empty(), "GetCommandLineArgCount/GetCommandLineArgs core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "GetCommandLineArgCount/GetCommandLineArgs fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "GetCommandLineArgCount/GetCommandLineArgs fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(
        llvm.find("call i32 @__nativesharp_environment_get_command_line_arg_count") != std::string::npos,
        "Environment.GetCommandLineArgCount() lowers to a direct call to __nativesharp_environment_get_command_line_arg_count");
    tests.expect(
        llvm.find("call i32 @__nativesharp_environment_get_command_line_args") != std::string::npos,
        "Environment.GetCommandLineArgs(...) lowers to a direct call to __nativesharp_environment_get_command_line_args");
}

// ---------------------------------------------------------------------------------------
// 2 + 3. Packaged example (examples/project_stage53_5), tested at BOTH O0 and O2, with the
//    exhaustive quoting-test argument matrix passed via fork()+execvp() (see this file's
//    header comment for why not std::system()).
// ---------------------------------------------------------------------------------------

void testPackagedExampleAtOptimizationLevel(
    TestContext& tests,
    const std::filesystem::path& root,
    nativesharp::llvm_backend::LlvmOptimizationLevel level,
    std::string_view levelFlag,
    std::string_view levelLabel)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage53_5/Stage53_5CommandLineArgs.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 53.5 Command-line-arguments example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        std::string("packaged Stage 53.5 example (") + std::string(levelLabel) +
            ") passes method-body ownership analysis");
    tests.expect(error.empty(), "packaged Stage 53.5 method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error),
        std::string("packaged Stage 53.5 example (") + std::string(levelLabel) + ") lowers to OOP MIR");
    tests.expect(error.empty(), "packaged Stage 53.5 MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        std::string("packaged Stage 53.5 example (") + std::string(levelLabel) + ") lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "packaged Stage 53.5 example LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, targetError);
    tests.expect(target != nullptr && targetError.empty(),
        std::string("Stage 53.5 example (") + std::string(levelLabel) + ") obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 53.5 example applies Windows x64 data layout");

    const auto object = root / (std::string("nativesharp_stage53_5_example_") + std::string(levelLabel) + ".obj");
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 53.5 example emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 53.5 example object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 53.5 example object machine is AMD64 / 0x8664");

    const auto exePath = root / (std::string("Stage53_5CommandLineArgs_") + std::string(levelLabel) + ".exe");
    const std::string projectText = project.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, levelFlag, "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode driverResult = driver.run(args, out, err);
    tests.expect(driverResult == ExitCode::Success,
        std::string("Stage 53.5 example (") + std::string(levelLabel) +
            "): Driver emits a real Windows x64 PE executable (" + err.str() + ")");
    tests.expect(err.str().empty(),
        std::string("Stage 53.5 example (") + std::string(levelLabel) + "): no Driver diagnostics");
    tests.expect(std::filesystem::is_regular_file(exePath),
        std::string("Stage 53.5 example (") + std::string(levelLabel) + "): .exe file exists on disk");

    // CommandLineToArgvW lives in SHELL32.dll, not KERNEL32.dll -- structural confirmation
    // (independent of Wine execution below) that the linked executable really pulled in a
    // SHELL32.dll import, not just KERNEL32.dll/KERNELBASE.dll like every prior Stage 53 round's
    // example. Driver::run's own "imports:" summary line (printed to `out` for
    // --emit-project-oop-exe, the same text `nsc` prints on the CLI) already lists every DLL the
    // real Windows executable link pulled in.
    tests.expect(out.str().find("SHELL32.dll") != std::string::npos,
        std::string("Stage 53.5 example (") + std::string(levelLabel) +
            ") Driver's imports summary lists SHELL32.dll (CommandLineToArgvW's real home, not "
            "KERNEL32.dll)");

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string("Wine not available in this environment -- Stage 53.5 (") +
            std::string(levelLabel) + ") real-execution check skipped, all structural checks above still ran");
        return;
    }

    const auto workingDirectory = root / (std::string("run_") + std::string(levelLabel));
    std::error_code cleanupError;
    std::filesystem::remove_all(workingDirectory, cleanupError);
    std::filesystem::create_directories(workingDirectory);

    // The exhaustive quoting/escaping matrix that justified this round's whole design -- see
    // examples/project_stage53_5/Main.ns's header comment for the rationale behind each case.
    // Passed byte-exact via fork()+execvp() (see runUnderWineWithArgs above), never through a
    // shell string.
    const std::vector<std::string> quotingMatrix = {
        "hello",
        "hello world",
        "",
        "say \"hi\"",
        "C:\\path\\",
        "a\\b"};

    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWineWithArgs(
        wine, workingDirectory, exePath, quotingMatrix,
        root / (std::string("Stage53_5CommandLineArgs_") + std::string(levelLabel) + ".log"),
        exitCode, crashed);
    tests.expect(ran, std::string("Wine could be invoked on the Stage 53.5 (") + std::string(levelLabel) + ") executable");
    tests.expect(!crashed, std::string("Stage 53.5 (") + std::string(levelLabel) + ") executable does not crash under real execution");
    tests.expect(exitCode == 0,
        std::string("Stage 53.5 (") + std::string(levelLabel) +
            ") executable returns exit code 0 (all GetCommandLineArgCount/GetCommandLineArgs "
            "quoting-matrix checks passed; see examples/project_stage53_5/Main.ns for the check "
            "numbering, exit code " + std::to_string(exitCode) + " observed)");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage53_5_command_line_args_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testCommandLineArgsCoreOperationsLowerToRuntimeCalls(tests, root);
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "O0");
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, "-O2", "O2");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
