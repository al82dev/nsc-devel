#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Language/MemoryModel.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

constexpr std::string_view atomics = R"NS(
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

    public static class AtomicFence
    {
        public static void Thread(MemoryOrder order) { }
        public static void Compiler(MemoryOrder order) { }
    }

    public class AtomicInt32
    {
        private int value_;
        public AtomicInt32(int value) { value_ = value; }
        public int Load(MemoryOrder order) { return 0; }
        public void Store(int value, MemoryOrder order) { }
        public int Add(int value, MemoryOrder order) { return 0; }
        public int CompareExchange(
            int value, int comparand,
            MemoryOrder successOrder, MemoryOrder failureOrder) { return 0; }
    }
}
)NS";

void writeProject(const std::filesystem::path& root, std::string_view program)
{
    writeFile(root / "Memory.nsproj",
        "<Project><PropertyGroup><Name>MemoryModelT10F07</Name><Language>3</Language>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(root / "Atomics.ns", atomics);
    writeFile(root / "Main.ns", program);
}
}

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::language;
    tests::TestContext tests;

    tests.expect(dataRaceBehavior() == DataRaceBehavior::UndefinedBehavior,
        "ordinary data races are explicitly undefined behavior");
    tests.expect(validMemoryOrder(AtomicOperation::Load, MemoryOrder::Acquire) &&
            !validMemoryOrder(AtomicOperation::Load, MemoryOrder::Release) &&
            validMemoryOrder(AtomicOperation::Store, MemoryOrder::Release) &&
            !validMemoryOrder(AtomicOperation::Store, MemoryOrder::Acquire) &&
            !validMemoryOrder(AtomicOperation::Fence, MemoryOrder::Relaxed),
        "one compiler-owned matrix validates load, store and fence orderings");
    tests.expect(validCompareExchangeOrders(
            MemoryOrder::AcquireRelease, MemoryOrder::Acquire) &&
            !validCompareExchangeOrders(MemoryOrder::Relaxed, MemoryOrder::Acquire),
        "compare-exchange failure ordering cannot be stronger than success ordering");
    tests.expect(atomicAccessIsLockFreeWindowsX64(32, true) &&
            atomicAccessIsLockFreeWindowsX64(64, true) &&
            !atomicAccessIsLockFreeWindowsX64(64, false),
        "Windows x64 lock-free atomic widths require natural alignment");
    tests.expect(!volatileProvidesSynchronization() &&
            !volatileAccessMayTear(32, true) && volatileAccessMayTear(128, true),
        "volatile is observable, not synchronization, and wide accesses may tear");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t10_f07_memory_model";
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    const std::filesystem::path valid = root / "valid";
    writeProject(valid, R"NS(
using NativeSharp.Core.Threading;

public class Program
{
    public static int Main()
    {
        AtomicInt32 value = new AtomicInt32(1);
        value.Store(40, MemoryOrder.Release);
        AtomicFence.Thread(MemoryOrder.AcquireRelease);
        if (value.Load(MemoryOrder.Acquire) != 40) { return 1; }
        if (value.Add(2, MemoryOrder.Relaxed) != 42) { return 2; }
        if (value.CompareExchange(
                7, 42, MemoryOrder.AcquireRelease, MemoryOrder.Acquire) != 42) { return 3; }
        volatile int observed = 7;
        observed = observed + 1;
        return value.Load(MemoryOrder.SequentiallyConsistent) == 7 && observed == 8 ? 0 : 4;
    }
}
)NS");
    const std::string project = (valid / "Memory.nsproj").string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 3> dumpArgs = {
        "--dump-project-oop-llvm", project, "-O0"};
    const bool dumped = runDriver(dumpArgs, output, error) == ExitCode::Success;
    tests.expect(dumped, "edition-3 memory fixture emits O0 LLVM (" + error + ")");
    tests.expect(dumped && output.find("store atomic i32 40") != std::string::npos &&
            output.find("release") != std::string::npos &&
            output.find("load atomic i32") != std::string::npos &&
            output.find("acquire") != std::string::npos &&
            output.find("fence acq_rel") != std::string::npos,
        "O0 lowering preserves release/acquire/fence semantics");
    tests.expect(dumped && output.find("load volatile i32") != std::string::npos &&
            output.find("store volatile i32") != std::string::npos,
        "O0 lowering keeps volatile loads/stores observable without making them atomic");

    const std::string executable = (valid / "Memory.exe").string();
    const std::array<std::string_view, 5> exeArgs = {
        "-O0", "--emit-project-oop-exe", project, "-o", executable};
    output.clear();
    error.clear();
    const bool emitted = runDriver(exeArgs, output, error) == ExitCode::Success;
    tests.expect(emitted, "memory fixture links at O0 through configured NSLink (" + error + ")");
    if (emitted)
        tests.expect(std::system(("\"" + executable + "\"").c_str()) == 0,
            "release/acquire/RMW behavior executes deterministically");

    const std::filesystem::path invalid = root / "invalid";
    writeProject(invalid, R"NS(
using NativeSharp.Core.Threading;
public class Program
{
    public static int Main()
    {
        AtomicInt32 value = new AtomicInt32(0);
        value.Load(MemoryOrder.Release);
        value.Store(1, MemoryOrder.Acquire);
        value.CompareExchange(2, 1, MemoryOrder.Relaxed, MemoryOrder.Acquire);
        AtomicFence.Thread(MemoryOrder.Relaxed);
        return 0;
    }
}
)NS");
    const std::string invalidProject = (invalid / "Memory.nsproj").string();
    const std::array<std::string_view, 3> invalidArgs = {
        "--dump-project-oop-hir", invalidProject, "-O0"};
    output.clear();
    error.clear();
    tests.expect(runDriver(invalidArgs, output, error) != ExitCode::Success,
        "invalid atomic orderings are rejected by semantic analysis");
    tests.expect(error.find("atomic Load order must be") != std::string::npos &&
            error.find("atomic Store order must be") != std::string::npos &&
            error.find("failure order cannot be stronger") != std::string::npos &&
            error.find("atomic fence order must be") != std::string::npos,
        "each illegal operation reports its precise memory-order contract");

    return tests.resultKeeping(root);
}
