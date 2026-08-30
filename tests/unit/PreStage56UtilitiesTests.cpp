// Pre-Stage-56 "utilidades basicas" round (Alberto, 2026-08-29).
//
// Covers, in increasing order of confidence, exactly like every other Stage 53/54/55 real-
// execution test file in this suite:
//   1. That the four new core operations (ThreadSleepRaw/FileCopyRaw/FileMoveRaw/
//      DirectoryMoveRaw) are recognized, lower to LLVM, and emit a call to their exact runtime
//      hook symbol -- a minimal hand-written fixture, independent of the packaged example, so a
//      future change that breaks core-operation recognition fails HERE with a precise message
//      instead of only failing deep inside the packaged example. FileMoveRaw/DirectoryMoveRaw
//      share one runtime hook (see buildMoveFile's header comment in WindowsRuntimeBuilder.cpp)
//      -- this fixture confirms BOTH MIR node kinds emit a call to that SAME symbol.
//   2. That the packaged example project (examples/project_prestage56_utilidades) -- exercising
//      all eight pieces of this round (Thread.Sleep, File.Copy/Move, Directory.Move, Random,
//      NumericLimits, ScopedLock, MathUtil) -- compiles end to end through a real linked Windows
//      x64 PE executable via the actual `nsc` Driver (--emit-project-oop-exe), not a hand-rolled
//      subset of it.
//   3. REAL EXECUTION under Wine (when available, gracefully skipped otherwise): Main.ns returns
//      0 only if every one of its runtime checks passes, and a distinct nonzero code identifying
//      exactly which check failed otherwise, so a regression here is immediately diagnosable from
//      the exit code alone.
//
// Random/NumericLimits/ScopedLock/MathUtil are pure NativeSharp source with ZERO core operations
// (see each file's own header comment) -- there is no LLVM-lowering fixture to write for them,
// unlike ThreadSleepRaw/FileCopyRaw/FileMoveRaw/DirectoryMoveRaw. Their correctness is verified
// entirely through the packaged example's real-execution checks in step 3 above.

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

// Same fallback order as every other Stage 52/53/54/55 real-execution test file in this repo.
std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

