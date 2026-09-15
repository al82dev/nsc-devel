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
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item372_typed_jobs";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string threadingProject =
        (std::filesystem::path(NSC_SOURCE_DIR) /
            "lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj").generic_string();
    writeFile(root / "TypedJobsItem372.nsproj",
        "<Project><PropertyGroup><Name>TypedJobsItem372</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"Main.ns\"/><ProjectReference Include=\"" + threadingProject +
        "\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Threading;
class IntFunction : JobFunction<int>
{
    public override int Invoke() { return 1234; }
}
class LongFunction : JobFunction<long>
{
    public override long Invoke() { return 9876543210L; }
}
public class Program
{
    public static int Main()
    {
        raii ThreadPool pool = new ThreadPool(2, 4);
        raii IntFunction intFunction = new IntFunction();
        raii LongFunction longFunction = new LongFunction();
        raii Job<int> intJob = new Job<int>(intFunction);
        raii Job<long> longJob = new Job<long>(longFunction);
        if (intJob.IsScheduled() || intJob.IsCompleted()) { return 1; }
        if (!intJob.Schedule(pool) || !longJob.Schedule(pool)) { return 2; }
        if (!intJob.Wait(-1) || !longJob.Wait(-1)) { return 3; }
        if (!intJob.IsScheduled() || !intJob.IsCompleted()) { return 4; }
        if (intJob.Schedule(pool)) { return 5; }
        if (intJob.Result() != 1234) { return 6; }
        if (longJob.Result() != 9876543210L) { return 7; }
        pool.Shutdown();
        return 42;
    }
}
)NS");

    std::string diagnostic;
    tests.expect(compileAndRun(
        root / "TypedJobsItem372.nsproj", root / "TypedJobsO0.exe", "-O0", diagnostic),
        "typed jobs return int and long results at O0 (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(compileAndRun(
        root / "TypedJobsItem372.nsproj", root / "TypedJobsO2.exe", "-O2", diagnostic),
        "typed jobs return int and long results at O2 (" + diagnostic + ")");
    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
