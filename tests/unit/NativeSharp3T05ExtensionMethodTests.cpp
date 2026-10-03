#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryApiDiff.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <array>
#include <algorithm>
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
    std::string& diagnostic,
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    const auto project = writeProject(directory, name, source, "Exe", items);
    const std::string projectText = project.string();
    const std::array<std::string_view, 2> arguments = {
        "--check-project-method-bodies", projectText};
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
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t05_extension_methods";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    std::string diagnostic;
    tests.expect(compileAndRun(root / "lookup", "ExtensionLookup", R"NS(
namespace Model
{
    public struct Number
    {
        public int Value;
        public int Score() { return 40; }
    }
}
namespace Extensions
{
    using Model;
    extension Number
    {
        public int Plus(int amount) { return this.Value + amount; }
        public int Score() { return 1; }
    }
}
namespace App
{
    using Model;
    using Extensions;
    public class Program
    {
        public static int Main()
        {
            Number value;
            value.Value = 40;
            return value.Score() + value.Plus(2) - 40;
        }
    }
}
)NS", 42, diagnostic),
        "using imports extensions while a real member wins over an extension (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "generic", "ExtensionClosedGeneric", R"NS(
namespace GenericExtensions
{
    public struct Box<T> { public T Value; }
    extension Box<int> { public int Read() { return this.Value; } }
    public class Program
    {
        public static int Main()
        {
            Box<int> box;
            box.Value = 42;
            return box.Read();
        }
    }
}
)NS", 42, diagnostic),
        "a closed generic receiver participates in extension lookup (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "generic_method", "ExtensionGenericMethod", R"NS(
namespace GenericExtensions
{
    extension int
    {
        public T Echo<T>(T value) { return value; }
    }
    public class Program
    {
        public static int Main()
        {
            int receiver = 0;
            return receiver.Echo<int>(42);
        }
    }
}
)NS", 42, diagnostic),
        "a generic extension method specializes through the ordinary namespace-function model (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "no_adl", "ExtensionNoAdl", R"NS(
namespace Model { public struct Number { public int Value; } }
namespace Hidden
{
    using Model;
    extension Number { public int Read() { return this.Value; } }
}
namespace App
{
    using Model;
    public class Program
    {
        public static int Main() { Number value; return value.Read(); }
    }
}
)NS", "has no compatible member named 'Read'", diagnostic),
        "the receiver type namespace does not trigger accidental extension lookup (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "ambiguous", "ExtensionAmbiguous", R"NS(
namespace Model { public struct Number { public int Value; } }
namespace First
{
    using Model;
    extension Number { public int Read() { return this.Value; } }
}
namespace Second
{
    using Model;
    extension Number { public int Read() { return this.Value + 1; } }
}
namespace App
{
    using Model;
    using First;
    using Second;
    public class Program
    {
        public static int Main() { Number value; return value.Read(); }
    }
}
)NS", "ambiguous namespace function call to 'Read'", diagnostic),
        "competing imported extensions produce an explicit ambiguity (" + diagnostic + ")");

    diagnostic.clear();
    tests.expect(compileAndRun(root / "visible_set", "ExtensionVisibleSet", R"NS(
namespace Imported
{
    extension int { public int Pick() { return this + 2; } }
}
namespace App
{
    using Imported;
    extension bool { public int Pick() { return 1; } }
    public class Program
    {
        public static int Main() { int value = 40; return value.Pick(); }
    }
}
)NS", 42, diagnostic),
        "an incompatible lexical extension does not hide a compatible imported overload (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(root / "ref_identity", "ExtensionRefIdentity", R"NS(
namespace App
{
    extension ref int { public int Read() { return this; } }
    public class Program
    {
        public static int Main() { short value = 42; return value.Read(); }
    }
}
)NS", "has no compatible member named 'Read'", diagnostic),
        "a ref extension receiver rejects numeric conversion and cannot reinterpret an address (" +
            diagnostic + ")");

    const std::filesystem::path producer = root / "cross_module" / "Producer";
    const auto producerProject = writeProject(producer, "ExtensionApi", R"NS(
namespace ExtensionApi
{
    public struct Number { public int Value; }
    extension ref Number
    {
        public int Add(int amount)
        {
            this.Value = this.Value + amount;
            return this.Value;
        }
        int Hidden() { return 99; }
    }
}
)NS", "StaticLibrary");
    const std::string producerProjectText = producerProject.string();
    const std::string libraryText = (producer / "ExtensionApi.lib").string();
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
            producer / "ExtensionApi.nsmeta", metadata, metadataError);
    const auto extension = std::ranges::find_if(
        metadata.namespaceFunctions, [](const auto& function)
        {
            return function.name == "Add";
        });
    tests.expect(metadataRead && metadata.formatMinor == 32U &&
            extension != metadata.namespaceFunctions.end() &&
            (extension->flags &
                nativesharp::interop::kMetadataNamespaceFunctionExtension) != 0U &&
            extension->parameters.size() == 2U &&
            extension->parameters.front().passKind ==
                nativesharp::interop::LibraryMetadataParameterPassKind::Ref,
        "NSMETA 1.32 preserves the extension identity and ref receiver contract (" +
            metadataError + diagnostic + ")");

    bool downlevelRejected = false;
    if (metadataRead)
    {
        nativesharp::interop::LibraryMetadata downlevel = metadata;
        downlevel.formatMinor = 31U;
        std::string downlevelError;
        downlevelRejected = !nativesharp::interop::LibraryMetadataCodec{}.writeModel(
            downlevel, producer / "ExtensionApi-1.31.nsmeta", downlevelError) &&
            downlevelError.find("before 1.32") != std::string::npos;
    }
    tests.expect(downlevelRejected,
        "a 1.31 writer cannot erase the extension lookup contract");

    bool extensionRemovalIsSourceOnly = false;
    if (metadataRead && extension != metadata.namespaceFunctions.end())
    {
        nativesharp::interop::LibraryMetadata erased = metadata;
        const std::size_t extensionIndex = static_cast<std::size_t>(
            std::distance(metadata.namespaceFunctions.begin(), extension));
        erased.namespaceFunctions[extensionIndex].flags &=
            static_cast<std::uint8_t>(
                ~nativesharp::interop::kMetadataNamespaceFunctionExtension);
        const nativesharp::interop::LibraryApiDiffReport diff =
            nativesharp::interop::LibraryApiDiffer::compare(metadata, erased);
        extensionRemovalIsSourceOnly = !diff.sourceCompatible && diff.abiCompatible;
    }
    tests.expect(extensionRemovalIsSourceOnly,
        "removing extension lookup changes source compatibility but not the static function ABI");

    const std::string_view consumerItems =
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/ExtensionApi.lib\" />";
    diagnostic.clear();
    const bool consumerPassed = producerResult == ExitCode::Success && compileAndRun(
        root / "cross_module" / "Consumer", "ExtensionConsumer", R"NS(
using ExtensionApi;
class Program
{
    public static int Main()
    {
        Number number;
        number.Value = 40;
        return number.Add(2);
    }
}
)NS", 42, diagnostic, consumerItems);
    tests.expect(consumerPassed,
        "an imported ref extension is materialized and invoked across a static-library boundary (" +
            diagnostic + ")");

    diagnostic.clear();
    tests.expect(rejected(
        root / "cross_module" / "HiddenConsumer", "ExtensionHiddenConsumer", R"NS(
using ExtensionApi;
class Program
{
    public static int Main()
    {
        Number number;
        return number.Hidden();
    }
}
)NS", "has no compatible member named 'Hidden'", diagnostic, consumerItems),
        "an internal extension is absent from NSMETA and remains inaccessible to a consumer (" +
            diagnostic + ")");

    return tests.resultKeeping(root);
}
