#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"

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
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << contents;
}

bool compileAndRun(const std::filesystem::path& project, const std::filesystem::path& executable,
    std::string_view optimization, std::string& diagnostic)
{
    const std::string projectText = project.string();
    const std::string executableText = executable.string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, optimization, "-o", executableText};
    std::ostringstream output;
    std::ostringstream error;
    if (Driver{}.run(arguments, output, error) != ExitCode::Success)
    {
        diagnostic = error.str() + output.str();
        return false;
    }
    const int exitCode = std::system(("\"" + executableText + "\"").c_str());
    diagnostic = "exit=" + std::to_string(exitCode);
    return exitCode == 42;
}
}

int main()
{
    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item374_per_worker_queues";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string threadingProject =
        (std::filesystem::path(NSC_SOURCE_DIR) /
            "lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj").generic_string();
    writeFile(root / "PerWorkerQueuesItem374.nsproj",
        "<Project><PropertyGroup><Name>PerWorkerQueuesItem374</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"Main.ns\"/><ProjectReference Include=\"" + threadingProject +
        "\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Threading;
class State
{
    public static Event Started = new Event(true, false);
    public static Event Release = new Event(true, false);
    public static AtomicInt32 SameQueueRan = new AtomicInt32(0);
    public static AtomicInt32 OtherQueueRan = new AtomicInt32(0);
    public static void Blocker() { Started.Set(); Release.Wait(-1); }
    public static void SameQueue() { SameQueueRan.Store(1); }
    public static void OtherQueue() { OtherQueueRan.Store(1); }
}
public class Program
{
    public static int Main()
    {
        raii ThreadPool pool = new ThreadPool(2, 4);
        if (pool.WorkerQueueCount() != 2 || pool.QueueCapacity() != 4) { return 1; }
        ThreadStart blocker = State.Blocker;
        ThreadStart same = State.SameQueue;
        ThreadStart other = State.OtherQueue;
        if (!pool.QueueOnWorker(0, blocker)) { return 2; }
        if (!State.Started.Wait(-1)) { return 3; }
        if (!pool.QueueOnWorker(0, same) || !pool.QueueOnWorker(1, other)) { return 4; }
        while (State.OtherQueueRan.Load() == 0) { }
        if (State.SameQueueRan.Load() != 0) { return 5; }
        State.Release.Set();
        pool.WaitIdle();
        if (State.SameQueueRan.Load() != 1 || State.OtherQueueRan.Load() != 1) { return 6; }
        pool.Shutdown();
        return 42;
    }
}
)NS");

    std::string diagnostic;
    tests.expect(compileAndRun(root / "PerWorkerQueuesItem374.nsproj",
                     root / "PerWorkerQueuesO0.exe", "-O0", diagnostic),
        "directed per-worker FIFO queues execute independently at O0 (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(compileAndRun(root / "PerWorkerQueuesItem374.nsproj",
                     root / "PerWorkerQueuesO2.exe", "-O2", diagnostic),
        "directed per-worker FIFO queues execute independently at O2 (" + diagnostic + ")");
    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
