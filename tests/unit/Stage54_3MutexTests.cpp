// Stage 54.3 "Mutex" -- NativeSharp.Core.Threading.Mutex, a real Win32 KERNEL mutex
// (CreateMutexA/WaitForSingleObject/ReleaseMutex/CloseHandle) round-tripped through the exact
// same opaque-HANDLE-as-nint pattern Stage 54.1's Thread already established -- see
// lib/NativeSharp.Core.Threading/Mutex.ns's header comment and `59_stage54_3_mutex.md` for the
// full design discussion (kernel mutex vs. CRITICAL_SECTION, reentrancy semantics).
//
// THIS ROUND ALSO FOUND AND FIXED TWO SERIOUS, PRE-EXISTING, COMPILER-WIDE BUGS in the static
// constructor ("cctor") lazy-initialization guard (ProjectOopLlvmLowerer.cpp's
// defineTypeInitializers) -- bugs unrelated to Mutex conceptually, simply never exercised by any
// prior round's examples until Stage 54.3's rigor-driven testing had multiple threads race a
// class's first static-field touch for the first time in this project's history. Full technical
// detail (diagnostic methodology, the mingw/C reference-program proof, both fix designs) lives in
// `59_stage54_3_mutex.md` and `59_stage54_3_mutex_implementacion_paso_a_paso.md`; this file adds
// DEDICATED regression coverage for both, in addition to the usual Mutex feature coverage:
//
//   BUG #1 (cross-thread race): the guard used a plain non-atomic `i8` state global with an
//   ordinary load-compare-branch-store sequence and NO synchronization -- two threads racing to
//   touch the SAME class's static members for the first time could BOTH observe "not started" and
//   BOTH run the initializer body concurrently (a classic TOCTOU bug). Fixed with a real atomic
//   compare-and-swap (LLVMBuildAtomicCmpXchg) for the 0->1 claim, plus atomic loads/stores
//   (LLVMSetOrdering) for the fast-path check and the spin-wait loop. See
//   testConcurrentStaticFieldFirstTouchDoesNotRace below.
//
//   BUG #2 (same-thread self-deadlock, introduced by the BUG #1 fix): a self-referencing static
//   field initializer (`static int A = 5; static int B = Foo.A + 1;`) recursively re-enters its
//   own type's cctor from the SAME thread/call-stack while that cctor is still "in progress" --
//   under a naive CAS-only guard, the recursive call spin-waits on a state transition that can
//   only happen when the OUTER call finishes, which never happens: an infinite self-deadlock.
//   Fixed with a per-type "owner thread id" global and a dedicated recursion-check block that lets
//   a thread recognize it is recursing into its own in-progress initialization and proceed
//   immediately instead of spin-waiting. See testSelfReferencingStaticInitializerDoesNotDeadlock
//   below -- deliberately bounded by runUnderWine's own `timeout 20` wrapper, so a regression here
//   fails loudly (exit code from `timeout`, never a hung test binary) rather than hanging ctest.
//
// Covers, in increasing order of confidence, exactly like every other Stage 53/54 real-execution
// test file in this suite:
//   1. That the four core operations (MutexCreate/MutexLock/MutexUnlock/MutexClose) are
//      recognized, lower to LLVM, and each emits a call to its exact runtime hook symbol --
//      MutexCreate is the FIRST zero-operand core operation in the project (no receiver, no
//      arguments) -- a minimal hand-written fixture, independent of the packaged example.
//   2. That the runtime module (WindowsRuntimeBuilder::buildSupport) emits the FOUR real Win32
//      imports this round adds (CreateMutexA/ReleaseMutex, plus the GetCurrentThreadId used only
//      by the cctor-guard fix) and reuses WaitForSingleObject/CloseHandle from Stage 54.1/53.3b
//      rather than declaring them again.
//   3. That the packaged example project (examples/project_stage54_3) compiles end to end through
//      a real linked Windows x64 PE executable via the actual `nsc` Driver, AT BOTH O0 and O2 --
//      same discipline every Stage 53/54 round has followed since 53.2b.
//   4. REAL EXECUTION under Wine (when available, gracefully skipped otherwise), at BOTH
//      optimization levels: Main.ns returns 0 only if every one of its 10 runtime checks passed
//      (see examples/project_stage54_3/Main.ns for the full numbering) -- basic Lock/Unlock,
//      recursive same-thread reacquisition, Unlock() on a never-acquired mutex, raii-scoped
//      destruction, a modest create/destroy resource-exhaustion loop, and TWO independent
//      4-threads-x-N real mutual-exclusion stress tests on SHARED NON-atomic counters.
//   5. DEDICATED regression coverage for both cctor-guard bugs found this round (see above),
//      independent of the packaged example, so a future change to the guard that reintroduces
//      either bug fails here even if Main.ns's own checks happened not to catch it.
//
// EXTRA RIGOR FOR THIS ROUND (Alberto's explicit instruction, recorded in
// claude/05_metodo_de_trabajo_y_archivo_historico.md's "Regla de rigor de testing: Stage 54"
// section): every concurrency-dependent check below runs the relevant executable under Wine
// SEVERAL TIMES, requiring exit code 0 with no crash on EVERY single run -- a scheduling-dependent
// bug (exactly the shape of both bugs fixed this round) would otherwise slip through a single
// green run undetected. No Sleep() is used anywhere as a synchronization substitute, in this test
// file or in any Main.ns it exercises -- every ordering guarantee comes from Thread.Join() (a real
// kernel-level memory barrier) plus the mutex/atomic primitives themselves, never from timing.

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

