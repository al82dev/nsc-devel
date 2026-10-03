#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Language/GenericSemantics.h"

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
    std::string_view source,
    std::string_view outputType = "Exe",
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
    const auto project = writeProject(directory, name, source, "Exe", items);
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
    std::string& diagnostic)
{
    const auto project = writeProject(directory, name, source);
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t04_generic_packs";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(compileAndRun(root / "productive", "ProductivePacks", R"NS(
struct Pair<A, B> { }
struct Types<pack Ts>
{
    public int Count() { return packcount(Ts); }
}
struct Zipped<pack Ts>
{
    public Types<Pair<Ts, Ts>...> Values;
    public int Count() { return Values.Count(); }
}
struct Indexed<pack Ts>
{
    public packtype<Ts, 0> First;
    public packtype<Ts, 1> Second;
    public int Count() { return packcount(Ts); }
}
class Program
{
    private static int Values<pack const int Ns>()
    {
        return packsum(Ns) + packproduct(Ns) + packat(Ns, 1) + packcount(Ns);
    }
    private static int Forward<pack const int Ns>() { return Values<Ns...>(); }
    private static int Empty<pack const int Ns>() { return packsum(Ns) + packproduct(Ns); }
    private static bool Every<pack const bool Bs>() { return packall(Bs); }
    private static bool Any<pack const bool Bs>() { return packany(Bs); }
    private static int Pick<pack Ts>() { return 40 + packcount(Ts); }
    private static int Pick<T, pack Ts>(int value) { return value + packcount(Ts); }

    public static int Main()
    {
        Indexed<int, long> indexed;
        indexed.First = 20;
        indexed.Second = 22;
        if (indexed.First + (int)indexed.Second != 42 || indexed.Count() != 2) return 1;
        Zipped<int, long> zipped;
        if (zipped.Count() != 2) return 2;
        if (Forward<2, 3, 4>() != 39 || Empty<>() != 1) return 3;
        if (!Every<true, true>() || Every<true, false>()) return 4;
        if (Any<>() || !Any<false, true>()) return 5;
        if (Pick<int, long>() != 42 || Pick<int, long>(41) != 42) return 6;
        return 42;
    }
}
)NS", 42, diagnostic),
        "type/value packs support indexing, folds, zip, nesting, forwarding, empty packs and overloads (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "value-kind", "ValuePackKind", R"NS(
class Program
{
    private static int Values<pack const int Ns>() { return packsum(Ns); }
    public static int Main() { return Values<1, true>(); }
}
)NS", "const generic pack 'Ns' requires an integral value", diagnostic),
        "value packs validate every element against their declared const type (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "value-index", "ValuePackIndex", R"NS(
class Program
{
    private static int Value<pack const int Ns>() { return packat(Ns, 2); }
    public static int Main() { return Value<1, 2>(); }
}
)NS", "outside a pack of length 2", diagnostic),
        "value-pack indexing is bounds checked during materialization (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "type-index", "TypePackIndex", R"NS(
struct Bad<pack Ts> { public packtype<Ts, 3> Value; }
class Program { public static int Main() { Bad<int, long> value; return 0; } }
)NS", "outside a pack of length 2", diagnostic),
        "type-pack indexing is bounds checked during materialization (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "fold-kind", "PackFoldKind", R"NS(
class Program
{
    private static int Bad<pack Ts>() { return packsum(Ts); }
    public static int Main() { return Bad<int, long>(); }
}
)NS", "requires an integral const value pack", diagnostic),
        "folds reject a type pack with a causal diagnostic (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "non-final", "NonFinalPack", R"NS(
class Bad<pack Ts, U> { }
class Program { public static int Main() { return 0; } }
)NS", "must be the final generic parameter", diagnostic),
        "an unpartitionable non-final pack is rejected at its declaration (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "residual", "ResidualExpansion", R"NS(
class Box<T> { }
class Bad { public Box<Ts...> Value; }
class Program { public static int Main() { return 0; } }
)NS", "valid only for a declared type pack", diagnostic),
        "a residual expansion outside an active pack fails closed (" + diagnostic + ")");

    std::string resourceSource =
        "class Program { private static int Count<pack const int Ns>() { return packcount(Ns); } "
        "public static int Main() { return Count<";
    for (std::size_t index = 0; index < 1025U; ++index)
    {
        if (index != 0) resourceSource += ',';
        resourceSource += '1';
    }
    resourceSource += ">(); } }";
    diagnostic.clear();
    tests.expect(rejected(
            root / "resource", "PackResource", resourceSource,
            "generic pack expansion exceeds its resource budget", diagnostic),
        "productive value-pack materialization enforces the 1024-element budget (" +
            diagnostic + ")");

    std::string validationError;
    const std::array<std::size_t, 2> equalLengths = {3, 3};
    const std::array<std::size_t, 2> unequalLengths = {3, 2};
    tests.expect(
        nativesharp::language::validatePackExpansion(equalLengths, 8, validationError) &&
            !nativesharp::language::validatePackExpansion(
                unequalLengths, 8, validationError) &&
            validationError == "zipped generic packs have different lengths",
        "zipped pack expansion requires one deterministic common length");

    const std::filesystem::path producer = root / "modules" / "Producer";
    const auto producerProject = writeProject(
        producer, "PackApi", R"NS(
namespace PackApi
{
    public class Api
    {
        public static int Sum<pack const int Ns>() { return packsum(Ns); }
    }
}
)NS", "StaticLibrary");
    const std::string producerProjectText = producerProject.string();
    const std::string libraryText = (producer / "PackApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", libraryText};
    std::string output;
    std::string error;
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    diagnostic = error + output;
    const bool moduleRoundTrip = producerResult == ExitCode::Success && compileAndRun(
        root / "modules" / "Consumer", "PackConsumer", R"NS(
using PackApi;
class Program { public static int Main() { return Api.Sum<10, 20, 12>(); } }
)NS", 42, diagnostic,
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/PackApi.lib\" />");
    tests.expect(moduleRoundTrip,
        "type/value pack source contracts survive a precompiled NSMETA boundary (" +
            diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
