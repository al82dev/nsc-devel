// Stage 53.2b -- "Environment (variables)": GetEnvironmentVariable / SetEnvironmentVariable.
//
// Covers, in increasing order of confidence, exactly like every other Stage 53 real-execution
// test file in this suite:
//   1. That the two Environment core operations (EnvironmentGetVariable/EnvironmentSetVariable)
//      are recognized, lower to LLVM, and emit a call to their exact runtime hook symbol -- a
//      minimal hand-written fixture, independent of the packaged example.
//   2. That the packaged example project (examples/project_stage53_2b) compiles end to end
//      through a real linked Windows x64 PE executable via the actual `nsc` Driver, AT BOTH
//      O0 and O2 -- this round's real bug (see Paso 6 of the paso-a-paso document) only showed
//      up at O2, where LLVM's LoopIdiomRecognize pass rewrites the hand-rolled byte-copy loop
//      in buildNullTerminatedCopy (WindowsRuntimeBuilder.cpp) into an @llvm.memcpy intrinsic
//      call, which needs "memcpy" in the linker's ucrtbase.dll import-symbol scan. Testing only
//      O0 (as every other Stage 53 test file does) would NOT have caught that regression, so
//      this file deliberately tests both.
//   3. REAL EXECUTION under Wine (when available, gracefully skipped otherwise), at BOTH
//      optimization levels: Main.ns returns 0 only if every one of its 8 runtime checks passed
//      (missing-variable empty-string behavior, a basic Set/Get round trip verified by both
//      length AND content, overwriting with a longer then a shorter value, a long variable
//      name, and Environment.Exit(0) itself as the final check).

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

// Same fallback order as every other Stage 52/53 real-execution test file in this repo -- see
// the identical helper's comment in Stage52_4OwnedCleanupRuntimeTests.cpp for why.
std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

bool runUnderWine(
    const std::string& wine,
    const std::filesystem::path& exePath,
    const std::filesystem::path& logPath,
    int& exitCode,
    bool& crashed)
{
    const std::string command = "WINEDEBUG=-all " + wine + " " + exePath.string() +
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
// 1. Minimal fixture: the two Environment-variable core operations lower to LLVM and each
//    emits a call to its exact runtime hook symbol name.
// ---------------------------------------------------------------------------------------

void testEnvironmentVariableCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "EnvironmentVariablesCore";
    writeProject(directory, "EnvironmentVariablesCore");
    writeFile(directory / "Environment.ns", R"NS(
namespace NativeSharp.Core
{
    public class Environment
    {
        public static string GetEnvironmentVariable(string name) { return ""; }
        public static void SetEnvironmentVariable(string name, string value) { }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core;

class Program
{
    public static string Run()
    {
        Environment.SetEnvironmentVariable("A", "B");
        return Environment.GetEnvironmentVariable("A");
    }
}
)NS");

    ProjectCompilation mir;
    std::string error;
    const bool ok = mir.compileProjectOopMir(directory / "EnvironmentVariablesCore.nsproj", error);
    tests.expect(ok && error.empty(), "Environment-variable core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Environment-variable core-operation fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Environment-variable core-operation fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("call ptr @__nativesharp_environment_get_variable") != std::string::npos,
        "GetEnvironmentVariable(...) lowers to a direct call to __nativesharp_environment_get_variable");
    tests.expect(llvm.find("call void @__nativesharp_environment_set_variable") != std::string::npos,
        "SetEnvironmentVariable(...) lowers to a direct call to __nativesharp_environment_set_variable");
}

// ---------------------------------------------------------------------------------------
// 2 + 3. Packaged example (examples/project_stage53_2b), tested at BOTH O0 and O2 -- see the
//    file-level comment above for why O2 specifically matters for this round.
// ---------------------------------------------------------------------------------------

void testPackagedExampleAtOptimizationLevel(
    TestContext& tests,
    const std::filesystem::path& root,
    nativesharp::llvm_backend::LlvmOptimizationLevel level,
    std::string_view levelFlag,
    std::string_view levelLabel)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage53_2b/Stage53_2bEnvironmentVariables.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 53.2b Environment-variables example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        std::string("packaged Stage 53.2b example (") + std::string(levelLabel) +
            ") passes method-body ownership analysis");
    tests.expect(error.empty(), "packaged Stage 53.2b method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error),
        std::string("packaged Stage 53.2b example (") + std::string(levelLabel) + ") lowers to OOP MIR");
    tests.expect(error.empty(), "packaged Stage 53.2b MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        std::string("packaged Stage 53.2b example (") + std::string(levelLabel) + ") lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "packaged Stage 53.2b example LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, targetError);
    tests.expect(target != nullptr && targetError.empty(),
        std::string("Stage 53.2b example (") + std::string(levelLabel) + ") obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 53.2b example applies Windows x64 data layout");

    const auto object = root / (std::string("nativesharp_stage53_2b_example_") + std::string(levelLabel) + ".obj");
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 53.2b example emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 53.2b example object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 53.2b example object machine is AMD64 / 0x8664");

    // Real linked executable via the actual Driver, at THIS level. This is the exact scenario
    // that caught the memcpy/ucrtbase bug: the structural checks above (using createWindowsX64
    // with whatever `level` this call was given) already exercise the requested optimization
    // level for the PROGRAM module, but the real bug only manifested through the Driver's own
    // linking path, because that is where the RUNTIME module also gets optimized at the same
    // level and where the ucrtbase.dll import-symbol scan (WindowsExecutableBuilder.cpp) runs.
    const auto exePath = root / (std::string("Stage53_2bEnvironmentVariables_") + std::string(levelLabel) + ".exe");
    const std::string projectText = project.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, levelFlag, "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode driverResult = driver.run(args, out, err);
    tests.expect(driverResult == ExitCode::Success,
        std::string("Stage 53.2b example (") + std::string(levelLabel) +
            "): Driver emits a real Windows x64 PE executable (" + err.str() + ")");
    tests.expect(err.str().empty(),
        std::string("Stage 53.2b example (") + std::string(levelLabel) + "): no Driver diagnostics");
    tests.expect(std::filesystem::is_regular_file(exePath),
        std::string("Stage 53.2b example (") + std::string(levelLabel) + "): .exe file exists on disk");

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string("Wine not available in this environment -- Stage 53.2b (") +
            std::string(levelLabel) + ") real-execution check skipped, all structural checks above still ran");
        return;
    }
    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(
        wine, exePath, root / (std::string("Stage53_2bEnvironmentVariables_") + std::string(levelLabel) + ".log"),
        exitCode, crashed);
    tests.expect(ran, std::string("Wine could be invoked on the Stage 53.2b (") + std::string(levelLabel) + ") executable");
    tests.expect(!crashed, std::string("Stage 53.2b (") + std::string(levelLabel) + ") executable does not crash under real execution");
    tests.expect(exitCode == 0,
        std::string("Stage 53.2b (") + std::string(levelLabel) +
            ") executable returns exit code 0 (all 8 GetEnvironmentVariable/SetEnvironmentVariable "
            "runtime checks passed AND Environment.Exit(0) itself terminated the process correctly; "
            "see examples/project_stage53_2b/Main.ns for the check numbering)");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage53_2b_environment_variables_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testEnvironmentVariableCoreOperationsLowerToRuntimeCalls(tests, root);
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "O0");
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, "-O2", "O2");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
