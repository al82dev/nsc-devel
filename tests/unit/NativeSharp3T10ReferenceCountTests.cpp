#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

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
std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

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

std::filesystem::path writeProject(
    const std::filesystem::path& root,
    std::string_view name,
    std::string_view program,
    bool includeThread)
{
    writeFile(root / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    const std::filesystem::path threading =
        std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core.Threading";
    writeFile(root / "Atomics.ns", readFile(threading / "Atomics.ns"));
    writeFile(root / "ReferenceCounting.ns", readFile(threading / "ReferenceCounting.ns"));
    if (includeThread) writeFile(root / "Thread.ns", readFile(threading / "Thread.ns"));
    writeFile(root / "Main.ns", program);
    return root / (std::string(name) + ".nsproj");
}
}

int main()
{
    using nativesharp::ExitCode;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t10_f08_reference_counts";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) /
            "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path noAllocation = writeProject(
        root / "no-allocation", "RefCountNoAllocation", R"NS(
using NativeSharp.Core.Threading;
public class Program
{
    public static int Main()
    {
        ReferenceCounts counts = new ReferenceCounts(1);
        if (!counts.IsValid() || counts.StrongCount() != 1 || counts.WeakCount() != 0) { return 1; }
        if (!counts.TryRetainStrong() || !counts.TryRetainWeak()) { return 2; }
        if (counts.StrongCount() != 2 || counts.WeakCount() != 1) { return 3; }
        if (counts.ReleaseStrong() != ReferenceRelease.Retained) { return 4; }
        if (counts.ReleaseStrong() != ReferenceRelease.Final) { return 5; }
        if (counts.TryRetainStrong()) { return 6; }
        if (counts.ReleaseStrong() != ReferenceRelease.Invalid) { return 7; }
        if (counts.ReleaseWeak() != ReferenceRelease.Retained) { return 8; }
        if (counts.ReleaseWeak() != ReferenceRelease.Final) { return 9; }
        return counts.ReleaseWeak() == ReferenceRelease.Invalid ? 0 : 10;
    }
}
)NS", false);
    const std::string noAllocationProject = noAllocation.string();
    std::string output;
    std::string error;
    const std::array<std::string_view, 3> dumpArgs = {
        "--dump-project-oop-llvm", noAllocationProject, "-O0"};
    const bool dumped = runDriver(dumpArgs, output, error) == ExitCode::Success;
    tests.expect(dumped, "reference-count primitive emits O0 LLVM (" + error + ")");
    tests.expect(dumped && output.find("cmpxchg ptr") != std::string::npos &&
            output.find("release monotonic") != std::string::npos &&
            output.find("fence acquire") != std::string::npos,
        "retain/release lower to CAS with release plus final acquire fence");
    tests.expect(dumped && output.find("call ptr @__nativesharp_alloc") == std::string::npos,
        "embeddable ReferenceCounts operations introduce no allocation");

    const std::string noAllocationExe = (root / "no-allocation/RefCount.exe").string();
    const std::array<std::string_view, 5> noAllocationExeArgs = {
        "-O0", "--emit-project-oop-exe", noAllocationProject, "-o", noAllocationExe};
    output.clear();
    error.clear();
    const bool noAllocationLinked =
        runDriver(noAllocationExeArgs, output, error) == ExitCode::Success;
    tests.expect(noAllocationLinked,
        "reference-count primitive links at O0 with NSLink (" + error + ")");
    if (noAllocationLinked)
        tests.expect(std::system(("\"" + noAllocationExe + "\"").c_str()) == 0,
            "strong/weak counts reject resurrection and underflow");

    const std::filesystem::path concurrent = writeProject(
        root / "concurrent", "RefCountConcurrent", R"NS(
using NativeSharp.Core.Threading;
public class Program
{
    public static ReferenceCounts Counts = new ReferenceCounts(2);
    public static AtomicInt32 Destroyed = new AtomicInt32(0);
    public static AtomicInt32 Deallocated = new AtomicInt32(0);

    public static void Destroy() { Destroyed.Increment(); }
    public static void Deallocate() { Deallocated.Increment(); }

    public static void Worker()
    {
        ReferenceCountHook destroyHook = Destroy;
        ReferenceCountHook deallocateHook = Deallocate;
        Counts.ReleaseStrongAndDestroy(destroyHook, deallocateHook);
    }

    public static int Main()
    {
        ThreadStart start = Worker;
        ReferenceCountHook destroyHook = Destroy;
        ReferenceCountHook deallocateHook = Deallocate;
        raii Thread worker = new Thread(start);
        Counts.ReleaseStrongAndDestroy(destroyHook, deallocateHook);
        worker.Join();
        if (Destroyed.Load(MemoryOrder.Acquire) != 1) { return 1; }
        if (Deallocated.Load(MemoryOrder.Acquire) != 1) { return 2; }
        return Counts.StrongCount() == 0 && Counts.WeakCount() == 0 ? 0 : 3;
    }
}
)NS", true);
    const std::string concurrentProject = concurrent.string();
    const std::string concurrentExe = (root / "concurrent/RefCountConcurrent.exe").string();
    const std::array<std::string_view, 5> concurrentArgs = {
        "-O0", "--emit-project-oop-exe", concurrentProject, "-o", concurrentExe};
    output.clear();
    error.clear();
    const bool concurrentLinked = runDriver(concurrentArgs, output, error) == ExitCode::Success;
    tests.expect(concurrentLinked,
        "concurrent final-release fixture links at O0 with NSLink (" + error + ")");
    if (concurrentLinked)
        tests.expect(std::system(("\"" + concurrentExe + "\"").c_str()) == 0,
            "two racing releases invoke destroy and deallocate exactly once");

    const std::filesystem::path overflow = writeProject(
        root / "overflow", "RefCountOverflow", R"NS(
using NativeSharp.Core.Threading;
public class Program
{
    public static int Main()
    {
        ReferenceCounts counts = new ReferenceCounts(9223372036854775807l);
        if (counts.TryRetainStrong()) { return 1; }
        ReferenceCounts invalid = new ReferenceCounts(0);
        if (invalid.IsValid() || invalid.TryRetainStrong() || invalid.TryRetainWeak()) { return 2; }
        return 0;
    }
}
)NS", false);
    const std::string overflowProject = overflow.string();
    const std::string overflowExe = (root / "overflow/RefCountOverflow.exe").string();
    const std::array<std::string_view, 5> overflowArgs = {
        "-O0", "--emit-project-oop-exe", overflowProject, "-o", overflowExe};
    output.clear();
    error.clear();
    const bool overflowLinked = runDriver(overflowArgs, output, error) == ExitCode::Success;
    tests.expect(overflowLinked, "overflow fixture links at O0 with NSLink (" + error + ")");
    if (overflowLinked)
        tests.expect(std::system(("\"" + overflowExe + "\"").c_str()) == 0,
            "retain detects overflow and invalid zero initialization before mutation");

    const std::filesystem::path producer = root / "dll/producer";
    const std::filesystem::path threading =
        std::filesystem::path(NSC_SOURCE_DIR) / "lib/NativeSharp.Core.Threading";
    writeFile(producer / "ReferenceCountProducer.nsproj",
        "<Project><PropertyGroup><Name>ReferenceCountProducer</Name>"
        "<OutputType>SharedLibrary</OutputType><Language>3</Language></PropertyGroup>"
        "<ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Atomics.ns", readFile(threading / "Atomics.ns"));
    writeFile(producer / "ReferenceCounting.ns", readFile(threading / "ReferenceCounting.ns"));
    const std::string producerProject = (producer / "ReferenceCountProducer.nsproj").string();
    const std::string producerDll = (producer / "ReferenceCountProducer.dll").string();
    const std::array<std::string_view, 5> producerArgs = {
        "--emit-project-oop-dll", producerProject, "-O0", "-o", producerDll};
    output.clear();
    error.clear();
    const bool producerLinked = runDriver(producerArgs, output, error) == ExitCode::Success;
    tests.expect(producerLinked,
        "ReferenceCounts publishes a DLL/NSMETA pair at O0 through NSLink (" + error + ")");

    const std::filesystem::path consumer = root / "dll/consumer";
    writeFile(consumer / "ReferenceCountConsumer.nsproj",
        "<Project><PropertyGroup><Name>ReferenceCountConsumer</Name><Language>3</Language>"
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" /></ItemGroup><ItemGroup>"
        "<LibraryReference Include=\"../producer/ReferenceCountProducer.lib\" />"
        "</ItemGroup></Project>");
    writeFile(consumer / "Main.ns", R"NS(
using NativeSharp.Core.Threading;
public class Program
{
    public static int Main()
    {
        ReferenceCounts counts = new ReferenceCounts(1);
        if (!counts.TryRetainStrong() || counts.StrongCount() != 2) { return 1; }
        if (counts.ReleaseStrong() != ReferenceRelease.Retained) { return 2; }
        return counts.ReleaseStrong() == ReferenceRelease.Final ? 0 : 3;
    }
}
)NS");
    const std::string consumerProject = (consumer / "ReferenceCountConsumer.nsproj").string();
    const std::string consumerExe = (consumer / "ReferenceCountConsumer.exe").string();
    const std::array<std::string_view, 5> consumerArgs = {
        "--emit-project-oop-exe", consumerProject, "-O0", "-o", consumerExe};
    output.clear();
    error.clear();
    const bool consumerLinked = producerLinked &&
        runDriver(consumerArgs, output, error) == ExitCode::Success;
    tests.expect(consumerLinked,
        "LibraryReference reconstructs ReferenceCounts across the DLL boundary (" + error + ")");
    if (consumerLinked)
    {
        std::filesystem::copy_file(
            producer / "ReferenceCountProducer.dll",
            consumer / "ReferenceCountProducer.dll",
            std::filesystem::copy_options::overwrite_existing,
            ignored);
        tests.expect(std::system(("\"" + consumerExe + "\"").c_str()) == 0,
            "the precompiled ReferenceCounts implementation executes through its DLL import");
    }

    return tests.resultKeeping(root);
}
