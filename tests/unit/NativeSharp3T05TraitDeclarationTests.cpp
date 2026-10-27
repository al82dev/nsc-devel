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
    std::string_view source,
    std::string_view outputType = "Exe",
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(
        project,
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
    const std::filesystem::path project = writeProject(directory, name, source, "Exe", items);
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
    std::string& diagnostic)
{
    const std::filesystem::path project = writeProject(directory, name, source);
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t05_trait_declarations";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(compileAndRun(root / "default", "TraitDefault", R"NS(
public trait IScore
{
    int Score();
    int Bonus() { return Score() + 2; }
}
public class Number : IScore
{
    private int value;
    public Number(int source) { value = source; }
    public int Score() { return value; }
}
public class Program
{
    public static int Main()
    {
        raii Number number = new Number(40);
        borrowed dyn IScore score = number;
        return score.Bonus();
    }
}
)NS", 42, diagnostic),
        "required and default trait methods share deterministic dynamic dispatch (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "static_requirements", "StaticRequirements", R"NS(
trait IAdd<T>
{
    public static int Zero { get; }
    public static T operator +(T left, T right);
}
class Number : IAdd<Number>
{
    private int value;
    public Number(int source) { value = source; }
    public int Value { get { return value; } }
    public static int Zero { get { return 0; } }
    public static owned Number operator +(Number left, Number right)
    {
        return new Number(left.Value + right.Value);
    }
}
class Program
{
    public static int Main()
    {
        raii Number left = new Number(20);
        raii Number right = new Number(22);
        raii Number sum = left + right;
        return sum.Value + Number.Zero;
    }
}
)NS", 42, diagnostic),
        "a generic trait's static property and operator requirements accept a complete conformer (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "missing_method", "MissingMethod", R"NS(
trait IScore { int Score(); }
class Missing : IScore { }
class Program { public static int Main() { return 0; } }
)NS", "does not implement interface method", diagnostic),
        "a concrete conformer must implement every required instance method (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "missing_property", "MissingProperty", R"NS(
trait IOrigin { public static int Zero { get; } }
class Missing : IOrigin { }
class Program { public static int Main() { return 0; } }
)NS", "does not implement static trait requirement", diagnostic),
        "a concrete conformer must implement static property accessors (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "missing_operator", "MissingOperator", R"NS(
trait IAdd<T> { public static T operator +(T left, T right); }
class Number : IAdd<Number> { }
class Program { public static int Main() { return 0; } }
)NS", "does not implement static trait requirement", diagnostic),
        "a concrete conformer must implement declared operator requirements (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "default_conflict", "DefaultConflict", R"NS(
trait ILeft { int Value() { return 1; } }
trait IRight { int Value() { return 2; } }
trait ICombined : ILeft, IRight { }
class Item : ICombined { }
class Program { public static int Main() { return 0; } }
)NS", "competing default implementations", diagnostic),
        "multiple inherited defaults require an explicit trait redeclaration (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "resolved_conflict", "ResolvedDefault", R"NS(
trait ILeft { int Value() { return 1; } }
trait IRight { int Value() { return 2; } }
trait ICombined : ILeft, IRight { int Value() { return 42; } }
class Item : ICombined { }
class Program
{
    public static int Main()
    {
        raii Item item = new Item();
        borrowed dyn ICombined value = item;
        return value.Value();
    }
}
)NS", 42, diagnostic),
        "a derived trait can resolve an inherited default conflict deterministically (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "dominant_default", "DominantDefault", R"NS(
trait IBase { int Value() { return 1; } }
trait IDerived : IBase { int Value() { return 42; } }
trait ICombined : IBase, IDerived { }
class Item : ICombined { }
class Program
{
    public static int Main()
    {
        raii Item item = new Item();
        borrowed dyn ICombined value = item;
        return value.Value();
    }
}
)NS", 42, diagnostic),
        "the most-derived inherited default dominates its base without a false conflict (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "default_accessors", "DefaultAccessors", R"NS(
trait ITraitDefaults
{
    int Value { get { return 20; } }
    int this[int index] { get { return index + 1; } }
}
class TraitItem : ITraitDefaults { }
interface IInterfaceDefaults
{
    int Value { get { return 20; } }
    int this[int index] { get { return index + 1; } }
}
class InterfaceItem : IInterfaceDefaults { }
class Program
{
    public static int Main()
    {
        raii TraitItem traitItem = new TraitItem();
        borrowed dyn ITraitDefaults traitView = traitItem;
        raii InterfaceItem interfaceItem = new InterfaceItem();
        borrowed IInterfaceDefaults interfaceView = interfaceItem;
        return traitView.Value + traitView[0] + interfaceView.Value + interfaceView[0];
    }
}
)NS", 42, diagnostic),
        "trait and interface properties/indexers accept productive default accessor bodies (" +
            diagnostic + ")");

    const std::filesystem::path producer = root / "modules" / "Producer";
    const std::filesystem::path producerProject = writeProject(
        producer, "TraitApi", R"NS(
namespace TraitApi
{
    public trait IScore
    {
        int Score();
        int Bonus() { return Score() + 2; }
        public static int Zero { get; }
    }
}
)NS", "StaticLibrary");
    const std::string producerProjectText = producerProject.string();
    const std::string libraryText = (producer / "TraitApi.lib").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", libraryText};
    std::string output;
    std::string error;
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    diagnostic = error + output;
    const std::filesystem::path metadataPath = producer / "TraitApi.nsmeta";
    nativesharp::interop::LibraryMetadata metadata;
    nativesharp::interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const bool metadataRead = producerResult == ExitCode::Success &&
        codec.read(metadataPath, metadata, metadataError);
    const auto trait = std::ranges::find_if(
        metadata.types,
        [](const nativesharp::interop::LibraryMetadataType& type)
        {
            return type.fullyQualifiedName == "TraitApi.IScore";
        });
    tests.expect(
        metadataRead &&
            metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
            trait != metadata.types.end() &&
            (trait->flags & nativesharp::interop::kMetadataTypeTrait) != 0U &&
            trait->traitMethodSignatures.size() == 2U &&
            trait->traitMethodSignatures[0] == "Score()" &&
            trait->traitMethodSignatures[1] == "Bonus()",
        "NSMETA 1.28 preserves trait identity and normative method-slot order (" +
            metadataError + ")");

    std::ostringstream abiReport;
    if (metadataRead)
        nativesharp::interop::LibraryAbiReporter::write(metadata, abiReport);
    tests.expect(
        abiReport.str().find("kind=trait") != std::string::npos &&
            abiReport.str().find("trait-slot #0 signature=Score()") != std::string::npos &&
            abiReport.str().find("trait-slot #1 signature=Bonus()") != std::string::npos,
        "the ABI report exposes trait identity and ordered dispatch slots");

    nativesharp::interop::LibraryMetadata reordered = metadata;
    const auto reorderedTrait = std::ranges::find_if(
        reordered.types,
        [](const nativesharp::interop::LibraryMetadataType& type)
        {
            return type.fullyQualifiedName == "TraitApi.IScore";
        });
    if (reorderedTrait != reordered.types.end())
        std::ranges::reverse(reorderedTrait->traitMethodSignatures);
    const nativesharp::interop::LibraryApiDiffReport reorderedDiff =
        nativesharp::interop::LibraryApiDiffer::compare(metadata, reordered);
    tests.expect(
        reorderedTrait != reordered.types.end() &&
            (!reorderedDiff.sourceCompatible || !reorderedDiff.abiCompatible),
        "API diff classifies trait slot reordering as a compatibility change");

    nativesharp::interop::LibraryMetadata downlevel = metadata;
    downlevel.formatMinor = 27;
    std::string downlevelError;
    tests.expect(
        !codec.writeModel(
            downlevel, root / "trait-cannot-be-1.27.nsmeta", downlevelError) &&
            downlevelError.find("before 1.28") != std::string::npos,
        "a trait contract cannot be mislabeled as NSMETA 1.27 (" +
            downlevelError + ")");

    const std::string_view consumerItems =
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/TraitApi.lib\" />";
    const bool moduleRoundTrip = producerResult == ExitCode::Success && compileAndRun(
        root / "modules" / "Consumer", "TraitConsumer", R"NS(
using TraitApi;
class Number : IScore
{
    private int value;
    public Number(int source) { value = source; }
    public int Score() { return value; }
    public static int Zero { get { return 0; } }
}
class Program
{
    public static int Main()
    {
        raii Number number = new Number(40);
        borrowed dyn IScore score = number;
        return score.Bonus() + Number.Zero;
    }
}
)NS", 42, diagnostic, consumerItems);
    tests.expect(moduleRoundTrip,
        "NSMETA 1.28 preserves a public trait default and static requirement in a separate build (" +
            diagnostic + ")");

    std::filesystem::remove_all(root, cleanupError);
    return tests.result();
}