// Takes a `workingDirectory` and `cd`s into it before invoking Wine -- same pattern
// Stage53_3FilesystemPathsTests.cpp established for its own File/Directory real-execution check
// (see that file's header comment): File.Copy/Move/Directory.Move in this round's packaged
// example create/rename real files and directories using RELATIVE paths, so the process's current
// directory at the moment Wine starts it is not cosmetic -- without the `cd`, everything would be
// created inside whatever directory `std::system` happened to inherit (typically the test
// binary's own build directory), leaking state across separate runs of this same test.
bool runUnderWine(
    const std::string& wine,
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& exePath,
    const std::filesystem::path& logPath,
    int& exitCode,
    bool& crashed)
{
    const std::string command = "cd " + workingDirectory.string() +
        " && WINEDEBUG=-all timeout 20 " + wine + " " + exePath.string() +
        " >" + logPath.string() + " 2>&1";
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
// 1. Minimal fixture: the four new core operations lower to LLVM and each emits a call to its
//    exact runtime hook symbol name. FileMoveRaw/DirectoryMoveRaw share one hook on purpose.
// ---------------------------------------------------------------------------------------

void testNewCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "PreStage56Core";
    writeProject(directory, "PreStage56Core");
    writeFile(directory / "Thread.ns", R"NS(
namespace NativeSharp.Core.Threading
{
    public class Thread
    {
        public static void SleepRaw(int milliseconds) { }
    }
}
)NS");
    writeFile(directory / "File.ns", R"NS(
namespace NativeSharp.Core.IO
{
    public class File
    {
        public static bool CopyRaw(string sourcePath, string destPath, bool overwrite) { return false; }
        public static bool MoveRaw(string sourcePath, string destPath) { return false; }
    }
}
)NS");
    writeFile(directory / "Directory.ns", R"NS(
namespace NativeSharp.Core.IO
{
    public class Directory
    {
        public static bool MoveRaw(string sourcePath, string destPath) { return false; }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.IO;
using NativeSharp.Core.Threading;

class Program
{
    public static bool Run()
    {
        Thread.SleepRaw(5);
        bool copied = File.CopyRaw("a.txt", "b.txt", true);
        bool movedFile = File.MoveRaw("b.txt", "c.txt");
        bool movedDir = Directory.MoveRaw("dir-a", "dir-b");
        return copied && movedFile && movedDir;
    }
}
)NS");

    ProjectCompilation mir;
    std::string error;
    const bool ok = mir.compileProjectOopMir(directory / "PreStage56Core.nsproj", error);
    tests.expect(ok && error.empty(), "Pre-Stage-56 core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Pre-Stage-56 core-operation fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Pre-Stage-56 core-operation fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("call void @__nativesharp_thread_sleep") != std::string::npos,
        "SleepRaw(int) lowers to a direct call to __nativesharp_thread_sleep");
    tests.expect(llvm.find("call i1 @__nativesharp_file_copy") != std::string::npos,
        "CopyRaw(string,string,bool) lowers to a direct call to __nativesharp_file_copy");
    // FileMoveRaw and DirectoryMoveRaw share the SAME runtime hook -- both call sites must emit a
    // call to the identical symbol, __nativesharp_move_file (see buildMoveFile's header comment).
    const std::size_t firstMoveCall = llvm.find("call i1 @__nativesharp_move_file");
    tests.expect(firstMoveCall != std::string::npos,
        "File.MoveRaw(string,string) lowers to a direct call to __nativesharp_move_file");
    const std::size_t secondMoveCall = firstMoveCall == std::string::npos
        ? std::string::npos
        : llvm.find("call i1 @__nativesharp_move_file", firstMoveCall + 1);
    tests.expect(secondMoveCall != std::string::npos,
        "Directory.MoveRaw(string,string) ALSO lowers to a direct call to __nativesharp_move_file "
        "(a second, distinct call site to the same shared symbol)");
}

// ---------------------------------------------------------------------------------------
// 2 + 3. Packaged example (examples/project_prestage56_utilidades): real Windows x64 PE via the
//    actual Driver, then real execution under Wine expecting exit code 0.
// ---------------------------------------------------------------------------------------

void testPackagedExample(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path project = std::filesystem::path(NSC_SOURCE_DIR) /
        "examples/project_prestage56_utilidades/PreStage56Utilidades.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Pre-Stage-56 utilities example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        "packaged Pre-Stage-56 example passes method-body ownership analysis");
    tests.expect(error.empty(), "packaged Pre-Stage-56 method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error), "packaged Pre-Stage-56 example lowers to OOP MIR");
    tests.expect(error.empty(), "packaged Pre-Stage-56 MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "packaged Pre-Stage-56 example lowers to LLVM (Thread/File/Directory/Random/NumericLimits/"
        "ScopedLock/MathUtil)");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "packaged Pre-Stage-56 example LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(),
        "Pre-Stage-56 example obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Pre-Stage-56 example applies Windows x64 data layout");

    const auto object = root / "nativesharp_prestage56_example.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Pre-Stage-56 example emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Pre-Stage-56 example object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Pre-Stage-56 example object machine is AMD64 / 0x8664");

    const auto exePath = root / "PreStage56Utilidades.exe";
    const std::string projectText = project.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode driverResult = driver.run(args, out, err);
    tests.expect(driverResult == ExitCode::Success,
        "Pre-Stage-56 example: Driver emits a real Windows x64 PE executable");
    tests.expect(err.str().empty(), "Pre-Stage-56 example: no Driver diagnostics (" + err.str() + ")");
    tests.expect(std::filesystem::is_regular_file(exePath), "Pre-Stage-56 example: .exe file exists on disk");

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- Pre-Stage-56 real-execution "
            "check skipped, all structural checks above still ran");
        return;
    }
    // File.Copy/Move/Directory.Move write into the CURRENT WORKING DIRECTORY (relative paths, same
    // convention as every File/Directory example since Stage 53.3) -- run the executable from
    // inside `root` (a fresh temp directory) so the exercise never touches, and never depends on,
    // any leftover state from a previous run.
    const auto workingDirectory = root / "run";
    std::error_code workingDirectoryError;
    std::filesystem::create_directories(workingDirectory, workingDirectoryError);
    const auto runExePath = workingDirectory / exePath.filename();
    std::error_code copyError;
    std::filesystem::copy_file(
        exePath, runExePath, std::filesystem::copy_options::overwrite_existing, copyError);
    tests.expect(!copyError, "Pre-Stage-56 example: executable copied into a clean working directory");

    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(
        wine, workingDirectory, runExePath, workingDirectory / "PreStage56Utilidades.log", exitCode, crashed);
    tests.expect(ran, "Wine could be invoked on the Pre-Stage-56 utilities executable");
    tests.expect(!crashed, "Pre-Stage-56 utilities executable does not crash under real execution");
    tests.expect(exitCode == 0,
        "Pre-Stage-56 utilities executable returns exit code 0 (every Thread.Sleep/File.Copy/"
        "File.Move/Directory.Move/Random/NumericLimits/ScopedLock/MathUtil runtime check in "
        "Main.ns passed; a nonzero code identifies exactly which check failed -- see "
        "examples/project_prestage56_utilidades/Main.ns for the numbering)");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_prestage56_utilities_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testNewCoreOperationsLowerToRuntimeCalls(tests, root);
    testPackagedExample(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
