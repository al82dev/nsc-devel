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
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "item373_job_dependencies";
    std::error_code filesystemError;
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::string threadingProject =
        (std::filesystem::path(NSC_SOURCE_DIR) /
            "lib/NativeSharp.Core.Threading/NativeSharp.Core.Threading.nsproj").generic_string();
    writeFile(root / "JobDependenciesItem373.nsproj",
        "<Project><PropertyGroup><Name>JobDependenciesItem373</Name></PropertyGroup><ItemGroup>"
        "<Compile Include=\"Main.ns\"/><ProjectReference Include=\"" + threadingProject +
        "\"/></ItemGroup></Project>");
    writeFile(root / "Main.ns", R"NS(
using NativeSharp.Core.Threading;
class State
{
    public static AtomicInt32 Marker = new AtomicInt32(0);
    public static AtomicInt32 CancelledInvocations = new AtomicInt32(0);
    public static Event Gate = new Event(true, false);
}
class DependencyFunction : JobFunction<int>
{
    public override int Invoke() { State.Marker.Store(1); return 21; }
}
class DependentFunction : JobFunction<long>
{
    public override long Invoke()
    {
        if (State.Marker.Load() != 1) { return -1L; }
        return 42L;
    }
}
class BlockingFunction : JobFunction<int>
{
    public override int Invoke() { State.Gate.Wait(-1); return 1; }
}
class MustNotRunFunction : JobFunction<int>
{
    public override int Invoke() { State.CancelledInvocations.Increment(); return 99; }
}
public class Program
{
    public static int Main()
    {
        raii ThreadPool pool = new ThreadPool(1, 8);
        raii DependencyFunction firstFunction = new DependencyFunction();
        raii DependentFunction secondFunction = new DependentFunction();
        raii Job<int> first = new Job<int>(firstFunction);
        raii Job<long> second = new Job<long>(secondFunction, 2);
        if (!second.AddDependency(first)) { return 1; }
        if (!first.Schedule(pool) || !second.Schedule(pool)) { return 2; }
        if (second.Result() != 42L || first.Result() != 21) { return 3; }

        raii BlockingFunction blockingFunction = new BlockingFunction();
        raii MustNotRunFunction cancelledFunction = new MustNotRunFunction();
        raii Job<int> blocker = new Job<int>(blockingFunction);
        raii Job<int> cancelled = new Job<int>(cancelledFunction);
        if (!blocker.Schedule(pool) || !cancelled.Schedule(pool)) { return 4; }
        if (!cancelled.Cancel()) { return 5; }
        State.Gate.Set();
        if (!cancelled.Wait(-1) || !cancelled.IsCancelled() || cancelled.IsCompleted()) { return 6; }
        pool.WaitIdle();
        if (State.CancelledInvocations.Load() != 0) { return 7; }

        raii MustNotRunFunction propagatedFunction = new MustNotRunFunction();
        raii Job<int> cancelledDependency = new Job<int>(cancelledFunction);
        raii Job<int> propagated = new Job<int>(propagatedFunction);
        if (!propagated.AddDependency(cancelledDependency)) { return 8; }
        if (!cancelledDependency.Cancel()) { return 9; }
        if (!propagated.Schedule(pool)) { return 10; }
        if (!propagated.Wait(-1) || !propagated.IsCancelled()) { return 11; }
        pool.Shutdown();
        return 42;
    }
}
)NS");

    std::string diagnostic;
    bool succeeded = compileAndRun(root / "JobDependenciesItem373.nsproj",
        root / "JobDependenciesO0.exe", "-O0", diagnostic);
    tests.expect(succeeded,
        "dependencies and cancellation execute correctly at O0 (" + diagnostic + ")");
    diagnostic.clear();
    succeeded = compileAndRun(root / "JobDependenciesItem373.nsproj",
        root / "JobDependenciesO2.exe", "-O2", diagnostic);
    tests.expect(succeeded,
        "dependencies and cancellation execute correctly at O2 (" + diagnostic + ")");
    const int result = tests.result();
    if (result == 0) std::filesystem::remove_all(root, filesystemError);
    return result;
}