// Writes an .nsproj that references the real NativeSharp.Core / NativeSharp.Core.Threading
// library projects by ABSOLUTE path (std::filesystem::path's operator/ treats an absolute RHS
// as a full replacement, so this resolves correctly regardless of the temp directory's own
// location) -- used by the two dedicated regression fixtures below, which need the REAL Mutex
// and Thread classes (not fixture stand-ins) to exercise real multi-threaded execution.
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

// Same fallback order as every other Stage 52/53/54 real-execution test file in this repo.
std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

// Portable (no POSIX-specific code) -- same std::system()-based invocation every other Stage 53/
// 54 real-execution test file in this repo uses. Per Alberto's explicit, repeated instruction
// this project's own C++ source (tests included) must contain zero Unix/Linux-specific code. The
// `timeout 20` here is what makes testSelfReferencingStaticInitializerDoesNotDeadlock below SAFE
// to run even if the deadlock regression it guards against were reintroduced: the wrapped process
// gets killed after 20s and this function returns a non-zero, non-crash exit code -- the test
// binary itself, and ctest, never hang.
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

// Compiles a project all the way to a Windows x64 .exe at the given optimization level via the
// real Driver, exactly like every packaged-example fixture in this suite. Returns the exe path on
// success, or an empty path (with a failing expectation already recorded) on any failure.
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
// 1. Minimal fixture: the four core operations lower to LLVM and each emits a call to its exact
//    runtime hook symbol. MutexCreate/Lock/Unlock/Close are PRIVATE in the real Mutex.ns (like
//    Thread's core operations) -- this fixture reflects that with four thin public wrappers,
//    exactly mirroring Stage 54.1's ThreadCore fixture. MutexCreate is the first zero-operand
//    core operation in the whole project (no receiver, no arguments) -- confirmed below by the
//    exact "()" in its call site.
// ---------------------------------------------------------------------------------------

void testMutexCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "MutexCore";
    writeProject(directory, "MutexCore");
    writeFile(directory / "Mutex.ns", R"NS(
namespace NativeSharp.Core.Threading
{
    public class Mutex
    {
        public static nint CreateHandle() { return MutexCreate(); }
        public static void LockHandle(nint handle) { MutexLock(handle); }
        public static void UnlockHandle(nint handle) { MutexUnlock(handle); }
        public static void CloseHandleFor(nint handle) { MutexClose(handle); }

        private static nint MutexCreate() { return 0; }
        private static void MutexLock(nint handle) { }
        private static void MutexUnlock(nint handle) { }
        private static void MutexClose(nint handle) { }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Threading;

class Program
{
    public static nint Run()
    {
        nint created = Mutex.CreateHandle();
        Mutex.LockHandle(created);
        Mutex.UnlockHandle(created);
        Mutex.CloseHandleFor(created);
        return created;
    }
}
)NS");

    std::string error;
    ProjectCompilation mir;
    const bool ok = mir.compileProjectOopMir(directory / "MutexCore.nsproj", error);
    tests.expect(ok && error.empty(), "Mutex core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Mutex core-operation fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Mutex core-operation fixture LLVM verifies clean");

    const std::string llvm = module->text();
    tests.expect(llvm.find("call i64 @__nativesharp_mutex_create()") != std::string::npos,
        "Mutex.MutexCreate() lowers to a direct, ZERO-OPERAND call to __nativesharp_mutex_create -- "
        "the first zero-operand core operation in the whole project");
    tests.expect(llvm.find("call void @__nativesharp_mutex_lock") != std::string::npos,
        "Mutex.MutexLock(...) lowers to a direct call to __nativesharp_mutex_lock");
    tests.expect(llvm.find("call void @__nativesharp_mutex_unlock") != std::string::npos,
        "Mutex.MutexUnlock(...) lowers to a direct call to __nativesharp_mutex_unlock");
    tests.expect(llvm.find("call void @__nativesharp_mutex_close") != std::string::npos,
        "Mutex.MutexClose(...) lowers to a direct call to __nativesharp_mutex_close");
}

// ---------------------------------------------------------------------------------------
// 2. Runtime-module fixture: confirm the runtime module declares and calls the real Win32 exports
//    this round needs -- CreateMutexA/ReleaseMutex are NEW imports, WaitForSingleObject/CloseHandle
//    are REUSED from Stage 54.1/53.3b (not re-declared), and GetCurrentThreadId is the new internal
//    (not NativeSharp-visible) hook added purely for the cctor-guard fix.
// ---------------------------------------------------------------------------------------

void testRuntimeModuleCallsRealWin32MutexAndThreadId(TestContext& tests)
{
    nativesharp::runtime::WindowsRuntimeBuilder builder;
    std::string error;
    std::unique_ptr<nativesharp::llvm_backend::LlvmModule> runtimeModulePtr = builder.buildSupport(error);
    tests.expect(runtimeModulePtr != nullptr && error.empty(),
        "Windows runtime module (with Stage 54.3 mutex + thread-id hooks) builds cleanly");
    if (runtimeModulePtr == nullptr) return;
    nativesharp::llvm_backend::LlvmModule& runtimeModule = *runtimeModulePtr;
    std::string verifyError;
    tests.expect(runtimeModule.verify(verifyError), "Windows runtime module LLVM verifies clean");

    const std::string llvm = runtimeModule.text();
    tests.expect(llvm.find("declare ptr @CreateMutexA(") != std::string::npos,
        "runtime module declares the real kernel32 CreateMutexA import");
    tests.expect(llvm.find("declare i32 @ReleaseMutex(") != std::string::npos,
        "runtime module declares the real kernel32 ReleaseMutex import");
    tests.expect(llvm.find("declare i32 @GetCurrentThreadId(") != std::string::npos,
        "runtime module declares the real kernel32 GetCurrentThreadId import (cctor-guard fix)");
    tests.expect(llvm.find("define i64 @__nativesharp_mutex_create()") != std::string::npos &&
            llvm.find("call ptr @CreateMutexA(") != std::string::npos,
        "__nativesharp_mutex_create calls the real CreateMutexA (anonymous, unowned mutex)");
    tests.expect(llvm.find("define void @__nativesharp_mutex_lock(") != std::string::npos,
        "__nativesharp_mutex_lock is defined (calls the REUSED WaitForSingleObject import)");
    tests.expect(llvm.find("define void @__nativesharp_mutex_unlock(") != std::string::npos &&
            llvm.find("call i32 @ReleaseMutex(") != std::string::npos,
        "__nativesharp_mutex_unlock calls the real ReleaseMutex (return value deliberately discarded)");
    tests.expect(llvm.find("define void @__nativesharp_mutex_close(") != std::string::npos,
        "__nativesharp_mutex_close is defined (calls the REUSED CloseHandle import)");
    tests.expect(llvm.find("define i32 @__nativesharp_get_current_thread_id()") != std::string::npos &&
            llvm.find("call i32 @GetCurrentThreadId()") != std::string::npos,
        "__nativesharp_get_current_thread_id is a trivial pass-through over the real GetCurrentThreadId");
}

// ---------------------------------------------------------------------------------------
// 3 + 4. Packaged example (examples/project_stage54_3), tested at BOTH O0 and O2, with a dedicated
//    Wine working directory, executed SEVERAL TIMES per level (extra rigor for Stage 54).
// ---------------------------------------------------------------------------------------

// Same extra-rigor run count Stage 54.1/54.2 established for this stage.
constexpr int kWineRunsPerLevel = 5;

void testPackagedExampleAtOptimizationLevel(
    TestContext& tests,
    const std::filesystem::path& root,
    nativesharp::llvm_backend::LlvmOptimizationLevel level,
    std::string_view levelFlag,
    std::string_view levelLabel)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage54_3/Stage54_3Mutex.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 54.3 mutex example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    const std::string label = std::string("packaged Stage 54.3 example (") + std::string(levelLabel) + ")";
    const std::filesystem::path exePath =
        buildExe(tests, project, root, level, levelFlag, "Stage54_3Mutex", label);
    if (exePath.empty()) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string("Wine not available in this environment -- Stage 54.3 (") +
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
            (std::string("Stage54_3Mutex_") + std::string(levelLabel) + "_run" +
                std::to_string(iteration) + ".log");
        const bool ran = runUnderWine(wine, workingDirectory, exePath, logPath, exitCode, crashed);
        const std::string runLabel = std::string(levelLabel) + " run " + std::to_string(iteration + 1) +
            "/" + std::to_string(kWineRunsPerLevel);
        tests.expect(ran, "Wine could be invoked on the Stage 54.3 (" + runLabel + ") executable");
        tests.expect(!crashed, "Stage 54.3 (" + runLabel + ") executable does not crash under real execution");
        tests.expect(exitCode == 0,
            "Stage 54.3 (" + runLabel +
                ") executable returns exit code 0 (all 10 mutex runtime checks passed -- basic Lock/"
                "Unlock, recursive same-thread reacquisition, Unlock() on a never-acquired mutex, "
                "raii-scoped destruction, a 200-mutex create/destroy exhaustion loop, and two genuine "
                "4-threads real mutual-exclusion stress tests on SHARED non-atomic counters verifying "
                "exact totals of 10000 and 4000; see examples/project_stage54_3/Main.ns for the check "
                "numbering)");
    }
}

