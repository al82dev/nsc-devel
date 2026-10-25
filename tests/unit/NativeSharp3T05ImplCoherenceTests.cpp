#include "nativesharp/Driver/Driver.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view outputType,
    std::string_view source,
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    const auto project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>" + std::string(outputType) +
        "</OutputType></PropertyGroup><ItemGroup>" + std::string(items) +
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
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

bool compileAndRun(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    int expected,
    std::string& diagnostic,
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    const auto project = writeProject(directory, name, "Exe", source, items);
    const std::string projectText = project.string();
    const std::string executableText = (directory / (std::string(name) + ".exe")).string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success)
    {
        std::cerr << diagnostic << '\n';
        return false;
    }
    const int observed = std::system(("\"" + executableText + "\"").c_str());
    if (observed == expected) return true;
    diagnostic += "process exit code " + std::to_string(observed) +
        ", expected " + std::to_string(expected);
    return false;
}

bool rejected(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view source,
    std::string_view expected,
    std::string& diagnostic,
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    const auto project = writeProject(directory, name, "Exe", source, items);
    const std::string projectText = project.string();
    const std::array<std::string_view, 2> arguments = {
        "--check-project-interfaces", projectText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success && diagnostic.find(expected) != std::string::npos)
        return true;
    std::cerr << diagnostic << '\n';
    return false;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t05_impl_coherence";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(compileAndRun(root / "productive", "ProductiveImpl", R"NS(
trait IScore { int Score(); int Bonus() { return Score() + 2; } }
class Number
{
    private int value;
    public Number(int source) { value = source; }
}
impl IScore for Number { int Score() { return value; } }
class Program
{
    public static int Main()
    {
        raii Number number = new Number(40);
        borrowed dyn IScore score = number;
        return score.Bonus();
    }
}
)NS", 42, diagnostic),
        "an out-of-line impl contributes public witnesses and dynamic dispatch in O0/NSLink (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "duplicate", "DuplicateImpl", R"NS(
trait IScore { int Score(); }
class Number { public int Score() { return 42; } }
impl IScore for Number { }
impl IScore for Number { }
class Program { public static int Main() { return 0; } }
)NS", "duplicate trait impl", diagnostic),
        "two explicit impls for the same pair are rejected deterministically (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "ordinary_interface", "OrdinaryInterface", R"NS(
interface IScore { int Score(); }
class Number { public int Score() { return 42; } }
impl IScore for Number { }
class Program { public static int Main() { return 0; } }
)NS", "requires a NativeSharp 3 trait", diagnostic),
        "impl cannot silently change an ordinary interface into a trait (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "generic", "GenericImpl", R"NS(
trait IScore<T> { int Score(); }
class Box<T> where T : copy { public int Value() { return 42; } }
impl<T> IScore<T> for Box<T> where T : copy { int Score() { return Value(); } }
class Program
{
    public static int Main()
    {
        raii Box<int> box = new Box<int>();
        borrowed dyn IScore<int> score = box;
        return score.Score();
    }
}
)NS", 42, diagnostic),
        "a conditional impl is cloned with its exactly matching generic owner (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "constraint_mismatch", "ConstraintMismatch", R"NS(
trait IScore<T> { int Score(); }
class Box<T> where T : copy { }
impl<T> IScore<T> for Box<T> where T : unmanaged { int Score() { return 42; } }
class Program { public static int Main() { return 0; } }
)NS", "must repeat the target class generic parameters and where constraints exactly", diagnostic),
        "conditional impl constraints cannot extend the owner inconsistently (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "base_and_impl", "BaseAndImpl", R"NS(
trait IScore { int Score(); }
class Number : IScore { public int Score() { return 42; } }
impl IScore for Number { }
class Program { public static int Main() { return 0; } }
)NS", "duplicate trait impl", diagnostic),
        "a base-list conformance and an impl block cannot claim the same pair twice (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "missing_witness", "MissingWitness", R"NS(
trait IScore { int Score(); }
class Number { }
impl IScore for Number { }
class Program { public static int Main() { return 0; } }
)NS", "does not implement interface method", diagnostic),
        "an impl block remains subject to the complete trait contract (" + diagnostic + ")");

    const std::filesystem::path producer = root / "modules" / "Producer";
    const auto producerProject = writeProject(producer, "CoherenceApi", "StaticLibrary", R"NS(
namespace CoherenceApi
{
    public trait IScore { int Score(); }
    trait IHidden { int Hidden(); }
    public class Foreign { public int Value() { return 7; } }
    public class Box<T> where T : copy { public int Value() { return 42; } }
    impl<T> IScore for Box<T> where T : copy { int Score() { return Value(); } }
}
)NS");
    const std::string producerProjectText = producerProject.string();
    const std::string libraryText = (producer / "CoherenceApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", libraryText};
    std::string output;
    std::string error;
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    diagnostic = error + output;
    const std::string_view consumerItems =
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/CoherenceApi.lib\" />";
    const bool downstreamImpl = producerResult == ExitCode::Success && compileAndRun(
        root / "modules" / "Downstream", "DownstreamImpl", R"NS(
using CoherenceApi;
class Local { }
impl IScore for Local { int Score() { return 42; } }
class Program
{
    public static int Main()
    {
        raii Local local = new Local();
        borrowed dyn IScore score = local;
        return score.Score();
    }
}
)NS", 42, diagnostic, consumerItems);
    tests.expect(downstreamImpl,
        "a downstream module may implement an imported trait for its own local class (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(producerResult == ExitCode::Success && rejected(
        root / "modules" / "Orphan", "OrphanImpl", R"NS(
using CoherenceApi;
trait ILocal { int Score(); }
impl ILocal for Foreign { int Score() { return 42; } }
class Program { public static int Main() { return 0; } }
)NS", "sealed trait coherence rejects this impl", diagnostic, consumerItems),
        "a downstream module cannot attach an impl to a foreign class (" + diagnostic + ")");

    diagnostic.clear();
    const bool genericRoundTrip = producerResult == ExitCode::Success && compileAndRun(
        root / "modules" / "GenericConsumer", "GenericImplConsumer", R"NS(
using CoherenceApi;
class Program
{
    public static int Main()
    {
        raii Box<int> box = new Box<int>();
        borrowed dyn IScore score = box;
        return score.Score();
    }
}
)NS", 42, diagnostic, consumerItems);
    tests.expect(genericRoundTrip,
        "NSMETA transports a generic impl source extension to a separate consumer (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(producerResult == ExitCode::Success && rejected(
        root / "modules" / "Privacy", "PrivateTraitImpl", R"NS(
using CoherenceApi;
class Local { }
impl IHidden for Local { int Hidden() { return 42; } }
class Program { public static int Main() { return 0; } }
)NS", "unknown type name 'IHidden'", diagnostic, consumerItems),
        "an internal producer trait cannot acquire a downstream witness through an impl block (" +
            diagnostic + ")");

    return tests.result();
}
