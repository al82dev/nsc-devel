#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Language/GenericSemantics.h"

#include "../support/TestContext.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::ExitCode;
using nativesharp::driver::Driver;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view properties,
    std::string_view source)
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(
        project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>Exe</OutputType>" +
        std::string(properties) +
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />"
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
}

ExitCode runDriver(
    const std::vector<std::string>& ownedArguments,
    std::string& output,
    std::string& error)
{
    std::vector<std::string_view> arguments;
    arguments.reserve(ownedArguments.size());
    for (const std::string& argument : ownedArguments) arguments.push_back(argument);
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}
}

int main()
{
    TestContext tests;
    using nativesharp::language::InstantiationBudget;

    std::string error;
    InstantiationBudget direct(8, 16);
    tests.expect(direct.enter("A<int>", error) && !direct.enter("A<int>", error) &&
            error == "recursive generic instantiation; causal chain: A<int> -> A<int>",
        "direct recursion has an exact deterministic causal chain");

    InstantiationBudget indirect(8, 16);
    tests.expect(indirect.enter("A<int>", error) && indirect.enter("B<int>", error) &&
            !indirect.enter("A<int>", error) &&
            error == "recursive generic instantiation; causal chain: "
                "A<int> -> B<int> -> A<int>",
        "indirect recursion reports the complete compact cycle");

    InstantiationBudget compact(13, 32);
    bool entered = true;
    for (int index = 0; index < 13; ++index)
        entered = compact.enter("F" + std::to_string(index), error) && entered;
    tests.expect(entered && !compact.enter("overflow", error) &&
            error.find("... -> F1 -> F2") != std::string::npos &&
            error.find("F0") == std::string::npos &&
            error.ends_with("F12 -> overflow"),
        "resource backtraces retain only the newest twelve causal frames");

    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t04_generic_resource_limits";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const auto cacheProject = writeProject(
        root / "cache", "GenericCache",
        "<GenericInstantiationLimit>1</GenericInstantiationLimit>", R"NS(
class Generic
{
    public static T Identity<T>(T value) { return value; }
}
class Program
{
    public static int Main()
    {
        int first = Generic.Identity<int>(40);
        int second = Generic.Identity<int>(1);
        return Generic.Identity<int>(first + second);
    }
}
)NS");
    std::string output;
    const ExitCode cacheResult = runDriver(
        {"--dump-project-generics", cacheProject.string()}, output, error);
    tests.expect(cacheResult == ExitCode::Success &&
            output.find("materialized=1") != std::string::npos &&
            output.find("cache-hits=2") != std::string::npos &&
            output.find("count-limit=1") != std::string::npos,
        "cache hits are reusable and do not consume the configured count budget (" +
            error + output + ")");

    const auto countProject = writeProject(
        root / "count", "GenericCount",
        "<GenericInstantiationLimit>1</GenericInstantiationLimit>", R"NS(
class Generic
{
    public static T Identity<T>(T value) { return value; }
}
class Program
{
    public static int Main()
    {
        int first = Generic.Identity<int>(40);
        string text = Generic.Identity<string>("ok");
        return first + 2;
    }
}
)NS");
    output.clear();
    error.clear();
    const ExitCode countResult = runDriver(
        {"--check-project-generics", countProject.string()}, output, error);
    tests.expect(countResult != ExitCode::Success &&
            (error + output).find("generic instantiation count limit (1) exceeded") !=
                std::string::npos,
        "project count budgets stop a family of distinct instantiations before cache insertion (" +
            error + output + ")");

    output.clear();
    error.clear();
    const ExitCode overrideResult = runDriver(
        {"--check-project-generics", countProject.string(),
         "--generic-instantiation-limit=2"}, output, error);
    tests.expect(overrideResult == ExitCode::Success,
        "an explicit CLI budget overrides the root project default (" + error + output + ")");

    const auto branchingProject = writeProject(
        root / "branching", "GenericBranching",
        "<GenericInstantiationLimit>8</GenericInstantiationLimit>", R"NS(
class Left<T> { }
class Right<T> { }
class Generic
{
    public static int Expand<T>()
    {
        int left = Expand<Left<T>>();
        return left + Expand<Right<T>>();
    }
}
class Program
{
    public static int Main() { return Generic.Expand<int>(); }
}
)NS");
    output.clear();
    error.clear();
    const ExitCode branchingResult = runDriver(
        {"--check-project-generics", branchingProject.string()}, output, error);
    tests.expect(branchingResult != ExitCode::Success &&
            (error + output).find("generic instantiation count limit (8) exceeded") !=
                std::string::npos,
        "branching generic expansion is bounded by count before exponential growth (" +
            error + output + ")");

    const auto depthProject = writeProject(
        root / "depth", "GenericDepth",
        "<GenericInstantiationDepth>4</GenericInstantiationDepth>", R"NS(
class Generic
{
    public static int Expand<T>() { return Expand<T[]>(); }
}
class Program
{
    public static int Main() { return Generic.Expand<int>(); }
}
)NS");
    output.clear();
    error.clear();
    const ExitCode depthResult = runDriver(
        {"--check-project-generics", depthProject.string()}, output, error);
    const std::string firstDiagnostic = error + output;
    output.clear();
    error.clear();
    const ExitCode repeatedDepthResult = runDriver(
        {"--check-project-generics", depthProject.string()}, output, error);
    const std::string secondDiagnostic = error + output;
    tests.expect(depthResult != ExitCode::Success && repeatedDepthResult == depthResult &&
            firstDiagnostic.find("generic instantiation depth limit (4) exceeded") !=
                std::string::npos &&
            firstDiagnostic.find("causal chain:") != std::string::npos &&
            firstDiagnostic == secondDiagnostic,
        "unbounded key growth stops at the configured depth with a reproducible diagnostic (" +
            firstDiagnostic + ")");

    tests.expect(runDriver(
            {"--check-project-generics", cacheProject.string(),
             "--generic-depth-limit=0"}, output, error) ==
            ExitCode::CommandLineError &&
            error.find("between 1 and 4096") != std::string::npos,
        "malformed CLI budgets fail before compilation with bounded guidance");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
