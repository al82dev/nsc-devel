#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace
{
using nativesharp::driver::Driver;
using nativesharp::ExitCode;
using nativesharp::tests::TestContext;

void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

void writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    std::string_view properties = {},
    std::string_view items = {})
{
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language>" + std::string(properties) +
        "</PropertyGroup><ItemGroup><Compile Include=\"**/*.ns\" />" +
        std::string(items) + "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
}

ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}

bool rejected(
    const std::filesystem::path& directory,
    std::string_view source,
    std::string_view expected,
    std::string& diagnostic)
{
    writeProject(directory, "RejectedMatch", source);
    const std::string project = (directory / "RejectedMatch.nsproj").string();
    const std::array<std::string_view, 3> arguments = {
        "--check-project-oop-hir", project, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    return result != ExitCode::Success && diagnostic.find(expected) != std::string::npos;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t03_exhaustive_match";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::filesystem::path direct = root / "direct";
    writeProject(direct, "ExhaustiveDirect", R"NS(
variant Result { Ok(int), Error(int) }
class Program
{
    public static int Main()
    {
        Result result = Result.Ok(42);
        match (result)
        {
            Ok(var value) => { return value; },
            Error(var code) => { return code + 1; }
        }
    }
}
)NS");
    const std::string directProject = (direct / "ExhaustiveDirect.nsproj").string();
    const std::string directExecutable = (direct / "ExhaustiveDirect.exe").string();
    const std::array<std::string_view, 5> directArguments = {
        "--emit-project-oop-exe", directProject, "-O0", "-o", directExecutable};
    std::string output;
    std::string error;
    const ExitCode directResult = runDriver(directArguments, output, error);
    tests.expect(
        directResult == ExitCode::Success,
        "closed variant match compiles without a redundant wildcard (" + error + output + ")");
    if (directResult == ExitCode::Success)
        tests.expect(std::system(("\"" + directExecutable + "\"").c_str()) == 42,
            "closed exhaustive match executes the selected payload arm");

    std::string diagnostic;
    tests.expect(
        rejected(root / "missing", R"NS(
variant State { Idle, Ready, Failed }
class Program { public static int Main() {
    match (State.Idle()) { Idle => { return 0; }, Ready => { return 1; } }
} }
)NS", "missing variant case: Failed", diagnostic),
        "missing closed-domain cases are named deterministically (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "redundant", R"NS(
variant State { Idle, Ready }
class Program { public static int Main() {
    match (State.Idle()) {
        Idle => { return 0; }, Ready => { return 1; }, _ => { return 2; }
    }
} }
)NS", "match arm #3 is unreachable", diagnostic),
        "wildcard after complete closed coverage is unreachable (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "partial", R"NS(
variant State { Value(int), Empty }
class Program { public static int Main() {
    match (State.Value(42)) { Value(42) => { return 1; }, Empty => { return 0; } }
} }
)NS", "missing variant case: Value", diagnostic),
        "a restrictive payload pattern does not claim the entire case (" + diagnostic + ")");
    diagnostic.clear();
    tests.expect(
        rejected(root / "open", R"NS(
class Program { public static int Main() {
    match (1) { 1 => { return 0; } }
} }
)NS", "open or unresolved domain requires a final wildcard", diagnostic),
        "open domains remain explicitly total (" + diagnostic + ")");

    const std::filesystem::path producer = root / "producer";
    writeProject(producer, "PublishedState", R"NS(
namespace Published { public variant State { Idle, Value(int) } }
)NS", "<OutputType>StaticLibrary</OutputType>");
    const std::string producerProject = (producer / "PublishedState.nsproj").string();
    const std::string producerLibrary = (producer / "PublishedState.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProject, "-O0", "-o", producerLibrary};
    output.clear();
    error.clear();
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    tests.expect(
        producerResult == ExitCode::Success,
        "variant producer for cross-module exhaustiveness compiles (" + error + output + ")");

    if (producerResult == ExitCode::Success)
    {
        const std::filesystem::path consumer = root / "consumer";
        const std::string reference =
            "<LibraryReference Include=\"" + producerLibrary + "\" Metadata=\"" +
            (producer / "PublishedState.nsmeta").string() + "\" />";
        writeProject(consumer, "PublishedConsumer", R"NS(
using Published;
class Program { public static int Main() {
    State state = State.Value(42);
    match (state) { Idle => { return 1; }, Value(var value) => { return value; } }
} }
)NS", {}, reference);
        const std::string consumerProject = (consumer / "PublishedConsumer.nsproj").string();
        const std::string consumerExecutable = (consumer / "PublishedConsumer.exe").string();
        const std::array<std::string_view, 5> consumerArguments = {
            "--emit-project-oop-exe", consumerProject, "-O0", "-o", consumerExecutable};
        output.clear();
        error.clear();
        const ExitCode consumerResult = runDriver(consumerArguments, output, error);
        tests.expect(
            consumerResult == ExitCode::Success,
            "NSMETA-rehydrated variant catalogue drives closed exhaustiveness (" +
                error + output + ")");
        if (consumerResult == ExitCode::Success)
            tests.expect(std::system(("\"" + consumerExecutable + "\"").c_str()) == 42,
                "cross-module exhaustive match executes through NSLink");
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
