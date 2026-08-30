// Stage 53.3b "Filesystem/Paths (contenido de archivo)" -- File.ReadAllText/WriteAllText/
// AppendAllText, la pieza que Stage 53.3's primera ronda dejo deliberadamente afuera (ver
// File.ns): CreateFileA + GetFileSizeEx + ReadFile/WriteFile + CloseHandle, una secuencia de
// Win32 mas larga que las operaciones de una sola llamada que Stage 53 venia usando hasta ahora.
//
// Covers, in increasing order of confidence, exactly like every other Stage 53 real-execution
// test file in this suite:
//   1. That the three core operations (FileReadAllText/FileWriteAllText/FileAppendAllText) are
//      recognized, lower to LLVM, and emit a call to their exact runtime hook symbol -- a
//      minimal hand-written fixture, independent of the packaged example.
//   2. That the packaged example project (examples/project_stage53_3b) compiles end to end
//      through a real linked Windows x64 PE executable via the actual `nsc` Driver, AT BOTH O0
//      and O2 -- same discipline every Stage 53 round has followed since 53.2b, since these
//      three operations still reuse buildNullTerminatedCopy for their PATH argument (their
//      CONTENT argument does not need it -- WriteFile takes an explicit byte buffer + count, see
//      the file-level comment in WindowsRuntimeBuilder.cpp's buildFileWriteAllText for why).
//   3. REAL EXECUTION under Wine (when available, gracefully skipped otherwise), at BOTH
//      optimization levels: Main.ns returns 0 only if every one of its 12 runtime checks passed
//      (see examples/project_stage53_3b/Main.ns for the full numbering). Like Stage 53.3's own
//      test, this round's packaged example touches the real filesystem (creates and appends to
//      real files with real content), so this file runs Wine with its CURRENT WORKING DIRECTORY
//      set to a dedicated, empty scratch directory -- otherwise the files this round's checks
//      create would scatter wherever ctest happens to run from. Unlike Stage 53.3's test, this
//      round's Main.ns does NOT need any file pre-created by the harness before running (every
//      file it reads, it also wrote itself first, since ReadAllText/WriteAllText/AppendAllText
//      together are now able to create the fixtures FileExists/FileDelete could only check for
//      in the previous round).

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

// Same "cd into a dedicated working directory first" pattern Stage53_3FilesystemPathsTests.cpp
// introduced -- see the file-level comment above for why this round needs it too.
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
// 1. Minimal fixture: the three File core operations lower to LLVM and each emits a call to
//    its exact runtime hook symbol.
// ---------------------------------------------------------------------------------------

void testFileContentCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "FileContentCore";
    writeProject(directory, "FileContentCore");
    writeFile(directory / "File.ns", R"NS(
namespace NativeSharp.Core.IO
{
    public class File
    {
        public static string ReadAllText(string path) { return ""; }
        public static void WriteAllText(string path, string content) { }
        public static void AppendAllText(string path, string content) { }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.IO;

class Program
{
    public static string Run()
    {
        File.WriteAllText("a", "x");
        File.AppendAllText("a", "y");
        return File.ReadAllText("a");
    }
}
)NS");

    std::string error;
    ProjectCompilation mir;
    const bool ok = mir.compileProjectOopMir(directory / "FileContentCore.nsproj", error);
    tests.expect(ok && error.empty(), "File content-core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "File content-core-operation fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "File content-core-operation fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("call ptr @__nativesharp_file_read_all_text") != std::string::npos,
        "File.ReadAllText(...) lowers to a direct call to __nativesharp_file_read_all_text");
    tests.expect(llvm.find("call void @__nativesharp_file_write_all_text") != std::string::npos,
        "File.WriteAllText(...) lowers to a direct call to __nativesharp_file_write_all_text");
    tests.expect(llvm.find("call void @__nativesharp_file_append_all_text") != std::string::npos,
        "File.AppendAllText(...) lowers to a direct call to __nativesharp_file_append_all_text");
}

// ---------------------------------------------------------------------------------------
// 2 + 3. Packaged example (examples/project_stage53_3b), tested at BOTH O0 and O2, with a
//    dedicated Wine working directory and a fixture that this round's own Main.ns creates for
//    itself (no pre-existing file needed from the harness, unlike Stage 53.3's own test).
// ---------------------------------------------------------------------------------------

void testPackagedExampleAtOptimizationLevel(
    TestContext& tests,
    const std::filesystem::path& root,
    nativesharp::llvm_backend::LlvmOptimizationLevel level,
    std::string_view levelFlag,
    std::string_view levelLabel)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage53_3b/Stage53_3bFilesystemPathsContent.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 53.3b Filesystem-content example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        std::string("packaged Stage 53.3b example (") + std::string(levelLabel) +
            ") passes method-body ownership analysis");
    tests.expect(error.empty(), "packaged Stage 53.3b method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error),
        std::string("packaged Stage 53.3b example (") + std::string(levelLabel) + ") lowers to OOP MIR");
    tests.expect(error.empty(), "packaged Stage 53.3b MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        std::string("packaged Stage 53.3b example (") + std::string(levelLabel) + ") lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "packaged Stage 53.3b example LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, targetError);
    tests.expect(target != nullptr && targetError.empty(),
        std::string("Stage 53.3b example (") + std::string(levelLabel) + ") obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 53.3b example applies Windows x64 data layout");

    const auto object = root / (std::string("nativesharp_stage53_3b_example_") + std::string(levelLabel) + ".obj");
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 53.3b example emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 53.3b example object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 53.3b example object machine is AMD64 / 0x8664");

    const auto exePath = root / (std::string("Stage53_3bFilesystemPathsContent_") + std::string(levelLabel) + ".exe");
    const std::string projectText = project.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, levelFlag, "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode driverResult = driver.run(args, out, err);
    tests.expect(driverResult == ExitCode::Success,
        std::string("Stage 53.3b example (") + std::string(levelLabel) +
            "): Driver emits a real Windows x64 PE executable (" + err.str() + ")");
    tests.expect(err.str().empty(),
        std::string("Stage 53.3b example (") + std::string(levelLabel) + "): no Driver diagnostics");
    tests.expect(std::filesystem::is_regular_file(exePath),
        std::string("Stage 53.3b example (") + std::string(levelLabel) + "): .exe file exists on disk");

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string("Wine not available in this environment -- Stage 53.3b (") +
            std::string(levelLabel) + ") real-execution check skipped, all structural checks above still ran");
        return;
    }

    // Dedicated, empty, writable working directory for THIS level's run -- Main.ns creates and
    // appends to real files, and this round's checks do not need any pre-existing fixture (every
    // file it reads, it wrote itself first).
    const auto workingDirectory = root / (std::string("run_") + std::string(levelLabel));
    std::error_code cleanupError;
    std::filesystem::remove_all(workingDirectory, cleanupError);
    std::filesystem::create_directories(workingDirectory);

    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(
        wine, workingDirectory, exePath,
        root / (std::string("Stage53_3bFilesystemPathsContent_") + std::string(levelLabel) + ".log"),
        exitCode, crashed);
    tests.expect(ran, std::string("Wine could be invoked on the Stage 53.3b (") + std::string(levelLabel) + ") executable");
    tests.expect(!crashed, std::string("Stage 53.3b (") + std::string(levelLabel) + ") executable does not crash under real execution");
    tests.expect(exitCode == 0,
        std::string("Stage 53.3b (") + std::string(levelLabel) +
            ") executable returns exit code 0 (all 12 ReadAllText/WriteAllText/AppendAllText runtime "
            "checks passed; see examples/project_stage53_3b/Main.ns for the check numbering)");

    // Independent confirmation, from the Linux side, that the append really appended (not
    // overwrote) and that the final content matches exactly what the NativeSharp program itself
    // already asserted via ReadAllText/.Equals -- a second, independent witness of the same
    // real filesystem effect.
    std::ifstream contentStream(workingDirectory / "nativesharp_stage53_3b_b.txt", std::ios::binary);
    std::ostringstream contentText;
    contentText << contentStream.rdbuf();
    tests.expect(contentText.str() == "primerosegundo",
        std::string("Stage 53.3b (") + std::string(levelLabel) +
            "): AppendAllText really appended on disk, confirmed from the Linux side (\"primerosegundo\", not \"segundo\")");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage53_3b_filesystem_paths_content_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testFileContentCoreOperationsLowerToRuntimeCalls(tests, root);
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "O0");
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, "-O2", "O2");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
