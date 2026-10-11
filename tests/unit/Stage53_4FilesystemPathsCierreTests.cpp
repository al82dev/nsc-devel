// Stage 53.4 "Filesystem/Paths (cierre)" -- Directory.Delete y Path.GetFullPath, las dos piezas
// "casi gratis" identificadas al cerrar Stage 53.3b: Directory.Delete es un calco letra por letra
// de File.Delete (misma forma: un string, void, BOOL descartado, solo cambia RemoveDirectoryA por
// DeleteFileA); Path.GetFullPath es la PRIMERA operacion nucleo de NativeSharp.Core.IO.Path (todo
// lo demas de esa clase sigue siendo NativeSharp puro), y reutiliza el mismo patron de dos
// llamadas "pedir tamaño, despues llenar el buffer" que Environment.GetEnvironmentVariable
// establecio en 53.2b.
//
// Covers, in increasing order of confidence, exactly like every other Stage 53 real-execution
// test file in this suite:
//   1. That the two core operations (DirectoryDelete/PathGetFullPath) are recognized, lower to
//      LLVM, and emit a call to their exact runtime hook symbol -- a minimal hand-written
//      fixture, independent of the packaged example.
//   2. That the packaged example project (examples/project_stage53_4) compiles end to end
//      through a real linked Windows x64 PE executable via the actual `nsc` Driver, AT BOTH O0
//      and O2 -- same discipline every Stage 53 round has followed since 53.2b.
//   3. REAL EXECUTION under Wine (when available, gracefully skipped otherwise), at BOTH
//      optimization levels: Main.ns returns 0 only if every one of its 14 runtime checks passed
//      (see examples/project_stage53_4/Main.ns for the full numbering). Same dedicated, empty
//      Wine working-directory pattern every filesystem-touching Stage 53 round has used since
//      53.3 -- this round's Main.ns creates AND deletes its own directories/files, so the
//      independent Linux-side confirmation here is different from 53.3b's: instead of checking
//      leftover content, it confirms the working directory is COMPLETELY EMPTY after the run --
//      a genuine independent witness (not just NativeSharp's own exit code) that Directory.Delete
//      and File.Delete really removed what they claimed to remove.

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

// Same fallback order as every other Stage 52/53 real-execution test file in this repo.
std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

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
// 1. Minimal fixture: the two core operations lower to LLVM and each emits a call to its
//    exact runtime hook symbol.
// ---------------------------------------------------------------------------------------

void testDirectoryPathCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "DirectoryPathCore";
    writeProject(directory, "DirectoryPathCore");
    writeFile(directory / "Directory.ns", R"NS(
namespace NativeSharp.Core.IO
{
    public class Directory
    {
        public static void Delete(string path) { }
    }
}
)NS");
    writeFile(directory / "Path.ns", R"NS(
namespace NativeSharp.Core.IO
{
    public class Path
    {
        public static string GetFullPath(string path) { return ""; }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.IO;

class Program
{
    public static string Run()
    {
        Directory.Delete("a");
        return Path.GetFullPath("a");
    }
}
)NS");

    std::string error;
    ProjectCompilation mir;
    const bool ok = mir.compileProjectOopMir(directory / "DirectoryPathCore.nsproj", error);
    tests.expect(ok && error.empty(), "Directory/Path content-core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Directory/Path fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Directory/Path fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("call void @__nativesharp_directory_delete") != std::string::npos,
        "Directory.Delete(...) lowers to a direct call to __nativesharp_directory_delete");
    tests.expect(llvm.find("call ptr @__nativesharp_path_get_full_path") != std::string::npos,
        "Path.GetFullPath(...) lowers to a direct call to __nativesharp_path_get_full_path");
}

// ---------------------------------------------------------------------------------------
// 2 + 3. Packaged example (examples/project_stage53_4), tested at BOTH O0 and O2, with a
//    dedicated Wine working directory.
// ---------------------------------------------------------------------------------------

void testPackagedExampleAtOptimizationLevel(
    TestContext& tests,
    const std::filesystem::path& root,
    nativesharp::llvm_backend::LlvmOptimizationLevel level,
    std::string_view levelFlag,
    std::string_view levelLabel)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage53_4/Stage53_4FilesystemPathsCierre.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 53.4 Filesystem-Paths-cierre example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        std::string("packaged Stage 53.4 example (") + std::string(levelLabel) +
            ") passes method-body ownership analysis");
    tests.expect(error.empty(), "packaged Stage 53.4 method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error),
        std::string("packaged Stage 53.4 example (") + std::string(levelLabel) + ") lowers to OOP MIR");
    tests.expect(error.empty(), "packaged Stage 53.4 MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        std::string("packaged Stage 53.4 example (") + std::string(levelLabel) + ") lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "packaged Stage 53.4 example LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, targetError);
    tests.expect(target != nullptr && targetError.empty(),
        std::string("Stage 53.4 example (") + std::string(levelLabel) + ") obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 53.4 example applies Windows x64 data layout");

    const auto object = root / (std::string("nativesharp_stage53_4_example_") + std::string(levelLabel) + ".obj");
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 53.4 example emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 53.4 example object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 53.4 example object machine is AMD64 / 0x8664");

    const auto exePath = root / (std::string("Stage53_4FilesystemPathsCierre_") + std::string(levelLabel) + ".exe");
    const std::string projectText = project.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, levelFlag, "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode driverResult = driver.run(args, out, err);
    tests.expect(driverResult == ExitCode::Success,
        std::string("Stage 53.4 example (") + std::string(levelLabel) +
            "): Driver emits a real Windows x64 PE executable (" + err.str() + ")");
    tests.expect(err.str().empty(),
        std::string("Stage 53.4 example (") + std::string(levelLabel) + "): no Driver diagnostics");
    tests.expect(std::filesystem::is_regular_file(exePath),
        std::string("Stage 53.4 example (") + std::string(levelLabel) + "): .exe file exists on disk");

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string("Wine not available in this environment -- Stage 53.4 (") +
            std::string(levelLabel) + ") real-execution check skipped, all structural checks above still ran");
        return;
    }

    // Dedicated, empty, writable working directory for THIS level's run -- Main.ns creates AND
    // deletes its own directories/files, so this is also what the post-run emptiness check below
    // inspects.
    const auto workingDirectory = root / (std::string("run_") + std::string(levelLabel));
    std::error_code cleanupError;
    std::filesystem::remove_all(workingDirectory, cleanupError);
    std::filesystem::create_directories(workingDirectory);

    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(
        wine, workingDirectory, exePath,
        root / (std::string("Stage53_4FilesystemPathsCierre_") + std::string(levelLabel) + ".log"),
        exitCode, crashed);
    tests.expect(ran, std::string("Wine could be invoked on the Stage 53.4 (") + std::string(levelLabel) + ") executable");
    tests.expect(!crashed, std::string("Stage 53.4 (") + std::string(levelLabel) + ") executable does not crash under real execution");
    tests.expect(exitCode == 0,
        std::string("Stage 53.4 (") + std::string(levelLabel) +
            ") executable returns exit code 0 (all 14 Directory.Delete/Path.GetFullPath runtime "
            "checks passed; see examples/project_stage53_4/Main.ns for the check numbering)");

    // Independent confirmation, from the Linux side, that Directory.Delete/File.Delete really
    // removed everything Main.ns created: the working directory must be completely empty after
    // the run. Unlike 53.3b (which left one file behind to check its final content), this round's
    // program is a create-then-delete cycle end to end, so "the directory ended up empty" IS the
    // independent witness that deletion genuinely happened on disk, not just that NativeSharp's
    // own exit code claims it did.
    std::size_t leftoverCount = 0;
    std::error_code listError;
    for (const auto& entry : std::filesystem::directory_iterator(workingDirectory, listError))
    {
        (void)entry;
        ++leftoverCount;
    }
    tests.expect(!listError && leftoverCount == 0,
        std::string("Stage 53.4 (") + std::string(levelLabel) +
            "): working directory is completely empty after the run, confirmed from the Linux side "
            "(Directory.Delete/File.Delete really removed everything Main.ns created)");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage53_4_filesystem_paths_cierre_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testDirectoryPathCoreOperationsLowerToRuntimeCalls(tests, root);
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "O0");
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, "-O2", "O2");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
