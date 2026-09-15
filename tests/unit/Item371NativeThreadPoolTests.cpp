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

bool compileAndRun(
    const std::filesystem::path& project,
    const std::filesystem::path& executable,
    std::string_view optimization,
    std::string& diagnostic)
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
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item371_thread_pool";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string threadingProject =
        (std::filesystem::path(NSC_SOURCE_DIR) /
            "lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj").generic_string();
    writeFile(root / "ThreadPoolItem371.nsproj",
        "<Project><PropertyGroup><Name>ThreadPoolItem371</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"Main.ns\"/><ProjectReference Include=\"" + threadingProject +
        "\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Threading;
class PoolState
{
    public static AtomicInt32 Completed = new AtomicInt32(0);
    public static void Work() { Completed.Increment(); }
}
public class Program
{
    public static int Main()
    {
        raii ThreadPool pool = new ThreadPool(4, 64);
        ThreadStart work = PoolState.Work;
        int i = 0;
        while (i < 64)
        {
            if (!pool.Queue(work)) { return 1; }
            i = i + 1;
        }
        pool.WaitIdle();
        if (PoolState.Completed.Load() != 64) { return 2; }
        if (pool.WorkerCount() != 4 || pool.QueueCapacity() != 64) { return 3; }
        pool.Shutdown();
        return 42;
    }
}
)NS");

    std::string diagnostic;
    tests.expect(compileAndRun(
        root / "ThreadPoolItem371.nsproj", root / "ThreadPoolO0.exe", "-O0", diagnostic),
        "the native thread pool drains 64 jobs at O0 (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(compileAndRun(
        root / "ThreadPoolItem371.nsproj", root / "ThreadPoolO2.exe", "-O2", diagnostic),
        "the native thread pool drains 64 jobs at O2 (" + diagnostic + ")");
    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