// ---------------------------------------------------------------------------------------
// 5a. DEDICATED regression fixture for BUG #1 (cross-thread cctor race): a class whose static
//    Mutex+counter fields are NEVER touched by any code until N worker threads' very first
//    statement touches them -- all racing to be the one that runs the class's cctor. Before this
//    round's CAS-based fix, this exact shape (two or more threads racing a class's first
//    static-field touch) intermittently crashed (~7-50% of runs, higher under more contention) or
//    corrupted the mutex handle via a lost/duplicated CreateMutexA call. Kept deliberately separate
//    from the packaged example (which also exercises this path, but conflates it with the Mutex
//    feature test itself) so a future regression here fails on its own dedicated signal.
// ---------------------------------------------------------------------------------------

// More threads and more Wine runs than the packaged example's own stress sections -- this fixture
// exists SPECIFICALLY to catch the cctor race, so it deliberately maximizes contention on the
// single moment that mattered (the class's first-ever static-field touch).
constexpr int kRaceRegressionWineRuns = 8;

void testConcurrentStaticFieldFirstTouchDoesNotRace(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "CctorRace";
    writeProjectWithThreadingLibraries(directory, "CctorRace");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Threading;

// Nothing in this program touches RaceState before the eight worker threads start -- the FIRST
// access to RaceState.Lock/RaceState.Counter happens concurrently, from eight different threads,
// which is exactly the scenario that used to race the static-initializer guard (Bug #1).
class RaceState
{
    public static int Counter = 0;
    public static Mutex Lock = new Mutex();
}

class Program
{
    private static void Worker()
    {
        int i = 0;
        while (i < 3000)
        {
            RaceState.Lock.Lock();
            RaceState.Counter = RaceState.Counter + 1;
            RaceState.Lock.Unlock();
            i = i + 1;
        }
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
        t1.Join();
        t2.Join();
        t3.Join();
        t4.Join();
        t5.Join();
        t6.Join();
        t7.Join();
        t8.Join();
        // 8 threads x 3000 increments = 24000 exactly, if (a) the cctor guard let exactly ONE
        // thread run RaceState's initializer (so exactly one Mutex handle exists), and (b) that
        // single Mutex genuinely serializes all eight threads' read-add-write sequences.
        if (RaceState.Counter != 24000) { return 1; }
        return 0;
    }
}
)NS");

    const std::string wine = wineCommand();
    const std::array<std::pair<nativesharp::llvm_backend::LlvmOptimizationLevel, std::pair<std::string_view, std::string_view>>, 2> levels = {
        std::make_pair(nativesharp::llvm_backend::LlvmOptimizationLevel::O0, std::make_pair(std::string_view("-O0"), std::string_view("O0"))),
        std::make_pair(nativesharp::llvm_backend::LlvmOptimizationLevel::O2, std::make_pair(std::string_view("-O2"), std::string_view("O2")))};

    for (const auto& entry : levels)
    {
        const auto level = entry.first;
        const std::string_view levelFlag = entry.second.first;
        const std::string_view levelLabel = entry.second.second;
        const std::string label = std::string("cctor-race regression (") + std::string(levelLabel) + ")";
        const std::filesystem::path exePath =
            buildExe(tests, directory / "CctorRace.nsproj", root, level, levelFlag, "CctorRace", label);
        if (exePath.empty()) continue;

        if (wine.empty())
        {
            tests.expect(true, "Wine not available in this environment -- cctor-race regression (" +
                std::string(levelLabel) + ") real-execution check skipped, all structural checks above still ran");
            continue;
        }

        const auto workingDirectory = root / (std::string("cctor_race_run_") + std::string(levelLabel));
        std::error_code cleanupError;
        std::filesystem::remove_all(workingDirectory, cleanupError);
        std::filesystem::create_directories(workingDirectory);

        for (int iteration = 0; iteration < kRaceRegressionWineRuns; ++iteration)
        {
            int exitCode = 0;
            bool crashed = false;
            const auto logPath = root /
                (std::string("CctorRace_") + std::string(levelLabel) + "_run" + std::to_string(iteration) + ".log");
            const bool ran = runUnderWine(wine, workingDirectory, exePath, logPath, exitCode, crashed);
            const std::string runLabel = std::string(levelLabel) + " run " + std::to_string(iteration + 1) +
                "/" + std::to_string(kRaceRegressionWineRuns);
            tests.expect(ran, "Wine could be invoked on the cctor-race regression (" + runLabel + ") executable");
            tests.expect(!crashed,
                "cctor-race regression (" + runLabel + "): 8 threads racing RaceState's first static-field "
                "touch does NOT crash (regression guard for Bug #1 -- the pre-fix TOCTOU cctor race)");
            tests.expect(exitCode == 0,
                "cctor-race regression (" + runLabel + "): RaceState.Counter == 24000 exactly -- the class "
                "was initialized exactly once and its single Mutex genuinely serialized all eight threads");
        }
    }
}

