// Stage 55.1 -- "Process".
//
// Covers, in increasing order of confidence, exactly like every other Stage 53/54/55 real-
// execution test file in this suite:
//   1. That the six Process core operations (ProcessGetCurrentProcessIdRaw/
//      ProcessGetWorkingSetSizeRaw/ProcessGetKernelTimeRaw/ProcessGetUserTimeRaw/
//      ProcessGetPriorityClassRaw/ProcessSetPriorityClassRaw) are recognized, lower to LLVM, and
//      emit a call to their exact runtime hook symbol -- a minimal hand-written fixture,
//      independent of the packaged example, so a future change that breaks core-operation
//      recognition fails HERE with a precise message instead of only failing deep inside the
//      packaged example.
//   2. That the packaged example project (examples/project_stage55_1) -- exercising
//      Process.CurrentProcessId/WorkingSetSize/KernelTime/UserTime/PriorityClass, the whole
//      NativeSharp-level surface built on top of those six core operations -- compiles end to
//      end through a real linked Windows x64 PE executable via the actual `nsc` Driver
//      (--emit-project-oop-exe), not a hand-rolled subset of it.
//   3. REAL EXECUTION under Wine (when available, gracefully skipped otherwise): Main.ns returns
//      0 only if every one of its runtime checks passes, and a distinct nonzero code identifying
//      exactly which check failed otherwise, so a regression here is immediately diagnosable
//      from the exit code alone.

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
// 1. Minimal fixture: the six Process core operations lower to LLVM and each emits a call to
//    its exact runtime hook symbol name.
// ---------------------------------------------------------------------------------------

void testProcessCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "ProcessCore";
    writeProject(directory, "ProcessCore");
    writeFile(directory / "Process.ns", R"NS(
namespace NativeSharp.Core.Diagnostics
{
    public class Process
    {
        public static long GetCurrentProcessIdRaw() { return 0; }
        public static long GetWorkingSetSizeRaw() { return 0; }
        public static long GetKernelTimeRaw() { return 0; }
        public static long GetUserTimeRaw() { return 0; }
        public static long GetPriorityClassRaw() { return 0; }
        public static void SetPriorityClassRaw(int priorityClass) { }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Diagnostics;

class Program
{
    public static long Run()
    {
        long pid = Process.GetCurrentProcessIdRaw();
        long workingSet = Process.GetWorkingSetSizeRaw();
        long kernelTime = Process.GetKernelTimeRaw();
        long userTime = Process.GetUserTimeRaw();
        long priority = Process.GetPriorityClassRaw();
        Process.SetPriorityClassRaw(32);
        return pid + workingSet + kernelTime + userTime + priority;
    }
}
)NS");

    ProjectCompilation mir;
    std::string error;
    const bool ok = mir.compileProjectOopMir(directory / "ProcessCore.nsproj", error);
    tests.expect(ok && error.empty(), "Process core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Process core-operation fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Process core-operation fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("call i64 @__nativesharp_process_get_current_process_id") != std::string::npos,
        "GetCurrentProcessIdRaw() lowers to a direct call to __nativesharp_process_get_current_process_id");
    tests.expect(llvm.find("call i64 @__nativesharp_process_get_working_set_size") != std::string::npos,
        "GetWorkingSetSizeRaw() lowers to a direct call to __nativesharp_process_get_working_set_size");
    tests.expect(llvm.find("call i64 @__nativesharp_process_get_kernel_time") != std::string::npos,
        "GetKernelTimeRaw() lowers to a direct call to __nativesharp_process_get_kernel_time");
    tests.expect(llvm.find("call i64 @__nativesharp_process_get_user_time") != std::string::npos,
        "GetUserTimeRaw() lowers to a direct call to __nativesharp_process_get_user_time");
    tests.expect(llvm.find("call i64 @__nativesharp_process_get_priority_class") != std::string::npos,
        "GetPriorityClassRaw() lowers to a direct call to __nativesharp_process_get_priority_class");
    tests.expect(llvm.find("call void @__nativesharp_process_set_priority_class") != std::string::npos,
        "SetPriorityClassRaw(int) lowers to a direct call to __nativesharp_process_set_priority_class");
}

// ---------------------------------------------------------------------------------------
// 2 + 3. Packaged example (examples/project_stage55_1): real Windows x64 PE via the actual
//    Driver, then real execution under Wine expecting exit code 0.
// ---------------------------------------------------------------------------------------

void testPackagedExample(TestContext& tests, const std::filesystem::path& root)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage55_1/Stage55_1Process.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 55.1 Process example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        "packaged Stage 55.1 example passes method-body ownership analysis");
    tests.expect(error.empty(), "packaged Stage 55.1 method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error), "packaged Stage 55.1 example lowers to OOP MIR");
    tests.expect(error.empty(), "packaged Stage 55.1 MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        "packaged Stage 55.1 example lowers to LLVM (Process/TimeSpan)");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "packaged Stage 55.1 example LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(
        nativesharp::llvm_backend::LlvmOptimizationLevel::O2, targetError);
    tests.expect(target != nullptr && targetError.empty(), "Stage 55.1 example obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 55.1 example applies Windows x64 data layout");

    const auto object = root / "nativesharp_stage55_1_example.obj";
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 55.1 example emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 55.1 example object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 55.1 example object machine is AMD64 / 0x8664");

    const auto exePath = root / "Stage55_1Process.exe";
    const std::string projectText = project.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode driverResult = driver.run(args, out, err);
    tests.expect(driverResult == ExitCode::Success, "Stage 55.1 example: Driver emits a real Windows x64 PE executable");
    tests.expect(err.str().empty(), "Stage 55.1 example: no Driver diagnostics (" + err.str() + ")");
    tests.expect(std::filesystem::is_regular_file(exePath), "Stage 55.1 example: .exe file exists on disk");

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- Stage 55.1 real-execution check "
            "skipped, all structural checks above still ran");
        return;
    }
    int exitCode = 0;
    bool crashed = false;
    const bool ran = runUnderWine(wine, exePath, root / "Stage55_1Process.log", exitCode, crashed);
    tests.expect(ran, "Wine could be invoked on the Stage 55.1 Process executable");
    tests.expect(!crashed, "Stage 55.1 Process executable does not crash under real execution");
    tests.expect(exitCode == 0,
        "Stage 55.1 Process executable returns exit code 0 (every CurrentProcessId/WorkingSetSize/"
        "KernelTime/UserTime/PriorityClass runtime check in Main.ns passed; a nonzero code "
        "identifies exactly which check failed -- see examples/project_stage55_1/Main.ns for the "
        "numbering)");
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage55_1_process_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testProcessCoreOperationsLowerToRuntimeCalls(tests, root);
    testPackagedExample(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
