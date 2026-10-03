#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryAbiReport.h"
#include "nativesharp/Interop/LibraryApiDiff.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

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
    std::string_view action,
    std::string_view expected,
    std::string& diagnostic)
{
    const auto project = writeProject(directory, name, source);
    const std::string projectText = project.string();
    const std::array<std::string_view, 2> arguments = {action, projectText};
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t05_trait_objects";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(compileAndRun(root / "views", "TraitObjectViews", R"NS(
trait IBase { int Score(); }
trait IDerived : IBase { int Bonus(); }
class Number : IDerived
{
    private int value;
    public Number(int source) { value = source; }
    public int Score() { return value; }
    public int Bonus() { return 2; }
}
class Program
{
    public static int Main()
    {
        raii Number number = new Number(40);
        borrowed dyn IDerived derived = number;
        borrowed dyn IBase baseView = derived;
        dyn IBase empty = null;
        if (empty != null) return 1;
        return baseView.Score() + derived.Bonus();
    }
}
)NS", 42, diagnostic),
        "dyn references dispatch, upcast to a base trait, and retain zero-null semantics (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "owned", "TraitObjectOwned", R"NS(
trait IScore { int Score(); }
class Resource : IScore
{
    private int value;
    private static int destructions;
    public Resource(int source) { value = source; }
    ~Resource() { destructions = destructions + 1; }
    public int Score() { return value; }
    public static int Destructions() { return destructions; }
}
class Program
{
    private static int Read(dyn IScore value) { return value.Score(); }
    private static void Scope()
    {
        owned Resource concrete = new Resource(42);
        owned dyn IScore erased = move(concrete);
        if (Read(erased) != 42) { return; }
    }
    public static int Main() { Scope(); return Resource.Destructions() == 1 ? 42 : 1; }
}
)NS", 42, diagnostic),
        "owned dyn transfers one existing object and descriptor-driven cleanup destroys it once (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "generic_method", "TraitObjectGenericMethod", R"NS(
trait IMap { int Map<T>(T value); }
class Program { public static int Main() { return 0; } }
)NS", "--check-project-interfaces", "generic trait methods are not object-safe", diagnostic),
        "generic trait methods cannot become dynamic slots (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "associated", "TraitObjectAssociated", R"NS(
trait IItems { type Item; int Count(); }
class Program
{
    public static int Main() { dyn IItems value = null; return value == null ? 0 : 1; }
}
)NS", "--check-project-interfaces", "fully defaulted associated items", diagnostic),
        "dyn rejects a trait whose associated identity remains implementation-dependent (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "modifier", "TraitObjectModifier", R"NS(
trait IScore { int Score(); }
class Program { private dyn IScore* invalid; public static int Main() { return 0; } }
)NS", "--check-project-interfaces", "cannot carry pointer or array modifiers", diagnostic),
        "dyn is already a reference carrier and cannot be wrapped in an accidental second pointer (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "no_box", "TraitObjectNoBox", R"NS(
trait IScore { int Score(); }
struct Value : IScore { public int Score() { return 42; } }
class Program
{
    public static int Main() { Value value; dyn IScore erased = value; return erased.Score(); }
}
)NS", "--check-project-method-bodies", "cannot initialize local 'erased'", diagnostic),
        "a value cannot become dyn by silent boxing (" + diagnostic + ")");

    const auto noAllocationProject = writeProject(
        root / "no_allocation_ir", "TraitObjectNoAllocation", R"NS(
trait IScore { int Score(); }
class Number : IScore { public int Score() { return 42; } }
class Views { public static dyn IScore Erase(Number value) { return value; } }
)NS", "StaticLibrary");
    const std::string noAllocationProjectText = noAllocationProject.string();
    const std::array<std::string_view, 3> mirArguments = {
        "--dump-project-oop-mir", noAllocationProjectText, "-O0"};
    std::string mir;
    std::string mirError;
    const bool mirDumped = runDriver(mirArguments, mir, mirError) == ExitCode::Success;
    tests.expect(mirDumped && mir.find("reference-interface") != std::string::npos &&
            mir.find("box") == std::string::npos,
        "concrete-to-dyn lowering is a reference view and introduces no boxing node (" +
            mirError + ")");

    const std::filesystem::path producer = root / "dll" / "Producer";
    const auto producerProject = writeProject(producer, "DynApi", R"NS(
namespace DynApi
{
    public trait IBase { int Score(); }
    public trait IDerived : IBase { int Bonus(); }
    public delegate dyn IBase Project(dyn IDerived value);
    public dyn IBase Current;
    public class Number : IDerived
    {
        private int value;
        public Number(int source) { value = source; }
        public int Score() { return value; }
        public int Bonus() { return 2; }
    }
    public class Api
    {
        public static dyn IDerived View(Number value) { return value; }
        public static int Read(dyn IBase value) { return value.Score(); }
    }
}
)NS", "SharedLibrary");
    const std::string producerProjectText = producerProject.string();
    const std::string dllText = (producer / "DynApi.dll").string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-dll", producerProjectText, "-O0", "-o", dllText};
    std::string output;
    std::string error;
    const ExitCode producerResult = runDriver(producerArguments, output, error);
    diagnostic = error + output;

    nativesharp::interop::LibraryMetadata metadata;
    std::string metadataError;
    const bool metadataRead = producerResult == ExitCode::Success &&
        nativesharp::interop::LibraryMetadataCodec{}.read(
            producer / "DynApi.nsmeta", metadata, metadataError);
    const auto trait = std::ranges::find_if(metadata.types, [](const auto& type)
    {
        return type.fullyQualifiedName == "DynApi.IDerived";
    });
    const auto view = std::ranges::find_if(metadata.members, [](const auto& member)
    {
        return member.name == "View";
    });
    const auto read = std::ranges::find_if(metadata.members, [](const auto& member)
    {
        return member.name == "Read";
    });
    const auto project = std::ranges::find_if(metadata.types, [](const auto& type)
    {
        return type.fullyQualifiedName == "DynApi.Project";
    });
    const auto current = std::ranges::find_if(
        metadata.namespaceStates, [](const auto& state)
        {
            return state.namespaceName == "DynApi" && state.name == "Current";
        });
    tests.expect(metadataRead && metadata.formatMinor == nativesharp::interop::kMetadataFormatMinor &&
            trait != metadata.types.end() && trait->traitObjectWordCount == 1U &&
            (trait->traitObjectFlags & nativesharp::interop::kMetadataTraitObjectObjectSafe) != 0U &&
            (trait->traitObjectFlags & nativesharp::interop::kMetadataTraitObjectDescriptorMap) != 0U &&
            view != metadata.members.end() &&
            (view->flags & nativesharp::interop::kMetadataMemberDynamicTrait) != 0U &&
            read != metadata.members.end() && read->parameters.size() == 1U &&
            read->parameters.front().isDynamicTrait &&
            project != metadata.types.end() && project->delegateReturnDynamicTrait &&
            project->delegateParameterDynamicTraits == std::vector<std::uint8_t>{1U} &&
            current != metadata.namespaceStates.end() && (current->flags & 2U) != 0U,
        "NSMETA 1.31 publishes one-word descriptor dispatch and retains dyn in members, delegates, and namespace state (" +
            metadataError + diagnostic + ")");

    std::ostringstream abiReport;
    if (metadataRead) nativesharp::interop::LibraryAbiReporter::write(metadata, abiReport);
    tests.expect(
        abiReport.str().find("trait-object representation=descriptor-reference words=1") !=
            std::string::npos &&
        abiReport.str().find("return=dyn:type:") != std::string::npos &&
        abiReport.str().find("dyn=yes") != std::string::npos,
        "ABI report exposes representation and every dynamic signature edge");

    bool downlevelRejected = false;
    bool dynRemovalIsSourceOnly = false;
    if (metadataRead && trait != metadata.types.end() && view != metadata.members.end())
    {
        nativesharp::interop::LibraryMetadata downlevel = metadata;
        downlevel.formatMinor = 30U;
        std::string downlevelError;
        downlevelRejected = !nativesharp::interop::LibraryMetadataCodec{}.writeModel(
            downlevel, root / "dll" / "DynApi-1.30.nsmeta", downlevelError) &&
            downlevelError.find("before 1.31") != std::string::npos;

        nativesharp::interop::LibraryMetadata erased = metadata;
        const std::size_t viewIndex = static_cast<std::size_t>(
            std::distance(metadata.members.begin(), view));
        erased.members[viewIndex].flags &= ~nativesharp::interop::kMetadataMemberDynamicTrait;
        const nativesharp::interop::LibraryApiDiffReport diff =
            nativesharp::interop::LibraryApiDiffer::compare(metadata, erased);
        dynRemovalIsSourceOnly = !diff.sourceCompatible && diff.abiCompatible;
    }
    tests.expect(downlevelRejected,
        "a 1.30 writer cannot silently erase the dyn representation/signature contract");
    tests.expect(dynRemovalIsSourceOnly,
        "removing explicit dyn is a source-contract change but keeps the one-word physical ABI");

    const std::string_view consumerItems =
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/DynApi.lib\" "
        "Runtime=\"../Producer/DynApi.dll\" />";
    diagnostic.clear();
    const bool consumerPassed = producerResult == ExitCode::Success && compileAndRun(
        root / "dll" / "Consumer", "DynConsumer", R"NS(
using DynApi;
class Program
{
    public static int Main()
    {
        raii Number number = new Number(40);
        dyn IDerived derived = Api.View(number);
        dyn IBase baseView = derived;
        return Api.Read(baseView) + derived.Bonus();
    }
}
)NS", 42, diagnostic, consumerItems);
    tests.expect(consumerPassed,
        "dyn return, parameter, dispatch, and safe upcast cross the native DLL boundary (" +
            diagnostic + ")");

    return tests.resultKeeping(root);
}
