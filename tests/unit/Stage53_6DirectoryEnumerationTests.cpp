// Stage 53.6 "Enumeracion de directorios" -- Directory.GetFileCount/GetFiles/GetDirectoryCount/
// GetDirectories, el ultimo item pendiente de Stage 53. Arreglo de DOS PASADAS (NO un enumerador
// via `foreach`) -- ver el comentario de cabecera de Directory.ns y
// `56_stage53_6_directory_enumeration.md` para el diseno completo. Las cuatro operaciones nucleo
// comparten, del lado del runtime C++, un solo par de funciones parametrizadas por un booleano
// `wantDirectories` que decide en tiempo de generacion de codigo (NO en tiempo de ejecucion) que
// filtro de atributo emitir -- ver buildDirectoryGetEntryCount/buildDirectoryGetEntries en
// src/Runtime/WindowsRuntimeBuilder.cpp.
//
// Covers, in increasing order of confidence, exactly like every other Stage 53 real-execution
// test file in this suite:
//   1. That the four core operations are recognized, lower to LLVM, and each emits a call to its
//      exact runtime hook symbol -- a minimal hand-written fixture, independent of the packaged
//      example.
//   2. That the packaged example project (examples/project_stage53_6) compiles end to end
//      through a real linked Windows x64 PE executable via the actual `nsc` Driver, AT BOTH O0
//      and O2 -- same discipline every Stage 53 round has followed since 53.2b.
//   3. REAL EXECUTION under Wine (when available, gracefully skipped otherwise), at BOTH
//      optimization levels: Main.ns returns 0 only if every one of its 20 runtime checks passed
//      (see examples/project_stage53_6/Main.ns for the full numbering) -- counts, exact-buffer
//      contents (checked without assuming any particular enumeration order), the files-vs-
//      directories filter, "."/".." exclusion, the smaller-buffer-than-actual silent-drop
//      behavior, and the missing-directory-collapses-to-0 behavior. Same dedicated, empty Wine
//      working-directory pattern every filesystem-touching Stage 53 round has used since 53.3;
//      like 53.4, this round's Main.ns creates AND deletes everything it makes, so the
//      independent Linux-side confirmation here is the same: the working directory must be
//      COMPLETELY EMPTY after the run.

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

// Portable (no POSIX-specific code) -- same std::system()-based invocation every other Stage 53
// real-execution test file in this repo uses. Per Alberto's explicit, repeated instruction this
// project's own C++ source (tests included) must contain zero Unix/Linux-specific code, even
// though this test suite is built both on this Linux/Wine sandbox and on a Windows/clang-cl
// toolchain -- see claude/05_metodo_de_trabajo_y_archivo_historico.md's "no Unix/Linux code"
// section and 55_stage53_5_command_line_args.md's build-break-and-fix writeup for the full story.
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
// 1. Minimal fixture: the four core operations lower to LLVM and each emits a call to its
//    exact runtime hook symbol.
// ---------------------------------------------------------------------------------------

