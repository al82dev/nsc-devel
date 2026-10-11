// Stage 53.3 "Filesystem/Paths", ronda 1 -- NativeSharp.Core.IO.Path (pure NativeSharp, zero
// new core operations) + NativeSharp.Core.IO.File.Exists/Delete + NativeSharp.Core.IO.
// Directory.Exists/CreateDirectory (four core operations). T46 moved both existence checks to
// GetFileAttributesW through an explicit UTF-8 -> UTF-16 path conversion; mutations retain their
// original Win32 contracts.
//
// Covers, in increasing order of confidence, exactly like every other Stage 53 real-execution
// test file in this suite:
//   1. That the four File/Directory core operations (FileExists/FileDelete/DirectoryExists/
//      DirectoryCreate) are recognized, lower to LLVM, and emit a call to their exact runtime
//      hook symbol -- a minimal hand-written fixture, independent of the packaged example. Path
//      does NOT need an equivalent fixture here: it is pure NativeSharp, the compiler does not
//      recognize it by name at all, so there is nothing core-operation-specific to check beyond
//      what the packaged example's real execution already verifies end to end.
//   2. That the packaged example project (examples/project_stage53_3) compiles end to end
//      through a real linked Windows x64 PE executable via the actual `nsc` Driver, AT BOTH O0
//      and O2 -- following the exact same "test both, not just O0" discipline Stage 53.2b
//      established, since these four operations reuse buildNullTerminatedCopy (the very helper
//      whose hand-rolled copy loop is what LLVM's LoopIdiomRecognize pass rewrote into an
//      @llvm.memcpy call at O2 last round). The ucrtbase.dll import-scan fix from 53.2b already
//      covers this class of bug, but this file verifies that empirically rather than assuming it.
//   3. REAL EXECUTION under Wine (when available, gracefully skipped otherwise), at BOTH
//      optimization levels: Main.ns returns 0 only if every one of its 20 runtime checks passed
//      (see examples/project_stage53_3/Main.ns for the full numbering). Unlike every earlier
//      Stage 53 real-execution test, THIS round's packaged example touches the real filesystem
//      (creates/deletes a real directory and a real file), so this file does something none of
//      the earlier ones needed: it runs Wine with its CURRENT WORKING DIRECTORY set to `root`
//      (this test's own dedicated scratch directory, wiped clean at the start and end of main())
//      rather than inheriting ctest's own working directory -- otherwise the checks that create/
//      delete real files/directories would scatter them wherever ctest happens to run from. It
//      also creates "nativesharp_stage53_3_existing_file.txt" in that same directory BEFORE
//      invoking Wine, because Stage 53.3 does not have File.WriteAllText yet (deliberately
//      deferred to a future 53.3b round, see File.ns) -- there is no way for the NativeSharp
//      program itself to create that file, so the C++ harness does it instead, exactly the same
//      role a test fixture's "arrange" step plays in any other testing framework.

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

std::string diagnosticMessages(const ProjectCompilation& compilation)
{
    std::string result;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (!result.empty()) result += " | ";
        result += diagnostic.message();
    }
    return result;
}

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

// Same fallback order as every other Stage 52/53 real-execution test file in this repo -- see
// the identical helper's comment in Stage52_4OwnedCleanupRuntimeTests.cpp for why.
std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

