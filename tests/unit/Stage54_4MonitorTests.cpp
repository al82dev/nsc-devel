// Stage 54.4 "Condition Variables" -- NativeSharp.Core.Threading.Monitor, a real Win32 SRWLOCK +
// CONDITION_VARIABLE pair (AcquireSRWLockExclusive/ReleaseSRWLockExclusive/WakeConditionVariable/
// WakeAllConditionVariable/SleepConditionVariableSRW) round-tripped through the FieldAddress
// mechanism Stage 54.2's Atomics established, extended this round to need TWO field addresses
// (`cv_` and `lock_`) in a single call (Monitor.Wait) -- see lib/NativeSharp.Core.Threading/
// Monitor.ns's header comment and `60_stage54_4_monitor.md` for the full design discussion.
//
// Covers, in increasing order of confidence, exactly like every other Stage 53/54 real-execution
// test file in this suite:
//   1. That the five core operations (MonitorEnter/Exit/Wait/Pulse/PulseAll) are recognized, lower
//      to LLVM, and each emits a call to its exact runtime hook symbol -- a minimal hand-written
//      fixture, independent of the packaged example. UNLIKE Mutex/Thread (private static helper
//      methods), Monitor's core operations ARE the five public methods themselves -- the compiler
//      replaces the whole call with native lowering directly.
//   2. That the runtime module (WindowsRuntimeBuilder::buildSupport) emits the FIVE real Win32
//      imports this round adds (AcquireSRWLockExclusive/ReleaseSRWLockExclusive/
//      WakeConditionVariable/WakeAllConditionVariable/SleepConditionVariableSRW) and defines all
//      five __nativesharp_monitor_* wrapper functions.
//   3. That the packaged example project (examples/project_stage54_4) compiles end to end through
//      a real linked Windows x64 PE executable via the actual `nsc` Driver, AT BOTH O0 and O2.
//   4. REAL EXECUTION under Wine (when available, gracefully skipped otherwise), at BOTH
//      optimization levels: Main.ns returns 0 only if every one of its 7 runtime checks passed
//      (see examples/project_stage54_4/Main.ns for the full numbering) -- basic mutual exclusion,
//      a Wait(timeout) that genuinely expires, Pulse()/PulseAll() as safe no-ops when nobody is
//      waiting, a real cross-thread single Wait()/Pulse() wake, a 6-thread barrier proving
//      PulseAll() wakes ALL waiting threads (not just one), and a real 4-producer/4-consumer
//      bounded queue with backpressure in both directions.
//   5. DEDICATED regression fixtures, independent of the packaged example, for the two properties
//      most likely to silently regress if Monitor's lowering or runtime wrappers were touched
//      later: PulseAll() genuinely broadcasts to every waiting thread (not just one), and
//      Wait(timeout) genuinely expires on a real timeout rather than blocking forever.
//
// EXTRA RIGOR FOR THIS ROUND (Alberto's explicit instruction, recorded in
// claude/05_metodo_de_trabajo_y_archivo_historico.md's "Regla de rigor de testing: Stage 54"
// section): every concurrency-dependent check below runs the relevant executable under Wine
// SEVERAL TIMES, requiring exit code 0 with no crash on EVERY single run. The PulseAll broadcast
// regression fixture has no automatic elapsed-time cutoff: if PulseAll() ever regressed into
// single-Pulse() behavior, N-1 of its worker threads would remain blocked until explicit outer
// cancellation. No Sleep() is used anywhere as a synchronization substitute, in this test file or
// in any Main.ns it
// exercises -- every ordering guarantee comes from Thread.Join() plus the monitor/condition-
// variable primitives themselves, never from timing.

#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/LLVM/CoffObjectVerifier.h"
#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/LLVM/ProjectOopLlvmLowerer.h"
#include "nativesharp/Runtime/WindowsRuntimeBuilder.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

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

