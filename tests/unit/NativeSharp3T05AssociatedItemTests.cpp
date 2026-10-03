#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <array>
#include <algorithm>
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
    const auto project = writeProject(directory, name, source, "Exe", items);
    const std::string projectText = project.string();
    const std::string executableText = (directory / (std::string(name) + ".exe")).string();
    const std::array<std::string_view, 5> arguments = {
        "--emit-project-oop-exe", projectText, "-O0", "-o", executableText};
    std::string output;
    std::string error;
    const ExitCode result = runDriver(arguments, output, error);
    diagnostic = error + output;
    if (result != ExitCode::Success) return false;
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
    std::string& diagnostic)
{
    const auto project = writeProject(directory, name, source);
    const std::string projectText = project.string();
    const std::array<std::string_view, 2> arguments = {
        "--check-project-interfaces", projectText};
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t05_associated_items";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(compileAndRun(root / "type", "IteratorProjection", R"NS(
trait IIterator { type Item; }
class Counter { }
impl IIterator for Counter { type Item = int; }
class Program
{
    private static T.Item Next<T>(T.Item value) where T : IIterator { return value; }
    public static int Main() { return Next<Counter>(42); }
}
)NS", 42, diagnostic),
        "an iterator-style associated type normalizes in generic signatures and bodies (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "constant", "AssociatedConstant", R"NS(
trait IValue { const int Value; }
class Number { }
impl IValue for Number { const int Value = 42; }
class Program
{
    private static int Read<T>() where T : IValue { return T.Value; }
    public static int Main() { return Read<Number>(); }
}
)NS", 42, diagnostic),
        "an associated constant is substituted as a compile-time expression (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "defaults", "AssociatedDefaults", R"NS(
trait IValue { type Item = int; const int Value = 42; }
class Number { }
impl IValue for Number { }
class Program
{
    private static T.Item Read<T>() where T : IValue { return T.Value; }
    public static int Main() { return Read<Number>(); }
}
)NS", 42, diagnostic),
        "trait defaults close omitted associated definitions deterministically (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "generic_default", "GenericAssociatedDefault", R"NS(
trait IValue<TValue> { type Item = TValue; }
class Number { }
impl IValue<int> for Number { }
class Program
{
    private static T.Item Read<T>(T.Item value) where T : IValue<int> { return value; }
    public static int Main() { return Read<Number>(42); }
}
)NS", 42, diagnostic),
        "a generic trait default is substituted before projection normalization (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "chain", "AssociatedChain", R"NS(
trait IInner { type Item; }
class Leaf { }
impl IInner for Leaf { type Item = int; }
trait IOuter { type Item; }
class Wrapper { }
impl IOuter for Wrapper { type Item = Leaf; }
class Program
{
    private static T.Item.Item Read<T>(T.Item.Item value) where T : IOuter { return value; }
    public static int Main() { return Read<Wrapper>(42); }
}
)NS", 42, diagnostic),
        "chained associated type projections normalize component by component (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "equality", "AssociatedEquality", R"NS(
trait IValue { type Item; }
class Number { }
impl IValue for Number { type Item = int; }
class Program
{
    private static int Read<T>() where T : IValue where T.Item == int { return 42; }
    public static int Main() { return Read<Number>(); }
}
)NS", 42, diagnostic),
        "an associated projection participates in equality constraints (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "missing", "AssociatedMissing", R"NS(
trait IValue { type Item; }
class Number { }
impl IValue for Number { }
class Program { public static int Main() { return 0; } }
)NS", "does not define required associated type", diagnostic),
        "a required associated type cannot be omitted (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "duplicate", "AssociatedDuplicate", R"NS(
trait IValue { type Item; }
class Number { }
impl IValue for Number { type Item = int; type Item = long; }
class Program { public static int Main() { return 0; } }
)NS", "duplicate associated item", diagnostic),
        "duplicate associated definitions are rejected (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "const_type", "AssociatedConstType", R"NS(
trait IValue { const int Value; }
class Number { }
impl IValue for Number { const long Value = 42; }
class Program { public static int Main() { return 0; } }
)NS", "requires 'int'", diagnostic),
        "an associated constant preserves its declared trait type (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "nonconstant", "AssociatedNonconstant", R"NS(
trait IValue { const int Value; }
class Number { public static int Make() { return 42; } }
impl IValue for Number { const int Value = Number.Make(); }
class Program { public static int Main() { return 0; } }
)NS", "must have a compile-time constant value", diagnostic),
        "an associated constant cannot contain a runtime expression (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "cycle", "AssociatedCycle", R"NS(
trait IValue { type A; type B; }
class Number { }
impl IValue for Number { type A = Number.B; type B = Number.A; }
class Program { public static int Main() { return 0; } }
)NS", "normalization cycle", diagnostic),
        "an associated type cycle is diagnosed before monomorphization (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "ambiguous_default", "AssociatedAmbiguousDefault", R"NS(
trait ILeft { type Item = int; }
trait IRight { type Item = long; }
class Number { }
impl ILeft for Number { }
impl IRight for Number { }
class Program { public static int Main() { return 0; } }
)NS", "competing inherited defaults", diagnostic),
        "competing associated defaults require an explicit definition (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "normalization", "AssociatedNormalizationDiagnostic", R"NS(
class Number { }
class Program
{
    private static T.Item Read<T>(T.Item value) { return value; }
    public static int Main() { return Read<Number>(42); }
}
)NS", "cannot normalize associated type projection", diagnostic),
        "a missing projection definition reports the projection and concrete owner (" +
            diagnostic + ")");

    const std::filesystem::path producer = root / "modules" / "Producer";
    const auto producerProject = writeProject(producer, "AssociatedApi", R"NS(
namespace AssociatedApi
{
    public trait IValue { type Item; const int Value; }
    public class Number { }
    impl IValue for Number { type Item = int; const int Value = 42; }
}
)NS", "StaticLibrary");
    const std::string producerProjectText = producerProject.string();
    const std::string libraryText = (producer / "AssociatedApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", libraryText};
    std::string output;
    std::string error;
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    diagnostic = error + output;
    nativesharp::interop::LibraryMetadata metadata;
    std::string metadataError;
    const bool metadataRead = producerResult == ExitCode::Success &&
        nativesharp::interop::LibraryMetadataCodec{}.read(
            producer / "AssociatedApi.nsmeta", metadata, metadataError);
    const auto traitRecord = std::ranges::find_if(metadata.types, [](const auto& type)
    {
        return type.fullyQualifiedName == "AssociatedApi.IValue";
    });
    const auto classRecord = std::ranges::find_if(metadata.types, [](const auto& type)
    {
        return type.fullyQualifiedName == "AssociatedApi.Number";
    });
    tests.expect(metadataRead &&
            metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
            traitRecord != metadata.types.end() && traitRecord->associatedItems.size() == 2U &&
            classRecord != metadata.types.end() && classRecord->associatedItems.size() == 2U,
        "current NSMETA publishes trait requirements and implementation definitions (" +
            metadataError + diagnostic + ")");

    bool rejectedDownlevel = false;
    if (metadataRead)
    {
        metadata.formatMinor = 28;
        std::string downlevelError;
        rejectedDownlevel = !nativesharp::interop::LibraryMetadataCodec{}.writeModel(
            metadata, root / "modules" / "AssociatedApi-1.28.nsmeta", downlevelError) &&
            downlevelError.find("before 1.29") != std::string::npos;
        if (!rejectedDownlevel) diagnostic += downlevelError;
    }
    tests.expect(rejectedDownlevel,
        "a 1.28 writer cannot silently erase associated-item contracts (" + diagnostic + ")");

    const std::string_view consumerItems =
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/AssociatedApi.lib\" />";
    const bool metadataRoundTrip = producerResult == ExitCode::Success && compileAndRun(
        root / "modules" / "Consumer", "AssociatedConsumer", R"NS(
using AssociatedApi;
class Program
{
    private static T.Item Read<T>() where T : IValue where T.Item == int
    {
        return T.Value;
    }
    public static int Main() { return Read<Number>(); }
}
)NS", 42, diagnostic, consumerItems);
    tests.expect(metadataRoundTrip,
        "current NSMETA preserves associated requirements and concrete definitions (" +
            diagnostic + ")");

    return tests.resultKeeping(root);
}
