#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryAbiReport.h"
#include "nativesharp/Interop/LibraryApiDiff.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <algorithm>
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t05_opaque_results";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    constexpr std::string_view prelude = R"NS(
trait IScore { int Score(); }
class Number : IScore
{
    private int value;
    public Number(int source) { value = source; }
    public int Score() { return value; }
}
)NS";

    std::string diagnostic;
    tests.expect(compileAndRun(root / "basic", "OpaqueBasic", std::string(prelude) + R"NS(
class Factory
{
    public static some IScore Make(bool high)
    {
        if (high) return new Number(40);
        return new Number(42);
    }
}
class Program
{
    public static int Main()
    {
        var value = Factory.Make(false);
        return value.Score();
    }
}
)NS", 42, diagnostic),
        "some Trait returns one nominal stack value and forwards its trait surface (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "recursive", "OpaqueRecursive", std::string(prelude) + R"NS(
class Factory
{
    public static some IScore Make(int depth)
    {
        if (depth == 0) return new Number(42);
        return Make(depth - 1);
    }
}
class Program { public static int Main() { return Factory.Make(2).Score(); } }
)NS", 42, diagnostic),
        "self-recursion reuses the declaration's opaque identity instead of nesting wrappers (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "mismatch", "OpaqueMismatch", std::string(prelude) + R"NS(
class Other : IScore { public int Score() { return 1; } }
class Factory
{
    public static some IScore Make(bool choose)
    {
        if (choose) return new Number(42);
        return new Other();
    }
}
class Program { public static int Main() { return 0; } }
)NS", "different concrete types", diagnostic),
        "all non-recursive return paths must resolve to one concrete type (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "not_trait", "OpaqueNotTrait", R"NS(
class Number { }
class Factory { public static some Number Make() { return new Number(); } }
class Program { public static int Main() { return 0; } }
)NS", "is not a visible trait", diagnostic),
        "some requires a trait rather than an arbitrary nominal type (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "not_conforming", "OpaqueNotConforming", R"NS(
trait IScore { int Score(); }
class Number { public int Score() { return 42; } }
class Factory { public static some IScore Make() { return new Number(); } }
class Program { public static int Main() { return 0; } }
)NS", "does not implement trait", diagnostic),
        "matching member names do not replace nominal trait conformance (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "invalid_position", "OpaqueInvalidPosition", R"NS(
trait IScore { int Score(); }
class Program
{
    private some IScore value;
    public static int Main() { return 0; }
}
)NS", "permitted only as the direct return type", diagnostic),
        "some Trait is rejected outside a direct function or method return type (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "struct_base", "OpaqueStructBase", R"NS(
class Base { }
struct Invalid : Base { }
class Program { public static int Main() { return 0; } }
)NS", "a struct cannot inherit class", diagnostic),
        "the struct trait syntax cannot silently become class inheritance (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "inherited", "OpaqueInherited", R"NS(
trait IBase { int Score(); }
trait IDerived : IBase { }
class Number : IDerived { public int Score() { return 42; } }
class Factory { public static some IDerived Make() { return new Number(); } }
class Program { public static int Main() { return Factory.Make().Score(); } }
)NS", 42, diagnostic),
        "an opaque wrapper forwards inherited trait requirements in stable base-first order (" +
            diagnostic + ")");

    const std::filesystem::path producer = root / "modules" / "Producer";
    const auto producerProject = writeProject(producer, "OpaqueResultApi", R"NS(
namespace OpaqueResultApi
{
    public trait IScore { type Item; int Score(); }
    internal class HiddenScore { }
    impl IScore for HiddenScore
    {
        type Item = int;
        public int Score() { return 42; }
    }
    public class Factory
    {
        public static some IScore Make() { return new HiddenScore(); }
    }
}
)NS", "StaticLibrary");
    const std::string producerProjectText = producerProject.string();
    const std::string libraryText = (producer / "OpaqueResultApi.lib").string();
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
            producer / "OpaqueResultApi.nsmeta", metadata, metadataError);
    const auto wrapper = std::ranges::find_if(metadata.types, [](const auto& type)
    {
        return type.fullyQualifiedName.find("__nsc_some_") != std::string::npos;
    });
    tests.expect(metadataRead && metadata.formatMinor == 31U &&
            wrapper != metadata.types.end() &&
            wrapper->kind == nativesharp::interop::LibraryMetadataTypeKind::Struct &&
            (wrapper->flags & nativesharp::interop::kMetadataTypeOpaqueResult) != 0U &&
            !wrapper->interfaceTypeKeys.empty() && wrapper->fields.size() == 1U &&
            wrapper->associatedItems.size() == 1U &&
            wrapper->associatedItems.front().name == "Item" &&
            wrapper->associatedItems.front().hasDefault &&
            wrapper->size == wrapper->fields.front().size &&
            wrapper->alignment == wrapper->fields.front().alignment,
        "the current NSMETA publishes the 1.30 one-field opaque-result contract and one trait for the "
        "declaration-scoped opaque identity (" +
            metadataError + diagnostic + ")");

    bool rejectedDownlevel = false;
    if (metadataRead)
    {
        nativesharp::interop::LibraryMetadata downlevel = metadata;
        downlevel.formatMinor = 29U;
        std::string downlevelError;
        rejectedDownlevel = !nativesharp::interop::LibraryMetadataCodec{}.writeModel(
            downlevel, root / "modules" / "OpaqueResultApi-1.29.nsmeta", downlevelError) &&
            downlevelError.find("before 1.30") != std::string::npos;
        if (!rejectedDownlevel) diagnostic += downlevelError;
    }
    tests.expect(rejectedDownlevel,
        "a 1.29 writer cannot erase an opaque-result identity contract (" + diagnostic + ")");

    std::ostringstream abiReport;
    if (metadataRead)
        nativesharp::interop::LibraryAbiReporter::write(metadata, abiReport);
    tests.expect(
        abiReport.str().find("kind=opaque-result") != std::string::npos &&
            abiReport.str().find("associated-item #0 kind=type name=Item") !=
                std::string::npos,
        "the ABI report names the opaque identity and its associated contract");

    bool malformedRejected = false;
    bool flagRemovalBreaksCompatibility = false;
    if (metadataRead && wrapper != metadata.types.end())
    {
        nativesharp::interop::LibraryMetadata malformed = metadata;
        const std::size_t wrapperIndex = static_cast<std::size_t>(
            std::distance(metadata.types.begin(), wrapper));
        malformed.types[wrapperIndex].fields.clear();
        std::string malformedError;
        malformedRejected = !nativesharp::interop::LibraryMetadataCodec{}
            .verifyCurrentCompiler(malformed, malformedError) &&
            malformedError.find("one-field struct") != std::string::npos;

        nativesharp::interop::LibraryMetadata erased = metadata;
        erased.types[wrapperIndex].flags &= static_cast<std::uint16_t>(
            ~nativesharp::interop::kMetadataTypeOpaqueResult);
        const nativesharp::interop::LibraryApiDiffReport diff =
            nativesharp::interop::LibraryApiDiffer::compare(metadata, erased);
        flagRemovalBreaksCompatibility = !diff.sourceCompatible || !diff.abiCompatible;
    }
    tests.expect(malformedRejected,
        "NSMETA rejects an opaque identity whose one-field representation was corrupted");
    tests.expect(flagRemovalBreaksCompatibility,
        "API/ABI diff classifies erasing the opaque-result flag as a compatibility change");

    const std::string_view consumerItems =
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/OpaqueResultApi.lib\" />";
    diagnostic.clear();
    const bool consumerPassed = producerResult == ExitCode::Success && compileAndRun(
        root / "modules" / "Consumer", "OpaqueResultConsumer", R"NS(
using OpaqueResultApi;
class Program
{
    private static int Read<T>(T value) where T : IScore where T.Item == int
    {
        return value.Score();
    }
    public static int Main()
    {
        return Read(Factory.Make());
    }
}
)NS", 42, diagnostic, consumerItems);
    tests.expect(consumerPassed,
        "a LibraryReference consumer uses the opaque trait surface without naming the hidden "
        "concrete type (" + diagnostic + ")");

    return tests.resultKeeping(root);
}