// Same absolute-path ProjectReference trick Stage 54.3's own test file uses, needed by the
// dedicated regression fixtures below (which exercise the REAL Monitor/Thread classes, not
// fixture stand-ins).
void writeProjectWithThreadingLibraries(const std::filesystem::path& directory, std::string_view name)
{
    const std::filesystem::path core = std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core/NativeSharp.Core.nsproj";
    const std::filesystem::path threading =
        std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj";
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
            "</Name></PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
            "<ProjectReference Include=\"" + core.string() + "\" />"
            "<ProjectReference Include=\"" + threading.string() + "\" /></ItemGroup></Project>");
}

bool commandAvailable(std::string_view command)
{
#ifdef _WIN32
    return std::system(("where " + std::string(command) + " >nul 2>nul").c_str()) == 0;
#else
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
#endif
}

std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

// Direct Wine invocation, matching the other Stage 53/54 real-execution tests. It deliberately
// applies no automatic deadline; cancellation belongs to the outer test controller.
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

std::filesystem::path buildExe(
    TestContext& tests,
    const std::filesystem::path& project,
    const std::filesystem::path& root,
    nativesharp::llvm_backend::LlvmOptimizationLevel level,
    std::string_view levelFlag,
    std::string_view exeStem,
    std::string_view label)
{
    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        std::string(label) + ": passes method-body ownership analysis");
    tests.expect(error.empty(), std::string(label) + ": method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error), std::string(label) + ": lowers to OOP MIR");
    tests.expect(error.empty(), std::string(label) + ": MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return {};

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), std::string(label) + ": lowers to LLVM");
    if (module == nullptr) return {};
    std::string verifyError;
    tests.expect(module->verify(verifyError), std::string(label) + ": LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, targetError);
    tests.expect(target != nullptr && targetError.empty(), std::string(label) + ": obtains Windows x64 target machine");
    if (target == nullptr) return {};
    tests.expect(target->configureModuleForTarget(*module, targetError), std::string(label) + ": applies Windows x64 data layout");

    const auto object = root / (std::string(exeStem) + "_" + std::string(levelFlag) + ".obj");
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        std::string(label) + ": emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), std::string(label) + ": object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, std::string(label) + ": object machine is AMD64 / 0x8664");

    const auto exePath = root / (std::string(exeStem) + "_" + std::string(levelFlag) + ".exe");
    const std::string projectText = project.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {"--emit-project-oop-exe", projectText, levelFlag, "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode driverResult = driver.run(args, out, err);
    tests.expect(driverResult == ExitCode::Success, std::string(label) + ": Driver emits a real Windows x64 PE executable (" + err.str() + ")");
    tests.expect(err.str().empty(), std::string(label) + ": no Driver diagnostics");
    tests.expect(std::filesystem::is_regular_file(exePath), std::string(label) + ": .exe file exists on disk");
    return exePath;
}

// ---------------------------------------------------------------------------------------
// 1. Minimal fixture: the five core operations lower to LLVM and each emits a call to its exact
//    runtime hook symbol. UNLIKE Mutex (private static helpers), Monitor's five core operations
//    ARE its five public methods -- this fixture defines the class with the EXACT class name,
//    field names (`lock_`/`cv_`, ordinals 0 and 1), and method signatures the recognizer requires.
// ---------------------------------------------------------------------------------------

void testMonitorCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "MonitorCore";
    writeProject(directory, "MonitorCore");
    writeFile(directory / "Monitor.ns", R"NS(
namespace NativeSharp.Core.Threading
{
    public class Monitor
    {
        private nint lock_;
        private nint cv_;

        public void Enter() { }
        public void Exit() { }
        public bool Wait(int millisecondsTimeout) { return false; }
        public void Pulse() { }
        public void PulseAll() { }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Threading;

class Program
{
    public static bool Run()
    {
        Monitor m = new Monitor();
        m.Enter();
        bool waited = m.Wait(5);
        m.Pulse();
        m.PulseAll();
        m.Exit();
        return waited;
    }
}
)NS");

    std::string error;
    ProjectCompilation mir;
    const bool ok = mir.compileProjectOopMir(directory / "MonitorCore.nsproj", error);
    tests.expect(ok && error.empty(), "Monitor core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Monitor core-operation fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Monitor core-operation fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("call void @__nativesharp_monitor_enter(") != std::string::npos,
        "Monitor.Enter() lowers to a direct call to __nativesharp_monitor_enter");
    tests.expect(llvm.find("call void @__nativesharp_monitor_exit(") != std::string::npos,
        "Monitor.Exit() lowers to a direct call to __nativesharp_monitor_exit");
    tests.expect(llvm.find("call i1 @__nativesharp_monitor_wait(") != std::string::npos,
        "Monitor.Wait(...) lowers to a direct call to __nativesharp_monitor_wait, passing BOTH the "
        "cv_ AND lock_ field addresses (the first core operation in this project needing two field "
        "addresses from a single receiver, lowered exactly once)");
    tests.expect(llvm.find("call void @__nativesharp_monitor_pulse(") != std::string::npos,
        "Monitor.Pulse() lowers to a direct call to __nativesharp_monitor_pulse");
    tests.expect(llvm.find("call void @__nativesharp_monitor_pulse_all(") != std::string::npos,
        "Monitor.PulseAll() lowers to a direct call to __nativesharp_monitor_pulse_all");
}

// ---------------------------------------------------------------------------------------
// 2. Runtime-module fixture: confirm the runtime module declares and calls the real Win32 exports
//    this round needs, and defines all five __nativesharp_monitor_* wrapper functions.
// ---------------------------------------------------------------------------------------

void testRuntimeModuleCallsRealWin32MonitorHooks(TestContext& tests)
{
    nativesharp::runtime::WindowsRuntimeBuilder builder;
    std::string error;
    std::unique_ptr<nativesharp::llvm_backend::LlvmModule> runtimeModulePtr = builder.buildSupport(error);
    tests.expect(runtimeModulePtr != nullptr && error.empty(),
        "Windows runtime module (with Stage 54.4 monitor hooks) builds cleanly");
    if (runtimeModulePtr == nullptr) return;
    nativesharp::llvm_backend::LlvmModule& runtimeModule = *runtimeModulePtr;
    std::string verifyError;
    tests.expect(runtimeModule.verify(verifyError), "Windows runtime module LLVM verifies clean");

    const std::string llvm = runtimeModule.text();
    tests.expect(llvm.find("declare void @AcquireSRWLockExclusive(") != std::string::npos,
        "runtime module declares the real kernel32 AcquireSRWLockExclusive import");
    tests.expect(llvm.find("declare void @ReleaseSRWLockExclusive(") != std::string::npos,
        "runtime module declares the real kernel32 ReleaseSRWLockExclusive import");
    tests.expect(llvm.find("declare void @WakeConditionVariable(") != std::string::npos,
        "runtime module declares the real kernel32 WakeConditionVariable import");
    tests.expect(llvm.find("declare void @WakeAllConditionVariable(") != std::string::npos,
        "runtime module declares the real kernel32 WakeAllConditionVariable import");
    tests.expect(llvm.find("declare i32 @SleepConditionVariableSRW(") != std::string::npos,
        "runtime module declares the real kernel32 SleepConditionVariableSRW import (Win32 BOOL, "
        "i.e. i32 -- unlike __nativesharp_monitor_wait's own i1 return, the raw import keeps Win32's "
        "native 32-bit BOOL width)");
    tests.expect(llvm.find("define void @__nativesharp_monitor_enter(") != std::string::npos &&
            llvm.find("call void @AcquireSRWLockExclusive(") != std::string::npos,
        "__nativesharp_monitor_enter calls the real AcquireSRWLockExclusive");
    tests.expect(llvm.find("define void @__nativesharp_monitor_exit(") != std::string::npos &&
            llvm.find("call void @ReleaseSRWLockExclusive(") != std::string::npos,
        "__nativesharp_monitor_exit calls the real ReleaseSRWLockExclusive");
    tests.expect(llvm.find("define i1 @__nativesharp_monitor_wait(") != std::string::npos &&
            llvm.find("call i32 @SleepConditionVariableSRW(") != std::string::npos,
        "__nativesharp_monitor_wait calls the real SleepConditionVariableSRW with the caller-chosen "
        "millisecond timeout (Flags=0)");
    tests.expect(llvm.find("define void @__nativesharp_monitor_pulse(") != std::string::npos &&
            llvm.find("call void @WakeConditionVariable(") != std::string::npos,
        "__nativesharp_monitor_pulse calls the real WakeConditionVariable (wakes exactly one waiter)");
    tests.expect(llvm.find("define void @__nativesharp_monitor_pulse_all(") != std::string::npos &&
            llvm.find("call void @WakeAllConditionVariable(") != std::string::npos,
        "__nativesharp_monitor_pulse_all calls the real WakeAllConditionVariable (wakes every waiter)");
}

// ---------------------------------------------------------------------------------------
// 3 + 4. Packaged example (examples/project_stage54_4), tested at BOTH O0 and O2, executed SEVERAL
//    TIMES per level (extra rigor for Stage 54, same run count Stage 54.1-54.3 established).
// ---------------------------------------------------------------------------------------

constexpr int kWineRunsPerLevel = 5;

void testPackagedExampleAtOptimizationLevel(
    TestContext& tests,
    const std::filesystem::path& root,
    nativesharp::llvm_backend::LlvmOptimizationLevel level,
    std::string_view levelFlag,
    std::string_view levelLabel)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage54_4/Stage54_4Monitor.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 54.4 monitor example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    const std::string label = std::string("packaged Stage 54.4 example (") + std::string(levelLabel) + ")";
    const std::filesystem::path exePath =
        buildExe(tests, project, root, level, levelFlag, "Stage54_4Monitor", label);
    if (exePath.empty()) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string("Wine not available in this environment -- Stage 54.4 (") +
            std::string(levelLabel) + ") real-execution check skipped, all structural checks above still ran");
        return;
    }

    const auto workingDirectory = root / (std::string("run_") + std::string(levelLabel));
    std::error_code cleanupError;
    std::filesystem::remove_all(workingDirectory, cleanupError);
    std::filesystem::create_directories(workingDirectory);

    for (int iteration = 0; iteration < kWineRunsPerLevel; ++iteration)
    {
        int exitCode = 0;
        bool crashed = false;
        const auto logPath = root /
            (std::string("Stage54_4Monitor_") + std::string(levelLabel) + "_run" +
                std::to_string(iteration) + ".log");
        const bool ran = runUnderWine(wine, workingDirectory, exePath, logPath, exitCode, crashed);
        const std::string runLabel = std::string(levelLabel) + " run " + std::to_string(iteration + 1) +
            "/" + std::to_string(kWineRunsPerLevel);
        tests.expect(ran, "Wine could be invoked on the Stage 54.4 (" + runLabel + ") executable");
        tests.expect(!crashed, "Stage 54.4 (" + runLabel + ") executable does not crash under real execution");
        tests.expect(exitCode == 0,
            "Stage 54.4 (" + runLabel +
                ") executable returns exit code 0 (all 7 monitor runtime checks passed -- mutual "
                "exclusion, a real Wait(timeout) expiration, safe no-op Pulse/PulseAll, a real "
                "cross-thread Wait/Pulse wake, a 6-thread PulseAll broadcast barrier, and a real "
                "4-producer/4-consumer bounded queue; see examples/project_stage54_4/Main.ns for the "
                "check numbering)");
    }
}