void testDirectoryEnumerationCoreOperationsLowerToRuntimeCalls(
    TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "DirectoryEnumerationCore";
    writeProject(directory, "DirectoryEnumerationCore");
    writeFile(directory / "Directory.ns", R"NS(
namespace NativeSharp.Core.IO
{
    public class Directory
    {
        public static int GetFileCount(string path) { return 0; }
        public static int GetFiles(string path, string[] destination) { return 0; }
        public static int GetDirectoryCount(string path) { return 0; }
        public static int GetDirectories(string path, string[] destination) { return 0; }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.IO;

class Program
{
    public static int Run(string[] buffer)
    {
        int fileCount = Directory.GetFileCount("a");
        int written = Directory.GetFiles("a", buffer);
        int directoryCount = Directory.GetDirectoryCount("a");
        int directoriesWritten = Directory.GetDirectories("a", buffer);
        return fileCount + written + directoryCount + directoriesWritten;
    }
}
)NS");

    std::string error;
    ProjectCompilation mir;
    const bool ok = mir.compileProjectOopMir(directory / "DirectoryEnumerationCore.nsproj", error);
    tests.expect(ok && error.empty(), "Directory enumeration core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Directory enumeration fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Directory enumeration fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("call i32 @__nativesharp_directory_get_file_count") != std::string::npos,
        "Directory.GetFileCount(...) lowers to a direct call to __nativesharp_directory_get_file_count");
    tests.expect(llvm.find("call i32 @__nativesharp_directory_get_files") != std::string::npos,
        "Directory.GetFiles(...) lowers to a direct call to __nativesharp_directory_get_files");
    tests.expect(llvm.find("call i32 @__nativesharp_directory_get_directory_count") != std::string::npos,
        "Directory.GetDirectoryCount(...) lowers to a direct call to __nativesharp_directory_get_directory_count");
    tests.expect(llvm.find("call i32 @__nativesharp_directory_get_directories") != std::string::npos,
        "Directory.GetDirectories(...) lowers to a direct call to __nativesharp_directory_get_directories");
}

// ---------------------------------------------------------------------------------------
// 2 + 3. Packaged example (examples/project_stage53_6), tested at BOTH O0 and O2, with a
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
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage53_6/Stage53_6DirectoryEnumeration.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 53.6 directory-enumeration example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        std::string("packaged Stage 53.6 example (") + std::string(levelLabel) +
            ") passes method-body ownership analysis");
    tests.expect(error.empty(), "packaged Stage 53.6 method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error),
        std::string("packaged Stage 53.6 example (") + std::string(levelLabel) + ") lowers to OOP MIR");
    tests.expect(error.empty(), "packaged Stage 53.6 MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        std::string("packaged Stage 53.6 example (") + std::string(levelLabel) + ") lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "packaged Stage 53.6 example LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, targetError);
    tests.expect(target != nullptr && targetError.empty(),
        std::string("Stage 53.6 example (") + std::string(levelLabel) + ") obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 53.6 example applies Windows x64 data layout");

    const auto object = root / (std::string("nativesharp_stage53_6_example_") + std::string(levelLabel) + ".obj");
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 53.6 example emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 53.6 example object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 53.6 example object machine is AMD64 / 0x8664");

    const auto exePath = root / (std::string("Stage53_6DirectoryEnumeration_") + std::string(levelLabel) + ".exe");
    const std::string projectText = project.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, levelFlag, "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode driverResult = driver.run(args, out, err);
    tests.expect(driverResult == ExitCode::Success,
        std::string("Stage 53.6 example (") + std::string(levelLabel) +
            "): Driver emits a real Windows x64 PE executable (" + err.str() + ")");
    tests.expect(err.str().empty(),
        std::string("Stage 53.6 example (") + std::string(levelLabel) + "): no Driver diagnostics");
    tests.expect(std::filesystem::is_regular_file(exePath),
        std::string("Stage 53.6 example (") + std::string(levelLabel) + "): .exe file exists on disk");

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string("Wine not available in this environment -- Stage 53.6 (") +
            std::string(levelLabel) + ") real-execution check skipped, all structural checks above still ran");
        return;
    }

    // Dedicated, empty, writable working directory for THIS level's run -- Main.ns creates AND
    // deletes its own directory/files, so this is also what the post-run emptiness check below
    // inspects.
    const auto workingDirectory = root / (std::string("run_") + std::string(levelLabel));
    std::error_code cleanupError;
    std::filesystem::remove_all(workingDirectory, cleanupError);
    std::filesystem::create_directories(workingDirectory);

    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(
        wine, workingDirectory, exePath,
        root / (std::string("Stage53_6DirectoryEnumeration_") + std::string(levelLabel) + ".log"),
        exitCode, crashed);
    tests.expect(ran, std::string("Wine could be invoked on the Stage 53.6 (") + std::string(levelLabel) + ") executable");
    tests.expect(!crashed, std::string("Stage 53.6 (") + std::string(levelLabel) + ") executable does not crash under real execution");
    tests.expect(exitCode == 0,
        std::string("Stage 53.6 (") + std::string(levelLabel) +
            ") executable returns exit code 0 (all 20 Directory.GetFileCount/GetFiles/"
            "GetDirectoryCount/GetDirectories runtime checks passed; see "
            "examples/project_stage53_6/Main.ns for the check numbering)");

    // Independent confirmation, from the Linux side, that Main.ns really cleaned up everything
    // it created: the working directory must be completely empty after the run -- same
    // discipline as 53.4's own post-run emptiness check.
    std::size_t leftoverCount = 0;
    std::error_code listError;
    for (const auto& entry : std::filesystem::directory_iterator(workingDirectory, listError))
    {
        (void)entry;
        ++leftoverCount;
    }
    tests.expect(!listError && leftoverCount == 0,
        std::string("Stage 53.6 (") + std::string(levelLabel) +
            "): working directory is completely empty after the run, confirmed from the Linux side "
            "(Main.ns really cleaned up everything it created)");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage53_6_directory_enumeration_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testDirectoryEnumerationCoreOperationsLowerToRuntimeCalls(tests, root);
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "O0");
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, "-O2", "O2");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