// Unlike every earlier Stage 53 real-execution helper, this one takes a `workingDirectory` and
// `cd`s into it before invoking Wine -- see the file-level comment above for why THIS round
// needs that (real file/directory creation, unlike Stage 53.1/53.2/53.2b which never touched the
// filesystem).
bool runUnderWine(
    const std::string& wine,
    const std::filesystem::path& workingDirectory,
    const std::filesystem::path& exePath,
    const std::filesystem::path& logPath,
    int& exitCode,
    bool& crashed)
{
    const std::string command = "cd " + workingDirectory.string() +
        " && WINEDEBUG=-all " + wine + " " + exePath.string() +
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
// 1. Minimal fixture: the four File/Directory core operations lower to LLVM and each emits a
//    call to its exact runtime hook symbol -- Path is NOT part of this fixture, see the
//    file-level comment above for why.
// ---------------------------------------------------------------------------------------

void testFileDirectoryCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "FileDirectoryCore";
    writeProject(directory, "FileDirectoryCore");
    writeFile(directory / "File.ns", R"NS(
namespace NativeSharp.Core.IO
{
    public class File
    {
        public static bool Exists(string path) { return false; }
        public static void Delete(string path) { }
    }
}
)NS");
    writeFile(directory / "Directory.ns", R"NS(
namespace NativeSharp.Core.IO
{
    public class Directory
    {
        public static bool Exists(string path) { return false; }
        public static void CreateDirectory(string path) { }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.IO;

class Program
{
    public static bool Run()
    {
        Directory.CreateDirectory("x");
        File.Delete("y");
        return File.Exists("y") || Directory.Exists("x");
    }
}
)NS");

    std::string error;
    ProjectCompilation mir;
    const bool ok = mir.compileProjectOopMir(directory / "FileDirectoryCore.nsproj", error);
    tests.expect(ok && error.empty(), "File/Directory core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "File/Directory core-operation fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "File/Directory core-operation fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("call i1 @__nativesharp_file_exists") != std::string::npos,
        "File.Exists(...) lowers to a direct call to __nativesharp_file_exists");
    tests.expect(llvm.find("call void @__nativesharp_file_delete") != std::string::npos,
        "File.Delete(...) lowers to a direct call to __nativesharp_file_delete");
    tests.expect(llvm.find("call i1 @__nativesharp_directory_exists") != std::string::npos,
        "Directory.Exists(...) lowers to a direct call to __nativesharp_directory_exists");
    tests.expect(llvm.find("call void @__nativesharp_directory_create") != std::string::npos,
        "Directory.CreateDirectory(...) lowers to a direct call to __nativesharp_directory_create");
}

// ---------------------------------------------------------------------------------------
// 2 + 3. Packaged example (examples/project_stage53_3), tested at BOTH O0 and O2 -- see the
//    file-level comment above for why O2 specifically matters for this round, and for why this
//    round's Wine invocation is different (working directory + a pre-existing file) from every
//    earlier Stage 53 real-execution test.
// ---------------------------------------------------------------------------------------

void testPackagedExampleAtOptimizationLevel(
    TestContext& tests,
    const std::filesystem::path& root,
    nativesharp::llvm_backend::LlvmOptimizationLevel level,
    std::string_view levelFlag,
    std::string_view levelLabel)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage53_3/Stage53_3FilesystemPaths.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 53.3 Filesystem/Paths example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    const bool bodiesOk = bodies.compileProjectMethodBodies(project, error);
    tests.expect(bodiesOk,
        std::string("packaged Stage 53.3 example (") + std::string(levelLabel) +
            ") passes method-body ownership analysis (" + diagnosticMessages(bodies) + ")");
    tests.expect(error.empty(), "packaged Stage 53.3 method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    const bool mirOk = mir.compileProjectOopMir(project, error);
    tests.expect(mirOk,
        std::string("packaged Stage 53.3 example (") + std::string(levelLabel) +
            ") lowers to OOP MIR (" + diagnosticMessages(mir) + ")");
    tests.expect(error.empty(), "packaged Stage 53.3 MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        std::string("packaged Stage 53.3 example (") + std::string(levelLabel) + ") lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "packaged Stage 53.3 example LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, targetError);
    tests.expect(target != nullptr && targetError.empty(),
        std::string("Stage 53.3 example (") + std::string(levelLabel) + ") obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 53.3 example applies Windows x64 data layout");

    const auto object = root / (std::string("nativesharp_stage53_3_example_") + std::string(levelLabel) + ".obj");
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 53.3 example emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 53.3 example object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 53.3 example object machine is AMD64 / 0x8664");

    // Real linked executable via the actual Driver, at THIS level -- same rationale as Stage
    // 53.2b's identical block: the structural checks above already exercise `level` for the
    // PROGRAM module, but the real bug that discipline exists to catch (see the file-level
    // comment) only shows up through the Driver's own linking path.
    const auto exePath = root / (std::string("Stage53_3FilesystemPaths_") + std::string(levelLabel) + ".exe");
    const std::string projectText = project.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, levelFlag, "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode driverResult = driver.run(args, out, err);
    tests.expect(driverResult == ExitCode::Success,
        std::string("Stage 53.3 example (") + std::string(levelLabel) +
            "): Driver emits a real Windows x64 PE executable (" + err.str() + ")");
    tests.expect(err.str().empty(),
        std::string("Stage 53.3 example (") + std::string(levelLabel) + "): no Driver diagnostics");
    tests.expect(std::filesystem::is_regular_file(exePath),
        std::string("Stage 53.3 example (") + std::string(levelLabel) + "): .exe file exists on disk");

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string("Wine not available in this environment -- Stage 53.3 (") +
            std::string(levelLabel) + ") real-execution check skipped, all structural checks above still ran");
        return;
    }

    // A dedicated, EMPTY, WRITABLE working directory for THIS optimization level's run -- Main.ns
    // creates a real directory and expects a real pre-existing file to be there (see the
    // file-level comment above), so O0's run and O2's run each get their own clean slate rather
    // than sharing one (and possibly tripping over each other's leftover
    // "nativesharp_stage53_3_new_dir", which the program creates but this round has no
    // Directory.Delete to clean back up -- see Directory.ns for why that is out of scope too).
    const auto workingDirectory = root / (std::string("run_") + std::string(levelLabel));
    std::error_code cleanupError;
    std::filesystem::remove_all(workingDirectory, cleanupError);
    std::filesystem::create_directories(workingDirectory);
    writeFile(workingDirectory / "nativesharp_stage53_3_existing_file.txt", "");

    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(
        wine, workingDirectory, exePath,
        root / (std::string("Stage53_3FilesystemPaths_") + std::string(levelLabel) + ".log"),
        exitCode, crashed);
    tests.expect(ran, std::string("Wine could be invoked on the Stage 53.3 (") + std::string(levelLabel) + ") executable");
    tests.expect(!crashed, std::string("Stage 53.3 (") + std::string(levelLabel) + ") executable does not crash under real execution");
    tests.expect(exitCode == 0,
        std::string("Stage 53.3 (") + std::string(levelLabel) +
            ") executable returns exit code 0 (all 20 Path/File/Directory runtime checks passed; "
            "see examples/project_stage53_3/Main.ns for the check numbering)");

    // Independent confirmation, from the Linux side, that Directory.CreateDirectory/File.Delete
    // really did touch the real filesystem through Wine -- not just that the NativeSharp program
    // itself believed they worked (checks 17/20 already assert that from inside the program via
    // Directory.Exists/File.Exists, but this is a second, independent witness).
    tests.expect(std::filesystem::is_directory(workingDirectory / "nativesharp_stage53_3_new_dir"),
        std::string("Stage 53.3 (") + std::string(levelLabel) +
            "): Directory.CreateDirectory really created a directory on disk (confirmed from the Linux side)");
    tests.expect(!std::filesystem::exists(workingDirectory / "nativesharp_stage53_3_existing_file.txt"),
        std::string("Stage 53.3 (") + std::string(levelLabel) +
            "): File.Delete really removed the file from disk (confirmed from the Linux side)");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage53_3_filesystem_paths_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testFileDirectoryCoreOperationsLowerToRuntimeCalls(tests, root);
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "O0");
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, "-O2", "O2");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