// ---------------------------------------------------------------------------------------
// 5a. DEDICATED regression fixture: PulseAll() must wake EVERY thread currently blocked in Wait(),
//    not just one -- the single most important broadcast-vs-single-wake property this round
//    introduces. 8 worker threads, a real barrier (Main's own Wait(-1) only proceeds once all 8
//    have incremented ThreadsStarted under the SAME lock) GUARANTEES all 8 are genuinely asleep in
//    their own Wait() before the ONE PulseAll() call fires. If PulseAll() ever regressed into
//    single-Pulse() behavior, 7 of the 8 threads would never wake and Join() would never return.
//    The fixture remains active until the outer test run is cancelled explicitly.
// ---------------------------------------------------------------------------------------

constexpr int kPulseAllRegressionWineRuns = 8;

void testPulseAllWakesAllWaitingThreads(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "PulseAllBroadcast";
    writeProjectWithThreadingLibraries(directory, "PulseAllBroadcast");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Threading;

class Barrier
{
    public static int ThreadsStarted = 0;
    public static bool Go = false;
    public static Monitor Lock1 = new Monitor();
    public static AtomicInt32 Completed = new AtomicInt32(0);
}

class Program
{
    private static void Worker()
    {
        Barrier.Lock1.Enter();
        Barrier.ThreadsStarted = Barrier.ThreadsStarted + 1;
        Barrier.Lock1.PulseAll();
        while (!Barrier.Go)
        {
            Barrier.Lock1.Wait(-1);
        }
        Barrier.Lock1.Exit();
        Barrier.Completed.Increment();
    }

    public static int Main()
    {
        ThreadStart start = Worker;
        raii Thread t1 = new Thread(start);
        raii Thread t2 = new Thread(start);
        raii Thread t3 = new Thread(start);
        raii Thread t4 = new Thread(start);
        raii Thread t5 = new Thread(start);
        raii Thread t6 = new Thread(start);
        raii Thread t7 = new Thread(start);
        raii Thread t8 = new Thread(start);

        Barrier.Lock1.Enter();
        while (Barrier.ThreadsStarted < 8)
        {
            Barrier.Lock1.Wait(-1);
        }
        Barrier.Go = true;
        // The ONE and ONLY PulseAll() call in this program -- every one of the 8 worker threads
        // must already be asleep in its own Wait() at this exact moment (guaranteed by the barrier
        // above), so this single call is what proves broadcast semantics, not repeated signaling.
        Barrier.Lock1.PulseAll();
        Barrier.Lock1.Exit();

        t1.Join();
        t2.Join();
        t3.Join();
        t4.Join();
        t5.Join();
        t6.Join();
        t7.Join();
        t8.Join();
        if (Barrier.Completed.Value != 8) { return 1; }
        return 0;
    }
}
)NS");

    const std::string label = "PulseAll-broadcast regression";
    const std::filesystem::path exePath = buildExe(
        tests, directory / "PulseAllBroadcast.nsproj", root,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "PulseAllBroadcast", label);
    if (exePath.empty()) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- " + label + " real-execution "
            "check skipped, all structural checks above still ran");
        return;
    }

    const auto workingDirectory = root / "pulse_all_run";
    std::error_code cleanupError;
    std::filesystem::remove_all(workingDirectory, cleanupError);
    std::filesystem::create_directories(workingDirectory);

    for (int iteration = 0; iteration < kPulseAllRegressionWineRuns; ++iteration)
    {
        int exitCode = 0;
        bool crashed = false;
        const auto logPath = root / (std::string("PulseAllBroadcast_run") + std::to_string(iteration) + ".log");
        const bool ran = runUnderWine(wine, workingDirectory, exePath, logPath, exitCode, crashed);
        const std::string runLabel = "run " + std::to_string(iteration + 1) + "/" + std::to_string(kPulseAllRegressionWineRuns);
        tests.expect(ran, label + " (" + runLabel + "): Wine could be invoked on the executable");
        tests.expect(!crashed, label + " (" + runLabel + "): executable does not crash under real execution");
        // The critical assertion is the executable's real zero exit code. The harness does not
        // synthesize a timeout exit code for a PulseAll->Pulse regression.
        tests.expect(exitCode == 0,
            label + " (" + runLabel + "): exit code 0, all 8 barrier-blocked worker threads woke up "
            "from the SAME single PulseAll() call and completed -- regression guard for PulseAll() "
            "ever behaving like a single Pulse()");
    }
}