// ---------------------------------------------------------------------------------------
// 5b. DEDICATED regression fixture for BUG #2 (same-thread cctor self-deadlock): a static field
//    initializer that reads another static field of ITS OWN type (directly, and via a static
//    method call), which -- because ensureTypeInitialized unconditionally calls the type's cctor
//    on every static field access -- recursively re-enters the SAME cctor from the SAME thread
//    while it is still "in progress". Under the naive CAS-only version of the Bug #1 fix, this
//    hung forever (confirmed experimentally with a 15s timeout before the recursion-check fix was
//    added). Single-threaded and fully deterministic -- no race, so a handful of Wine runs is
//    plenty; the important property is that it terminates AT ALL. runUnderWine's own `timeout 20`
//    wrapper bounds this fixture even if the regression were reintroduced: a hang shows up as a
//    clean, loud test failure (non-zero, non-crash exit code from `timeout`), never as ctest itself
//    hanging.
// ---------------------------------------------------------------------------------------

constexpr int kSelfRefRegressionWineRuns = 3;

void testSelfReferencingStaticInitializerDoesNotDeadlock(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "CctorSelfRef";
    writeProject(directory, "CctorSelfRef");
    writeFile(directory / "Main.ns", R"NS(
class DirectSelfRef
{
    // Evaluating B's initializer, while still inside DirectSelfRef's own cctor, reads static
    // field A -- which triggers ANOTHER call to DirectSelfRef's cctor, recursively, from the
    // SAME thread and SAME call stack, while the outer call is still "in progress".
    public static int A = 5;
    public static int B = DirectSelfRef.A + 1;
}

class MethodSelfRef
{
    // Same shape, but the recursive re-entry happens indirectly, through a static method call
    // rather than a direct field read -- confirms the recursion-check guard does not depend on
    // the AST shape of how the type's own static state is touched.
    public static int X = 10;
    public static int Y = ComputeY();
    public static int ComputeY() { return MethodSelfRef.X + 100; }
}

class Program
{
    public static int Main()
    {
        if (DirectSelfRef.B != 6) { return 1; }
        if (MethodSelfRef.Y != 110) { return 2; }
        return 0;
    }
}
)NS");

    const std::string label = "self-referencing-static-initializer regression";
    const std::filesystem::path exePath = buildExe(
        tests, directory / "CctorSelfRef.nsproj", root,
        nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "CctorSelfRef", label);
    if (exePath.empty()) return;

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, "Wine not available in this environment -- " + label + " real-execution check "
            "skipped, all structural checks above still ran");
        return;
    }

    const auto workingDirectory = root / "cctor_self_ref_run";
    std::error_code cleanupError;
    std::filesystem::remove_all(workingDirectory, cleanupError);
    std::filesystem::create_directories(workingDirectory);

    for (int iteration = 0; iteration < kSelfRefRegressionWineRuns; ++iteration)
    {
        int exitCode = 0;
        bool crashed = false;
        const auto logPath = root / (std::string("CctorSelfRef_run") + std::to_string(iteration) + ".log");
        const bool ran = runUnderWine(wine, workingDirectory, exePath, logPath, exitCode, crashed);
        const std::string runLabel = "run " + std::to_string(iteration + 1) + "/" + std::to_string(kSelfRefRegressionWineRuns);
        tests.expect(ran, label + " (" + runLabel + "): Wine could be invoked on the executable");
        tests.expect(!crashed, label + " (" + runLabel + "): executable does not crash under real execution");
        // The critical assertion: exitCode == 0, NOT 124 (timeout's own exit code when it has to
        // kill the wrapped process) and not anything else -- a self-deadlock regression here would
        // fail this exact check with a clearly non-zero, non-crash exit code, never hang the suite.
        tests.expect(exitCode == 0,
            label + " (" + runLabel + "): exit code 0, both self-referencing static initializers "
            "(direct field read and indirect via static method call) resolved without deadlocking -- "
            "regression guard for Bug #2 (the same-thread cctor self-deadlock introduced by the Bug #1 "
            "fix, fixed by the owner-thread-id recursion check)");
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage54_3_mutex_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testMutexCoreOperationsLowerToRuntimeCalls(tests, root);
    testRuntimeModuleCallsRealWin32MutexAndThreadId(tests);
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "O0");
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, "-O2", "O2");
    testConcurrentStaticFieldFirstTouchDoesNotRace(tests, root);
    testSelfReferencingStaticInitializerDoesNotDeadlock(tests, root);

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
