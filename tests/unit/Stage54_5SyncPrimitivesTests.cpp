// Stage 54.5 "Otras primitivas de sincronizacion" -- NativeSharp.Core.Threading.Semaphore and
// NativeSharp.Core.Threading.Event, both real Win32 KERNEL objects (CreateSemaphoreA/
// WaitForSingleObject/ReleaseSemaphore, CreateEventA/SetEvent/ResetEvent/WaitForSingleObject) round-
// tripped through the exact same opaque-HANDLE-as-nint pattern Stage 54.1's Thread and Stage 54.3's
// Mutex already established -- pure HANDLE-based, zero new compiler mechanism this round. See
// lib/NativeSharp.Core.Threading/Semaphore.ns and Event.ns's header comments, and
// `61_stage54_5_semaphore_event.md`, for the full design discussion.
//
// Covers, in increasing order of confidence, exactly like every other Stage 53/54 real-execution
// test file in this suite:
//   1. That the nine core operations (SemaphoreCreate/Wait/Release/Close, EventCreate/Set/Reset/
//      Wait/Close) are recognized, lower to LLVM, and each emits a call to its exact runtime hook
//      symbol -- a minimal hand-written fixture, independent of the packaged example.
//   2. That the runtime module (WindowsRuntimeBuilder::buildSupport) emits the FIVE real Win32
//      imports this round adds (CreateSemaphoreA/ReleaseSemaphore, CreateEventA/SetEvent/
//      ResetEvent) and reuses WaitForSingleObject/CloseHandle from Stage 54.1/53.3b rather than
//      declaring them again -- including confirming Semaphore.Wait/Event.Wait SHARE the exact same
//      __nativesharp_wait_for_handle_timeout runtime wrapper (the first such sharing in this
//      project's core-operation history).
//   3. That the packaged example project (examples/project_stage54_5) compiles end to end through
//      a real linked Windows x64 PE executable via the actual `nsc` Driver, AT BOTH O0 and O2.
//   4. REAL EXECUTION under Wine (when available, gracefully skipped otherwise), at BOTH
//      optimization levels: Main.ns returns 0 only if every one of its 12 runtime checks passed
//      (see examples/project_stage54_5/Main.ns for the full numbering) -- basic Semaphore Wait/
//      Release, a Semaphore.Wait(timeout) that genuinely expires, a modest create/destroy
//      exhaustion loop, a real 10-thread concurrency-limit stress test on Semaphore(3,3) verified
//      with an atomic in-flight counter, Event manual-reset broadcast semantics (5 waiters),
//      Event auto-reset single-consumer semantics (with a follow-up timeout proving the automatic
//      reset), and an explicit Event.Reset() check.
//   5. A DEDICATED regression fixture for Semaphore's real concurrency limit, independent of the
//      packaged example, with MORE contention (16 threads over a Semaphore(4,4)) than the packaged
//      example's own stress section -- exists specifically to catch a regression in the
//      concurrency-limiting property itself.
//
// EXTRA RIGOR FOR THIS ROUND (Alberto's explicit instruction, recorded in
// claude/05_metodo_de_trabajo_y_archivo_historico.md's "Regla de rigor de testing: Stage 54"
// section): every concurrency-dependent check below runs the relevant executable under Wine
// SEVERAL TIMES, requiring exit code 0 with no crash on EVERY single run. No Sleep() is used
// anywhere as a synchronization substitute, in this test file or in any Main.ns it exercises --
// every ordering guarantee comes from Thread.Join() plus the semaphore/event primitives
// themselves, never from timing.

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
// 1. Minimal fixture: the nine core operations lower to LLVM and each emits a call to its exact
//    runtime hook symbol. Semaphore/Event's core operations are PRIVATE static helpers, exactly
//    mirroring Stage 54.3's Mutex fixture pattern.
// ---------------------------------------------------------------------------------------

void testSyncPrimitivesCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "SyncCore";
    writeProject(directory, "SyncCore");
    writeFile(directory / "Semaphore.ns", R"NS(
namespace NativeSharp.Core.Threading
{
    public class Semaphore
    {
        public static nint CreateHandle(int initialCount, int maximumCount) { return SemaphoreCreate(initialCount, maximumCount); }
        public static bool WaitHandle(nint handle, int millisecondsTimeout) { return SemaphoreWait(handle, millisecondsTimeout); }
        public static void ReleaseHandle(nint handle) { SemaphoreRelease(handle); }
        public static void CloseHandleFor(nint handle) { SemaphoreClose(handle); }

        private static nint SemaphoreCreate(int initialCount, int maximumCount) { return 0; }
        private static bool SemaphoreWait(nint handle, int millisecondsTimeout) { return false; }
        private static void SemaphoreRelease(nint handle) { }
        private static void SemaphoreClose(nint handle) { }
    }
}
)NS");
    writeFile(directory / "Event.ns", R"NS(
namespace NativeSharp.Core.Threading
{
    public class Event
    {
        public static nint CreateHandle(bool manualReset, bool initialState) { return EventCreate(manualReset, initialState); }
        public static void SetHandle(nint handle) { EventSet(handle); }
        public static void ResetHandle(nint handle) { EventReset(handle); }
        public static bool WaitHandle(nint handle, int millisecondsTimeout) { return EventWait(handle, millisecondsTimeout); }
        public static void CloseHandleFor(nint handle) { EventClose(handle); }

        private static nint EventCreate(bool manualReset, bool initialState) { return 0; }
        private static void EventSet(nint handle) { }
        private static void EventReset(nint handle) { }
        private static bool EventWait(nint handle, int millisecondsTimeout) { return false; }
        private static void EventClose(nint handle) { }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Threading;

class Program
{
    public static bool Run()
    {
        nint sem = Semaphore.CreateHandle(1, 1);
        bool semWaited = Semaphore.WaitHandle(sem, 5);
        Semaphore.ReleaseHandle(sem);
        Semaphore.CloseHandleFor(sem);

        nint ev = Event.CreateHandle(true, false);
        Event.SetHandle(ev);
        bool evWaited = Event.WaitHandle(ev, 5);
        Event.ResetHandle(ev);
        Event.CloseHandleFor(ev);

        return semWaited && evWaited;
    }
}
)NS");

    std::string error;
    ProjectCompilation mir;
    const bool ok = mir.compileProjectOopMir(directory / "SyncCore.nsproj", error);
    tests.expect(ok && error.empty(), "Semaphore/Event core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Semaphore/Event core-operation fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Semaphore/Event core-operation fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("call i64 @__nativesharp_semaphore_create(") != std::string::npos,
        "Semaphore.SemaphoreCreate(...) lowers to a direct call to __nativesharp_semaphore_create");
    tests.expect(llvm.find("call i1 @__nativesharp_wait_for_handle_timeout(") != std::string::npos,
        "Semaphore.SemaphoreWait(...) AND Event.EventWait(...) both lower to the SAME shared "
        "__nativesharp_wait_for_handle_timeout runtime wrapper (found at least one call site)");
    tests.expect(llvm.find("call void @__nativesharp_semaphore_release(") != std::string::npos,
        "Semaphore.SemaphoreRelease(...) lowers to a direct call to __nativesharp_semaphore_release");
    tests.expect(llvm.find("call void @__nativesharp_semaphore_close(") != std::string::npos,
        "Semaphore.SemaphoreClose(...) lowers to a direct call to __nativesharp_semaphore_close");
    tests.expect(llvm.find("call i64 @__nativesharp_event_create(") != std::string::npos,
        "Event.EventCreate(...) lowers to a direct call to __nativesharp_event_create");
    tests.expect(llvm.find("call void @__nativesharp_event_set(") != std::string::npos,
        "Event.EventSet(...) lowers to a direct call to __nativesharp_event_set");
    tests.expect(llvm.find("call void @__nativesharp_event_reset(") != std::string::npos,
        "Event.EventReset(...) lowers to a direct call to __nativesharp_event_reset");
    tests.expect(llvm.find("call void @__nativesharp_event_close(") != std::string::npos,
        "Event.EventClose(...) lowers to a direct call to __nativesharp_event_close");

    // Confirm the SHARED wait wrapper is actually called from BOTH Semaphore.Wait and Event.Wait
    // call sites (not just one), by counting occurrences.
    std::size_t waitCallCount = 0;
    std::size_t position = 0;
    while (true)
    {
        position = llvm.find("call i1 @__nativesharp_wait_for_handle_timeout(", position);
        if (position == std::string::npos) break;
        ++waitCallCount;
        position += 1;
    }
    tests.expect(waitCallCount == 2,
        "the shared __nativesharp_wait_for_handle_timeout wrapper is called from BOTH the "
        "Semaphore.Wait call site AND the Event.Wait call site (exactly 2 occurrences)");
}

// ---------------------------------------------------------------------------------------
// 2. Runtime-module fixture: confirm the runtime module declares and calls the real Win32 exports
//    this round needs, reuses WaitForSingleObject/CloseHandle rather than re-declaring them, and
//    that Semaphore.Wait/Event.Wait genuinely share ONE runtime wrapper function.
// ---------------------------------------------------------------------------------------

void testRuntimeModuleCallsRealWin32SyncPrimitiveHooks(TestContext& tests)
{
    nativesharp::runtime::WindowsRuntimeBuilder builder;
    std::string error;
    std::unique_ptr<nativesharp::llvm_backend::LlvmModule> runtimeModulePtr = builder.buildSupport(error);
    tests.expect(runtimeModulePtr != nullptr && error.empty(),
        "Windows runtime module (with Stage 54.5 semaphore/event hooks) builds cleanly");
    if (runtimeModulePtr == nullptr) return;
    nativesharp::llvm_backend::LlvmModule& runtimeModule = *runtimeModulePtr;
    std::string verifyError;
    tests.expect(runtimeModule.verify(verifyError), "Windows runtime module LLVM verifies clean");

    const std::string llvm = runtimeModule.text();
    tests.expect(llvm.find("declare ptr @CreateSemaphoreA(") != std::string::npos,
        "runtime module declares the real kernel32 CreateSemaphoreA import");
    tests.expect(llvm.find("declare i32 @ReleaseSemaphore(") != std::string::npos,
        "runtime module declares the real kernel32 ReleaseSemaphore import");
    tests.expect(llvm.find("declare ptr @CreateEventA(") != std::string::npos,
        "runtime module declares the real kernel32 CreateEventA import");
    tests.expect(llvm.find("declare i32 @SetEvent(") != std::string::npos,
        "runtime module declares the real kernel32 SetEvent import");
    tests.expect(llvm.find("declare i32 @ResetEvent(") != std::string::npos,
        "runtime module declares the real kernel32 ResetEvent import");
    tests.expect(llvm.find("define i64 @__nativesharp_semaphore_create(") != std::string::npos &&
            llvm.find("call ptr @CreateSemaphoreA(") != std::string::npos,
        "__nativesharp_semaphore_create calls the real CreateSemaphoreA");
    tests.expect(llvm.find("define void @__nativesharp_semaphore_release(") != std::string::npos &&
            llvm.find("call i32 @ReleaseSemaphore(") != std::string::npos,
        "__nativesharp_semaphore_release calls the real ReleaseSemaphore (releases exactly one level)");
    tests.expect(llvm.find("define void @__nativesharp_semaphore_close(") != std::string::npos,
        "__nativesharp_semaphore_close is defined (calls the REUSED CloseHandle import)");
    tests.expect(llvm.find("define i64 @__nativesharp_event_create(") != std::string::npos &&
            llvm.find("call ptr @CreateEventA(") != std::string::npos,
        "__nativesharp_event_create calls the real CreateEventA");
    tests.expect(llvm.find("define void @__nativesharp_event_set(") != std::string::npos &&
            llvm.find("call i32 @SetEvent(") != std::string::npos,
        "__nativesharp_event_set calls the real SetEvent");
    tests.expect(llvm.find("define void @__nativesharp_event_reset(") != std::string::npos &&
            llvm.find("call i32 @ResetEvent(") != std::string::npos,
        "__nativesharp_event_reset calls the real ResetEvent");
    tests.expect(llvm.find("define void @__nativesharp_event_close(") != std::string::npos,
        "__nativesharp_event_close is defined (calls the REUSED CloseHandle import)");
    tests.expect(llvm.find("define i1 @__nativesharp_wait_for_handle_timeout(") != std::string::npos &&
            llvm.find("call i32 @WaitForSingleObject(") != std::string::npos,
        "__nativesharp_wait_for_handle_timeout (the SHARED wrapper) calls the REUSED "
        "WaitForSingleObject import, collapsing the result to a bool via WAIT_OBJECT_0");
}

// ---------------------------------------------------------------------------------------
// 3 + 4. Packaged example (examples/project_stage54_5), tested at BOTH O0 and O2, executed SEVERAL
//    TIMES per level (extra rigor for Stage 54, same run count Stage 54.1-54.4 established).
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
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage54_5/Stage54_5SyncPrimitives.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 54.5 sync-primitives example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    const std::string label = std::string("packaged Stage 54.5 example (") + std::string(levelLabel) + ")";
    const std::filesystem::path exePath =
        buildExe(tests, project, root, level, levelFlag, "Stage54_5SyncPrimitives", label);
    if (exePath.empty()) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string("Wine not available in this environment -- Stage 54.5 (") +
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
            (std::string("Stage54_5SyncPrimitives_") + std::string(levelLabel) + "_run" +
                std::to_string(iteration) + ".log");
        const bool ran = runUnderWine(wine, workingDirectory, exePath, logPath, exitCode, crashed);
        const std::string runLabel = std::string(levelLabel) + " run " + std::to_string(iteration + 1) +
            "/" + std::to_string(kWineRunsPerLevel);
        tests.expect(ran, "Wine could be invoked on the Stage 54.5 (" + runLabel + ") executable");
        tests.expect(!crashed, "Stage 54.5 (" + runLabel + ") executable does not crash under real execution");
        tests.expect(exitCode == 0,
            "Stage 54.5 (" + runLabel +
                ") executable returns exit code 0 (all 12 semaphore/event runtime checks passed -- "
                "basic Semaphore Wait/Release, a real Wait(timeout) expiration, a 200-semaphore "
                "create/destroy exhaustion loop, a 10-thread Semaphore(3,3) concurrency-limit stress "
                "test verified with an atomic in-flight counter, Event manual-reset broadcast to 5 "
                "waiters, Event auto-reset single-consumer semantics with automatic re-arm to "
                "non-signaled, and an explicit Event.Reset(); see "
                "examples/project_stage54_5/Main.ns for the check numbering)");
    }
}

