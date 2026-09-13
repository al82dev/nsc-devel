// Stage 54.2 "Atomics" -- NativeSharp.Core.Threading.AtomicInt32/AtomicInt64.
//
// DESIGN PIVOT DURING THIS ROUND (see lib/NativeSharp.Core.Threading/Atomics.ns's header
// comment and `58_stage54_2_atomics.md` for the full discussion): the ORIGINAL plan for this
// round was a direct C#-style `Interlocked.Increment(int* location)` static-method API,
// substituting NativeSharp's existing raw-pointer syntax for C#'s `ref`. That plan was
// abandoned during implementation upon discovering that NativeSharp's modern OOP LLVM lowering
// pipeline (every stage since 41, this one included) does not lower the `&`/`*` operators AT
// ALL -- see ProjectOopHirModel.cpp's UnaryExpression case. The final design instead uses two
// INSTANCE-method wrapper classes (AtomicInt32/AtomicInt64, mirroring java.util.concurrent.
// atomic.AtomicInteger/AtomicLong) where each of the ten core operations implicitly addresses
// the RECEIVER's own private `value_` field -- resolved entirely inside the compiler (a
// synthetic ProjectOopHirNodeKind::FieldAddress node, see MethodCoreBinding::
// implicitFieldMemberOrdinal in ProjectMethodBodyModel.h), with zero reliance on the blocked
// `&` operator.
//
// A SECOND pivot happened one layer down: the plan for the RUNTIME implementation was also
// originally "wrap the real kernel32.dll InterlockedIncrement/InterlockedExchangeAdd/
// InterlockedExchange/InterlockedCompareExchange exports (and their *64 counterparts)", the same
// "wrap one real Win32 function" pattern every prior core operation in this compiler uses --
// abandoned upon confirming (via mingw-w64's psdk_inc/intrin-impl.h, mirroring the Microsoft SDK)
// that the 64-bit Interlocked*64 family is NOT exported by kernel32.dll at all on x64; only the
// 32-bit family is. Rather than have an asymmetric Int32-calls-Win32/Int64-cannot design, BOTH
// widths now emit real LLVM atomic IR instructions directly (LLVMBuildAtomicRMW/
// LLVMBuildAtomicCmpXchg, lowering to `lock xadd`/`lock xchg`/`lock cmpxchg` on x86-64) -- see
// buildInterlockedIncrementOrDecrement's header comment in WindowsRuntimeBuilder.cpp. No new
// Win32 imports/kernel32Symbols entries were needed for this round as a direct consequence.
//
// Covers, in increasing order of confidence, exactly like every other Stage 53/54
// real-execution test file in this suite:
//   1. That the ten core operations (Increment/Decrement/Add/Exchange/CompareExchange x
//      Int32/Int64) are recognized, lower to LLVM, and each emits a call to its exact runtime
//      hook symbol -- a minimal hand-written fixture, independent of the packaged example.
//   2. That the packaged example project (examples/project_stage54_2) compiles end to end
//      through a real linked Windows x64 PE executable via the actual `nsc` Driver, AT BOTH O0
//      and O2 -- same discipline every Stage 53/54 round has followed since 53.2b.
//   3. REAL EXECUTION under Wine (when available, gracefully skipped otherwise), at BOTH
//      optimization levels: Main.ns returns 0 only if every one of its 38 runtime checks passed
//      (see examples/project_stage54_2/Main.ns for the full numbering) -- the EXACT return-value
//      contract of all ten operations (including CompareExchange's "does not match" branch, not
//      just its happy path) for both widths, TWO genuine multi-threaded stress tests (4 threads x
//      2500 increments each, both Int32 and Int64, on a SHARED counter -- the real proof that
//      these operations are atomic: any lost update would make the final count come out lower
//      than the exact expected 10000), and a negative control field no code ever touches.
//
// EXTRA RIGOR FOR THIS ROUND (Alberto's explicit instruction, recorded in
// claude/05_metodo_de_trabajo_y_archivo_historico.md's "Regla de rigor de testing: Stage 54"
// section, carried over unchanged from Stage 54.1): the real-execution check below runs the
// packaged example under Wine SEVERAL TIMES per optimization level, and requires exit code 0
// with no crash on EVERY single run -- a concurrency bug that only shows up on some scheduling
// interleavings would otherwise slip through a single run undetected. No Sleep() is used anywhere
// as a synchronization substitute, in this test file or in Main.ns itself -- every ordering
// guarantee this suite depends on comes from Thread.Join() (WaitForSingleObject, a real
// kernel-level memory barrier) plus the atomic instructions themselves, never from timing.

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