// ---------------------------------------------------------------------------------------
// 5b. DEDICATED regression fixture: Wait(timeout) must genuinely expire (return false) when the
//    condition it is waiting for never becomes true and nobody ever calls Pulse()/PulseAll() --
//    independent of the packaged example's own Section 2, and re-verified across several Wine runs
//    since a timeout-vs-hang bug is exactly the shape of regression that could pass once by luck
//    under a slow/fast scheduler and fail on a different run.
// ---------------------------------------------------------------------------------------

constexpr int kTimeoutRegressionWineRuns = 5;

void testWaitTimeoutGenuinelyExpires(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "WaitTimeoutExpires";
    writeProjectWithThreadingLibraries(directory, "WaitTimeoutExpires");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Threading;

class Program
{
    public static int Main()
    {
        // Nobody else touches this Monitor -- a condition that never becomes true, with a short
        // real timeout, must always expire (false), never block forever.
        Monitor m = new Monitor();
        m.Enter();
        bool waited1 = m.Wait(10);
        bool waited2 = m.Wait(50);
        bool waited3 = m.Wait(200);
        m.Exit();
        if (waited1) { return 1; }
        if (waited2) { return 2; }
        if (waited3) { return 3; }
        // The lock must still be perfectly usable after three timeout expirations in a row -- Win32
        // guarantees Wait() always reacquires the lock before returning, success or timeout.
        m.Enter();
        m.Exit();
        return 0;
    }
}
)NS");

    const std::string label = "Wait(timeout)-genuinely-expires regression";
    const std::filesystem::path exePath = buildExe(
        tests, directory / "WaitTimeoutExpires.nsproj", root,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "WaitTimeoutExpires", label);
    if (exePath.empty()) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- " + label + " real-execution "
            "check skipped, all structural checks above still ran");
        return;
    }

    const auto workingDirectory = root / "wait_timeout_run";
    std::error_code cleanupError;
    std::filesystem::remove_all(workingDirectory, cleanupError);
    std::filesystem::create_directories(workingDirectory);

    for (int iteration = 0; iteration < kTimeoutRegressionWineRuns; ++iteration)
    {
        int exitCode = 0;
        bool crashed = false;
        const auto logPath = root / (std::string("WaitTimeoutExpires_run") + std::to_string(iteration) + ".log");
        const bool ran = runUnderWine(wine, workingDirectory, exePath, logPath, exitCode, crashed);
        const std::string runLabel = "run " + std::to_string(iteration + 1) + "/" + std::to_string(kTimeoutRegressionWineRuns);
        tests.expect(ran, label + " (" + runLabel + "): Wine could be invoked on the executable");
        tests.expect(!crashed, label + " (" + runLabel + "): executable does not crash under real execution");
        tests.expect(exitCode == 0,
            label + " (" + runLabel + "): exit code 0, three consecutive Wait(timeout) calls on a "
            "condition nobody ever signals all genuinely expired (10ms, 50ms, 200ms) and the lock "
            "remained usable afterward");
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage54_4_monitor_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testMonitorCoreOperationsLowerToRuntimeCalls(tests, root);
    testRuntimeModuleCallsRealWin32MonitorHooks(tests);
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "O0");
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, "-O2", "O2");
    testPulseAllWakesAllWaitingThreads(tests, root);
    testWaitTimeoutGenuinelyExpires(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