// ---------------------------------------------------------------------------------------
// 5. DEDICATED regression fixture for Semaphore's real concurrency limit -- independent of the
//    packaged example, MORE contention (16 threads over a Semaphore(4,4), vs. the packaged
//    example's 10 threads over a Semaphore(3,3)) specifically to maximize the chance of catching a
//    regression in the concurrency-limiting property itself.
// ---------------------------------------------------------------------------------------

constexpr int kSemaphoreLimitRegressionWineRuns = 8;

void testSemaphoreNeverExceedsConcurrencyLimit(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "SemaphoreLimit";
    writeProjectWithThreadingLibraries(directory, "SemaphoreLimit");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Threading;

class SemLimit
{
    public static Semaphore Sem = new Semaphore(4, 4);
    public static AtomicInt32 Active = new AtomicInt32(0);
    public static AtomicInt32 Violations = new AtomicInt32(0);
    public static AtomicInt32 Completed = new AtomicInt32(0);
}

class Program
{
    private static void Worker()
    {
        SemLimit.Sem.Wait(-1);
        int active = SemLimit.Active.Increment();
        if (active > 4) { SemLimit.Violations.Increment(); }
        int busy = 0;
        while (busy < 30000) { busy = busy + 1; }
        SemLimit.Active.Decrement();
        SemLimit.Sem.Release();
        SemLimit.Completed.Increment();
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
        raii Thread t9 = new Thread(start);
        raii Thread t10 = new Thread(start);
        raii Thread t11 = new Thread(start);
        raii Thread t12 = new Thread(start);
        raii Thread t13 = new Thread(start);
        raii Thread t14 = new Thread(start);
        raii Thread t15 = new Thread(start);
        raii Thread t16 = new Thread(start);
        t1.Join();
        t2.Join();
        t3.Join();
        t4.Join();
        t5.Join();
        t6.Join();
        t7.Join();
        t8.Join();
        t9.Join();
        t10.Join();
        t11.Join();
        t12.Join();
        t13.Join();
        t14.Join();
        t15.Join();
        t16.Join();
        if (SemLimit.Completed.Value != 16) { return 1; }
        if (SemLimit.Violations.Value != 0) { return 2; }
        if (SemLimit.Active.Value != 0) { return 3; }
        return 0;
    }
}
)NS");

    const std::string label = "Semaphore-concurrency-limit regression";
    const std::filesystem::path exePath = buildExe(
        tests, directory / "SemaphoreLimit.nsproj", root,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "SemaphoreLimit", label);
    if (exePath.empty()) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- " + label + " real-execution "
            "check skipped, all structural checks above still ran");
        return;
    }

    const auto workingDirectory = root / "semaphore_limit_run";
    std::error_code cleanupError;
    std::filesystem::remove_all(workingDirectory, cleanupError);
    std::filesystem::create_directories(workingDirectory);

    for (int iteration = 0; iteration < kSemaphoreLimitRegressionWineRuns; ++iteration)
    {
        int exitCode = 0;
        bool crashed = false;
        const auto logPath = root / (std::string("SemaphoreLimit_run") + std::to_string(iteration) + ".log");
        const bool ran = runUnderWine(wine, workingDirectory, exePath, logPath, exitCode, crashed);
        const std::string runLabel = "run " + std::to_string(iteration + 1) + "/" + std::to_string(kSemaphoreLimitRegressionWineRuns);
        tests.expect(ran, label + " (" + runLabel + "): Wine could be invoked on the executable");
        tests.expect(!crashed, label + " (" + runLabel + "): executable does not crash under real execution");
        tests.expect(exitCode == 0,
            label + " (" + runLabel + "): exit code 0, 16 threads over a Semaphore(4,4) never "
            "observed more than 4 concurrently in-flight (atomic violation counter stayed 0) -- "
            "regression guard for Semaphore's real Win32 concurrency limit");
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage54_5_sync_primitives_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testSyncPrimitivesCoreOperationsLowerToRuntimeCalls(tests, root);
    testRuntimeModuleCallsRealWin32SyncPrimitiveHooks(tests);
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "O0");
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, "-O2", "O2");
    testSemaphoreNeverExceedsConcurrencyLimit(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
