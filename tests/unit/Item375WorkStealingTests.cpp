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
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item375_work_stealing";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string threadingProject =
        (std::filesystem::path(NSC_SOURCE_DIR) /
            "lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj").generic_string();
    writeFile(root / "WorkStealingItem375.nsproj",
        "<Project><PropertyGroup><Name>WorkStealingItem375</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"Main.ns\"/><ProjectReference Include=\"" + threadingProject +
        "\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Threading;
class State
{
    public static Event BlockerStarted = new Event(true, false);
    public static Event ReleaseBlocker = new Event(true, false);
    public static Event StolenFinished = new Event(true, false);
    public static AtomicInt32 Completed = new AtomicInt32(0);
    public static void Blocker() { BlockerStarted.Set(); ReleaseBlocker.Wait(-1); }
    public static void Work()
    {
        if (Completed.Increment() == 4) { StolenFinished.Set(); }
    }
}
public class Program
{
    public static int Main()
    {
        raii ThreadPool pool = new ThreadPool(2, 8);
        if (!pool.WorkStealingEnabled()) { return 1; }
        ThreadStart blocker = State.Blocker;
        ThreadStart work = State.Work;
        if (!pool.QueueOnWorker(0, blocker)) { return 2; }
        if (!State.BlockerStarted.Wait(-1)) { return 3; }
        int i = 0;
        while (i < 4)
        {
            if (!pool.QueueOnWorker(0, work)) { return 4; }
            i = i + 1;
        }
        if (!State.StolenFinished.Wait(5000)) { return 5; }
        if (State.Completed.Load() != 4 || pool.StealCount() < 4) { return 6; }
        State.ReleaseBlocker.Set();
        pool.WaitIdle();
        pool.Shutdown();
        return 42;
    }
}
)NS");

    std::string diagnostic;
    bool succeeded = compileAndRun(root / "WorkStealingItem375.nsproj",
        root / "WorkStealingO0.exe", "-O0", diagnostic);
    tests.expect(succeeded,
        "an idle worker steals a blocked worker's backlog at O0 (" + diagnostic + ")");
    diagnostic.clear();
    succeeded = compileAndRun(root / "WorkStealingItem375.nsproj",
        root / "WorkStealingO2.exe", "-O2", diagnostic);
    tests.expect(succeeded,
        "an idle worker steals a blocked worker's backlog at O2 (" + diagnostic + ")");
    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