bool commandAvailable(std::string_view command)
{
#ifdef _WIN32
    return std::system(("where " + std::string(command) + " >nul 2>nul").c_str()) == 0;
#else
    return std::system(("command -v " + std::string(command) + " >/dev/null 2>&1").c_str()) == 0;
#endif
}

// Same fallback order as every other Stage 52/53/54.1 real-execution test file in this repo.
std::string wineCommand()
{
    if (std::filesystem::exists("/usr/lib/wine/wine64")) return "/usr/lib/wine/wine64";
    if (commandAvailable("wine64")) return "wine64";
    if (commandAvailable("wine")) return "wine";
    return {};
}

// Portable (no POSIX-specific code) -- same std::system()-based invocation every other Stage 53/
// 54.1 real-execution test file in this repo uses. Per Alberto's explicit, repeated instruction
// this project's own C++ source (tests included) must contain zero Unix/Linux-specific code.
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
// 1. Minimal fixture: the ten core operations lower to LLVM and each emits a call to its exact
//    runtime hook symbol. Redefines minimal AtomicInt32/AtomicInt64 classes under the EXACT
//    fully-qualified names ProjectMethodBodyModel.cpp's recognition block matches on
//    (NativeSharp.Core.Threading.AtomicInt32/AtomicInt64) -- recognition is by class+name+
//    signature only, independent of which project/file the class is physically declared in,
//    exactly like Stage 54.1's ThreadCore fixture above reuses the same technique for Thread.
// ---------------------------------------------------------------------------------------

