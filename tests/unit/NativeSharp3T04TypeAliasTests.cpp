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
    writeFile(
        directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><Language>3</Language><OutputType>" + std::string(outputType) +
        "</OutputType></PropertyGroup><ItemGroup>" + std::string(items) +
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
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
    std::cerr << diagnostic << '\n';
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
    const std::array<std::string_view, 3> arguments = {
        "--check-project-oop-hir", projectText, "-O0"};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    const bool accepted = result != ExitCode::Success &&
        diagnostic.find(expected) != std::string::npos;
    if (!accepted) std::cerr << diagnostic << '\n';
    return accepted;
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t04_type_aliases";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(compileAndRun(root / "productive", "TypeAliases", R"NS(
using Counter = int;
public opaque type Meters = int;
public opaque type Handle<T> = T* where T : unmanaged;
struct Box<T> { public T Value; }
class Boundary
{
    public static export extern C int ReadMeters(Meters value)
    {
        return value.ToRepresentation();
    }
}
class Program
{
    public static int Main()
    {
        Counter transparent = 1;
        Meters distance = Meters.FromRepresentation(41);
        Box<Meters> box = default(Box<Meters>);
        box.Value = distance;
        int value = Boundary.ReadMeters(box.Value);
        int number = 42;
        Handle<int> handle = Handle<int>.FromRepresentation(&number);
        if (sizeof(Meters) != sizeof(int) || sizeof(handle) != sizeof(int*) ||
            *handle.ToRepresentation() != 42)
        {
            return 1;
        }
        return value + transparent;
    }
}
)NS", 42, diagnostic),
        "transparent aliases erase while opaque aliases keep nominal generic identity, exact layout, "
        "explicit module boundaries and an explicit C ABI path in O0/NSLink (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "implicit", "ImplicitConversion", R"NS(
opaque type Meters = int;
class Program { public static int Main() { Meters value = 42; return 0; } }
)NS", "cannot initialize local 'value'", diagnostic),
        "opaque aliases reject representation-to-nominal implicit conversion (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "field", "PrivateRepresentation", R"NS(
opaque type Meters = int;
class Program
{
    public static int Main()
    {
        Meters value = Meters.FromRepresentation(42);
        return value.__nsc_opaque_value;
    }
}
)NS", "is not accessible", diagnostic),
        "the generated representation field is not source-accessible (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "cycle", "OpaqueCycle", R"NS(
opaque type First = Second;
opaque type Second = First;
class Program { public static int Main() { return 0; } }
)NS", "recursive by-value struct field", diagnostic),
        "opaque representation cycles terminate with the ordinary deterministic layout diagnostic (" +
        diagnostic + ")");

    const std::filesystem::path producer = root / "modules" / "Producer";
    const auto producerProject = writeProject(producer, "OpaqueApi", "StaticLibrary", R"NS(
namespace OpaqueApi
{
    public opaque type Token = int;
    public class Api
    {
        public static Token Create(int value) { return Token.FromRepresentation(value); }
        public static int Read(Token value) { return value.ToRepresentation(); }
    }
}
)NS");
    const std::string producerProjectText = producerProject.string();
    const std::string libraryText = (producer / "OpaqueApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", libraryText};
    std::string output;
    std::string error;
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    diagnostic = error + output;
    const std::string_view consumerItems =
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/OpaqueApi.lib\" />";
    const bool moduleRoundTrip = producerResult == ExitCode::Success && compileAndRun(
        root / "modules" / "Consumer", "OpaqueConsumer", R"NS(
using OpaqueApi;
class Program
{
    public static int Main()
    {
        Token value = Api.Create(42);
        return Api.Read(value);
    }
}
)NS", 42, diagnostic, consumerItems);
    tests.expect(moduleRoundTrip,
        "a public opaque value preserves identity and layout through NSMETA without exposing its "
        "representation boundary (" + diagnostic + ")");

    if (producerResult == ExitCode::Success)
    {
        diagnostic.clear();
        tests.expect(rejected(
            root / "modules" / "ForbiddenConsumer", "ForbiddenOpaqueConsumer", R"NS(
using OpaqueApi;
class Program
{
    public static int Main()
    {
        Token value = Token.FromRepresentation(42);
        return Api.Read(value);
    }
}
)NS", "no compatible member named 'FromRepresentation'", diagnostic, consumerItems),
            "module-internal representation conversion is absent from the consumer contract (" +
            diagnostic + ")");
    }
    else
    {
        tests.expect(false, "opaque producer library compiles (" + diagnostic + ")");
    }

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