void testAtomicCoreOperationsLowerToRuntimeCalls(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "AtomicCore";
    writeProject(directory, "AtomicCore");
    writeFile(directory / "Atomics.ns", R"NS(
namespace NativeSharp.Core.Threading
{
    public enum MemoryOrder
    {
        Relaxed = 0,
        Acquire = 1,
        Release = 2,
        AcquireRelease = 3,
        SequentiallyConsistent = 4
    }

    public class AtomicInt32
    {
        private int value_;
        public AtomicInt32(int initialValue) { value_ = initialValue; }
        public int Load() { return 0; }
        public int Load(MemoryOrder order) { return 0; }
        public void Store(int value) { }
        public void Store(int value, MemoryOrder order) { }
        public int Increment() { return 0; }
        public int Decrement() { return 0; }
        public int Add(int value) { return 0; }
        public int Exchange(int value) { return 0; }
        public int CompareExchange(int value, int comparand) { return 0; }
    }

    public class AtomicInt64
    {
        private long value_;
        public AtomicInt64(long initialValue) { value_ = initialValue; }
        public long Load() { return 0; }
        public long Load(MemoryOrder order) { return 0; }
        public void Store(long value) { }
        public void Store(long value, MemoryOrder order) { }
        public long Increment() { return 0; }
        public long Decrement() { return 0; }
        public long Add(long value) { return 0; }
        public long Exchange(long value) { return 0; }
        public long CompareExchange(long value, long comparand) { return 0; }
    }
}
)NS");
    writeFile(directory / "Main.ns", R"NS(
using NativeSharp.Core.Threading;

class Program
{
    public static int Run32(AtomicInt32 a, int value, int comparand)
    {
        int r = a.Load();
        a.Store(value);
        r = a.Increment();
        r = a.Decrement();
        r = a.Add(value);
        r = a.Exchange(value);
        r = a.CompareExchange(value, comparand);
        return r;
    }

    public static long Run64(AtomicInt64 a, long value, long comparand)
    {
        long r = a.Load();
        a.Store(value);
        r = a.Increment();
        r = a.Decrement();
        r = a.Add(value);
        r = a.Exchange(value);
        r = a.CompareExchange(value, comparand);
        return r;
    }

    public static long RunOrdered(AtomicInt32 a32, AtomicInt64 a64)
    {
        a32.Store(1, MemoryOrder.Relaxed);
        a32.Store(2, MemoryOrder.Release);
        a32.Store(3, MemoryOrder.SequentiallyConsistent);
        int r32 = a32.Load(MemoryOrder.Relaxed);
        r32 = a32.Load(MemoryOrder.Acquire);
        r32 = a32.Load(MemoryOrder.SequentiallyConsistent);
        a64.Store(4, MemoryOrder.Relaxed);
        a64.Store(5, MemoryOrder.Release);
        a64.Store(6, MemoryOrder.SequentiallyConsistent);
        long r64 = a64.Load(MemoryOrder.Relaxed);
        r64 = a64.Load(MemoryOrder.Acquire);
        r64 = a64.Load(MemoryOrder.SequentiallyConsistent);
        return r64 + r32;
    }
}
)NS");

    std::string error;
    ProjectCompilation mir;
    const bool ok = mir.compileProjectOopMir(directory / "AtomicCore.nsproj", error);
    tests.expect(ok && error.empty(), "Atomic core-operation fixture reaches OOP MIR");
    if (!ok || mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(), "Atomic core-operation fixture lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "Atomic core-operation fixture LLVM verifies clean");

    const std::string llvm = module->text();
    const std::array<std::string_view, 14> expectedCalls = {
        "call i32 @__nativesharp_atomic_load_int32",
        "call void @__nativesharp_atomic_store_int32",
        "call i32 @__nativesharp_interlocked_increment_int32",
        "call i32 @__nativesharp_interlocked_decrement_int32",
        "call i32 @__nativesharp_interlocked_add_int32",
        "call i32 @__nativesharp_interlocked_exchange_int32",
        "call i32 @__nativesharp_interlocked_compare_exchange_int32",
        "call i64 @__nativesharp_atomic_load_int64",
        "call void @__nativesharp_atomic_store_int64",
        "call i64 @__nativesharp_interlocked_increment_int64",
        "call i64 @__nativesharp_interlocked_decrement_int64",
        "call i64 @__nativesharp_interlocked_add_int64",
        "call i64 @__nativesharp_interlocked_exchange_int64",
        "call i64 @__nativesharp_interlocked_compare_exchange_int64"};
    for (std::string_view expected : expectedCalls)
    {
        tests.expect(llvm.find(expected) != std::string::npos,
            "Atomic core operation lowers to a direct call to " + std::string(expected));
    }
    tests.expect(
        llvm.find("load atomic i32") != std::string::npos &&
            llvm.find("load atomic i64") != std::string::npos &&
            llvm.find("acquire") != std::string::npos,
        "ordered AtomicInt32/AtomicInt64 loads lower to native acquire instructions");
    tests.expect(
        llvm.find("store atomic i32") != std::string::npos &&
            llvm.find("store atomic i64") != std::string::npos &&
            llvm.find("release") != std::string::npos,
        "ordered AtomicInt32/AtomicInt64 stores lower to native release instructions");
    tests.expect(llvm.find("monotonic") != std::string::npos,
        "MemoryOrder.Relaxed maps explicitly to LLVM monotonic ordering");
    // Also confirm the "location" operand feeding each of those calls is a field-address GEP
    // (`field.addr`), not some other addressing scheme -- proof that the synthetic FieldAddress
    // node this round introduces (see ProjectOopHirModel.cpp's dedicated Atomics block) is really
    // being used, and not silently bypassed.
    tests.expect(llvm.find("field.addr") != std::string::npos,
        "Atomic core operations address their implicit `value_` field via the shared field-address GEP machinery");
}
void testOrderedAtomicDiagnostics(TestContext& tests, const std::filesystem::path& root)
{
    const auto directory = root / "AtomicOrderReject";
    writeProject(directory, "AtomicOrderReject");
    writeFile(directory / "Main.ns", R"NS(
namespace NativeSharp.Core.Threading
{
    public enum MemoryOrder
    {
        Relaxed = 0,
        Acquire = 1,
        Release = 2,
        AcquireRelease = 3,
        SequentiallyConsistent = 4
    }

    public class AtomicInt32
    {
        private int value_;
        public AtomicInt32(int initialValue) { value_ = initialValue; }
        public int Load(MemoryOrder order) { return 0; }
        public void Store(int value, MemoryOrder order) { }
    }
}

using NativeSharp.Core.Threading;

class Program
{
    public static int Main()
    {
        raii AtomicInt32 value = new AtomicInt32(0);
        value.Load(MemoryOrder.Release);
        value.Store(1, MemoryOrder.Acquire);
        MemoryOrder dynamicOrder = MemoryOrder.Relaxed;
        value.Load(dynamicOrder);
        return 0;
    }
}
)NS");

    const std::string projectText = (directory / "AtomicOrderReject.nsproj").string();
    const std::string exeText = (directory / "AtomicOrderReject.exe").string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(args, out, err);
    const std::string diagnostics = err.str() + out.str();
    tests.expect(result != ExitCode::Success, "invalid ordered atomic operations are rejected");
    tests.expect(diagnostics.find(
        "atomic Load order must be Relaxed, Acquire, or SequentiallyConsistent") != std::string::npos,
        "ordered Load rejects Release");
    tests.expect(diagnostics.find(
        "atomic Store order must be Relaxed, Release, or SequentiallyConsistent") != std::string::npos,
        "ordered Store rejects Acquire");
    tests.expect(diagnostics.find(
        "atomic memory order must be a compile-time MemoryOrder constant") != std::string::npos,
        "ordered atomic operations reject runtime-selected ordering");
}

// ---------------------------------------------------------------------------------------
// 2. Runtime-module fixture: confirm the actual atomic HARDWARE instructions (not a Win32 call)
//    are what each of the ten runtime hooks emits -- see this file's header comment for why this
//    differs from the originally planned kernel32.dll-wrapping design.
// ---------------------------------------------------------------------------------------

void testRuntimeHooksEmitRealAtomicInstructions(TestContext& tests)
{
    // The real API (nativesharp::runtime::WindowsRuntimeBuilder, see
    // include/nativesharp/Runtime/WindowsRuntimeBuilder.h) builds the runtime as a fresh,
    // independent LlvmModule -- there is no LlvmModule::buildWindowsRuntime member function.
    // buildSupport() is the variant used by the library linker path (no "start"/entry-point
    // machinery), which is exactly what this fixture wants: it only cares about the ten
    // atomic runtime hooks' instruction bodies, not executable-entry wiring.
    nativesharp::runtime::WindowsRuntimeBuilder builder;
    std::string error;
    std::unique_ptr<nativesharp::llvm_backend::LlvmModule> runtimeModulePtr = builder.buildSupport(error);
    tests.expect(runtimeModulePtr != nullptr && error.empty(),
        "Windows runtime module (with Stage 54.2 atomics) builds cleanly");
    if (runtimeModulePtr == nullptr) return;
    nativesharp::llvm_backend::LlvmModule& runtimeModule = *runtimeModulePtr;
    std::string verifyError;
    tests.expect(runtimeModule.verify(verifyError), "Windows runtime module LLVM verifies clean");

    const std::string llvm = runtimeModule.text();
    tests.expect(llvm.find("atomicrmw add") != std::string::npos,
        "runtime module emits a real `atomicrmw add` instruction (Increment/Decrement/Add)");
    tests.expect(llvm.find("atomicrmw xchg") != std::string::npos,
        "runtime module emits a real `atomicrmw xchg` instruction (Exchange)");
    tests.expect(llvm.find("cmpxchg") != std::string::npos,
        "runtime module emits a real `cmpxchg` instruction (CompareExchange)");
    tests.expect(
        llvm.find("load atomic i32") != std::string::npos &&
            llvm.find("load atomic i64") != std::string::npos,
        "runtime module emits real atomic loads for both supported widths");
    tests.expect(
        llvm.find("store atomic i32") != std::string::npos &&
            llvm.find("store atomic i64") != std::string::npos,
        "runtime module emits real atomic stores for both supported widths");
    tests.expect(llvm.find("seq_cst") != std::string::npos,
        "Stage 54.2 atomics use sequentially-consistent ordering throughout");
    // Negative check: none of the ten runtime hooks should be calling into kernel32.dll's
    // Interlocked* family -- confirming the design pivot documented at the top of this file was
    // actually carried through, not just described.
    tests.expect(llvm.find("@InterlockedIncrement") == std::string::npos &&
            llvm.find("@InterlockedExchangeAdd") == std::string::npos &&
            llvm.find("@InterlockedExchange") == std::string::npos &&
            llvm.find("@InterlockedCompareExchange") == std::string::npos,
        "Stage 54.2 atomics do NOT call any kernel32.dll Interlocked* export -- real IR instructions only");
}

// ---------------------------------------------------------------------------------------
// 3 + 4. Packaged example (examples/project_stage54_2), tested at BOTH O0 and O2, with a
//    dedicated Wine working directory, executed SEVERAL TIMES per level (extra rigor for
//    Stage 54 -- see this file's header comment).
// ---------------------------------------------------------------------------------------

// Same extra-rigor run count Stage 54.1 established for this stage.
constexpr int kWineRunsPerLevel = 5;

void testPackagedExampleAtOptimizationLevel(
    TestContext& tests,
    const std::filesystem::path& root,
    nativesharp::llvm_backend::LlvmOptimizationLevel level,
    std::string_view levelFlag,
    std::string_view levelLabel)
{
    const std::filesystem::path project =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples/project_stage54_2/Stage54_2Atomics.nsproj";
    tests.expect(std::filesystem::exists(project),
        "packaged Stage 54.2 atomics example project exists in the source tree");
    if (!std::filesystem::exists(project)) return;

    std::string error;
    ProjectCompilation bodies;
    tests.expect(bodies.compileProjectMethodBodies(project, error),
        std::string("packaged Stage 54.2 example (") + std::string(levelLabel) +
            ") passes method-body ownership analysis");
    tests.expect(error.empty(), "packaged Stage 54.2 method-body analysis has no loader error");

    error.clear();
    ProjectCompilation mir;
    tests.expect(mir.compileProjectOopMir(project, error),
        std::string("packaged Stage 54.2 example (") + std::string(levelLabel) + ") lowers to OOP MIR");
    tests.expect(error.empty(), "packaged Stage 54.2 MIR lowering has no loader/lowering error");
    if (mir.nameLookup() == nullptr || mir.oopMirModel() == nullptr) return;

    nativesharp::llvm_backend::ProjectOopLlvmLowerer lowerer;
    auto module = lowerer.lower(*mir.nameLookup(), *mir.oopMirModel());
    tests.expect(module != nullptr && lowerer.error().empty(),
        std::string("packaged Stage 54.2 example (") + std::string(levelLabel) + ") lowers to LLVM");
    if (module == nullptr) return;
    std::string verifyError;
    tests.expect(module->verify(verifyError), "packaged Stage 54.2 example LLVM verifies clean");

    std::string targetError;
    auto target = nativesharp::llvm_backend::LlvmTargetMachine::createWindowsX64(level, targetError);
    tests.expect(target != nullptr && targetError.empty(),
        std::string("Stage 54.2 example (") + std::string(levelLabel) + ") obtains Windows x64 target machine");
    if (target == nullptr) return;
    tests.expect(target->configureModuleForTarget(*module, targetError),
        "Stage 54.2 example applies Windows x64 data layout");

    const auto object = root / (std::string("nativesharp_stage54_2_example_") + std::string(levelLabel) + ".obj");
    tests.expect(target->emit(*module, object, nativesharp::llvm_backend::LlvmCodeGenFileType::Object, targetError),
        "Stage 54.2 example emits Windows x64 COFF");
    nativesharp::llvm_backend::CoffObjectVerifier verifier;
    tests.expect(verifier.verify(object), "Stage 54.2 example object passes structural COFF verification");
    tests.expect(verifier.info().machine == 0x8664, "Stage 54.2 example object machine is AMD64 / 0x8664");

    const auto exePath = root / (std::string("Stage54_2Atomics_") + std::string(levelLabel) + ".exe");
    const std::string projectText = project.string();
    const std::string exeText = exePath.string();
    const std::array<std::string_view, 5> args = {
        "--emit-project-oop-exe", projectText, levelFlag, "-o", exeText};
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode driverResult = driver.run(args, out, err);
    tests.expect(driverResult == ExitCode::Success,
        std::string("Stage 54.2 example (") + std::string(levelLabel) +
            "): Driver emits a real Windows x64 PE executable (" + err.str() + ")");
    tests.expect(err.str().empty(),
        std::string("Stage 54.2 example (") + std::string(levelLabel) + "): no Driver diagnostics");
    tests.expect(std::filesystem::is_regular_file(exePath),
        std::string("Stage 54.2 example (") + std::string(levelLabel) + "): .exe file exists on disk");

    const std::string wine = wineCommand();
    if (wine.empty())
    {
        tests.expect(true, std::string("Wine not available in this environment -- Stage 54.2 (") +
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
            (std::string("Stage54_2Atomics_") + std::string(levelLabel) + "_run" +
                std::to_string(iteration) + ".log");
        const bool ran = runUnderWine(wine, workingDirectory, exePath, logPath, exitCode, crashed);
        const std::string runLabel = std::string(levelLabel) + " run " + std::to_string(iteration + 1) +
            "/" + std::to_string(kWineRunsPerLevel);
        tests.expect(ran, "Wine could be invoked on the Stage 54.2 (" + runLabel + ") executable");
        tests.expect(!crashed, "Stage 54.2 (" + runLabel + ") executable does not crash under real execution");
        tests.expect(exitCode == 0,
            "Stage 54.2 (" + runLabel +
                ") executable returns exit code 0 (all 38 atomics runtime checks passed -- exact "
                "return-value contract for Increment/Decrement/Add/Exchange/CompareExchange on both "
                "Int32 and Int64, including CompareExchange's non-matching branch, plus two genuine "
                "4-threads-x-2500-increments stress tests on a SHARED counter verifying the exact "
                "expected total of 10000, and the untouched-field negative control; see "
                "examples/project_stage54_2/Main.ns for the check numbering)");
    }
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::temp_directory_path() / "nativesharp_stage54_2_atomics_tests";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    testAtomicCoreOperationsLowerToRuntimeCalls(tests, root);
    testOrderedAtomicDiagnostics(tests, root);
    testRuntimeHooksEmitRealAtomicInstructions(tests);
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O0, "-O0", "O0");
    testPackagedExampleAtOptimizationLevel(
        tests, root, nativesharp::llvm_backend::LlvmOptimizationLevel::O2, "-O2", "O2");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
