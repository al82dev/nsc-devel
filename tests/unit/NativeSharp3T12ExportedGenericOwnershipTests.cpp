#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryMetadataMerger.h"
#include "nativesharp/Interop/LibraryApiDiffer.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::filesystem::path writeProject(
    const std::filesystem::path& directory,
    std::string_view name,
    std::string_view output,
    std::string_view source,
    std::string_view items = "<Compile Include=\"**/*.ns\" />")
{
    writeFile(directory / (std::string(name) + ".nsproj"),
        "<Project><PropertyGroup><Name>" + std::string(name) + "</Name><OutputType>" +
        std::string(output) + "</OutputType><Language>3</Language></PropertyGroup><ItemGroup>" +
        std::string(items) + "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return directory / (std::string(name) + ".nsproj");
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode result = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return result;
}
}

int main()
{
    using nativesharp::ExitCode;
    using namespace nativesharp::interop;
    nativesharp::tests::TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t12_f07_generic_ownership";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    const std::string runtime =
        (std::filesystem::path(NSC_BINARY_DIR) / "generated/NativeSharp.Runtime.Win32.lib").string();
    _putenv_s("NSC_WINDOWS_RUNTIME_LIBRARY", runtime.c_str());

    const std::filesystem::path producer = root / "Producer";
    const std::filesystem::path producerProject = writeProject(
        producer, "GenericContracts", "Library", R"NS(
using Pick = GenericContracts.Pod;
using Number = int;
using Marker<T> = T;
import module GenericContracts;
import symbol GenericContracts.Pod as ImportedPod;
namespace GenericContracts
{
    using Local = GenericContracts.Pod;
    public copy struct Pod { public int Value; }
    public class Api
    {
        public static ref[shorter] int Narrow<lifetime longer, lifetime shorter, T>(
            ref[longer] int value, T witness)
            where T : unmanaged, copy, move, new(), notnull
            where longer : shorter
        {
            return ref value;
        }
        public static int Track<T>(T value)
            where T : class, lifetime, copy, move, new(), notnull
        {
            return 0;
        }
    }
}
)NS");
    const std::string producerProjectText = producerProject.string();
    const std::string producerLibraryText = (producer / "GenericContracts.lib").string();
    const std::array<std::string_view, 5> emitLibrary = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", producerLibraryText};
    std::string output;
    std::string error;
    const bool producerCompiled = runDriver(emitLibrary, output, error) == ExitCode::Success;

    LibraryMetadata metadata;
    LibraryMetadataCodec codec;
    std::string metadataError;
    const bool metadataRead = producerCompiled && codec.read(
        producer / "GenericContracts.nsmeta", metadata, metadataError);
    const auto narrow = std::ranges::find_if(metadata.genericTemplates, [](const auto& generic)
        { return generic.name == "Narrow"; });
    const auto track = std::ranges::find_if(metadata.genericTemplates, [](const auto& generic)
        { return generic.name == "Track"; });
    const std::uint16_t ownershipFlags = kMetadataGenericRequiresCopy |
        kMetadataGenericRequiresMove | kMetadataGenericRequiresUnmanaged |
        kMetadataGenericRequiresDefaultConstructor | kMetadataGenericRequiresNotNull;
    tests.expect(metadataRead && metadata.formatMinor == kMetadataFormatMinor &&
            narrow != metadata.genericTemplates.end() &&
            narrow->parameterContracts.size() == 1 &&
            (narrow->parameterContracts[0].flags & ownershipFlags) == ownershipFlags &&
            narrow->lifetimeParameters == std::vector<std::string>({"longer", "shorter"}) &&
            narrow->lifetimeOutlivesConstraints ==
                std::vector<std::pair<std::string, std::string>>({{"longer", "shorter"}}) &&
            track != metadata.genericTemplates.end() &&
            track->parameterContracts.size() == 1 &&
            (track->parameterContracts[0].flags & kMetadataGenericRequiresDrop) != 0U,
        "NSMETA 1.38 publishes move/copy/drop capabilities and erased lifetime constraints (" +
            error + output + metadataError + ")");
    tests.expect(metadataRead && narrow != metadata.genericTemplates.end() &&
            narrow->sourceImports.size() == 6 &&
            narrow->sourceImports[0].namespaceName.empty() &&
            narrow->sourceImports[0].alias == "Pick" &&
            narrow->sourceImports[0].target == "GenericContracts.Pod" &&
            narrow->sourceImports[1].namespaceName.empty() &&
            narrow->sourceImports[1].alias == "Number" && narrow->sourceImports[1].target == "int" &&
            narrow->sourceImports[2].namespaceName.empty() &&
            narrow->sourceImports[2].alias == "Marker < T >" && narrow->sourceImports[2].target == "T" &&
            narrow->sourceImports[3].namespaceName.empty() &&
            narrow->sourceImports[3].alias.empty() &&
            narrow->sourceImports[3].target == "GenericContracts" &&
            narrow->sourceImports[3].importKind == 1U && !narrow->sourceImports[3].reexport &&
            narrow->sourceImports[4].namespaceName.empty() &&
            narrow->sourceImports[4].alias == "ImportedPod" &&
            narrow->sourceImports[4].target == "GenericContracts.Pod" &&
            narrow->sourceImports[4].importKind == 2U && !narrow->sourceImports[4].reexport &&
            narrow->sourceImports[5].namespaceName == "GenericContracts" &&
            narrow->sourceImports[5].alias == "Local" &&
            narrow->sourceImports[5].target == "GenericContracts.Pod",
        "NSMETA 1.41 round-trips ordered using aliases and explicit imports in both scopes");

    LibraryMetadata downlevel = metadata;
    downlevel.formatMinor = 37;
    std::string downlevelError;
    tests.expect(metadataRead && !codec.writeModel(
            downlevel, root / "generic-contracts-1.37.nsmeta", downlevelError) &&
            downlevelError.find("before 1.38") != std::string::npos,
        "generic ownership contracts cannot be mislabeled as NSMETA 1.37 (" + downlevelError + ")");
    downlevel = metadata;
    downlevel.formatMinor = 40;
    downlevelError.clear();
    tests.expect(metadataRead && !codec.writeModel(
            downlevel, root / "generic-imports-1.40.nsmeta", downlevelError) &&
            downlevelError.find("before 1.41") != std::string::npos,
        "source imports cannot be silently discarded when writing NSMETA 1.40 (" + downlevelError + ")");
    for (auto& generic : downlevel.genericTemplates) generic.sourceImports.clear();
    downlevelError.clear();
    const auto legacyPath = root / "generic-contracts-1.40.nsmeta";
    const bool legacyWritten = codec.writeModel(downlevel, legacyPath, downlevelError);
    LibraryMetadata legacyDecoded;
    const bool legacyRead = legacyWritten && codec.read(legacyPath, legacyDecoded, downlevelError) &&
        codec.verifyCurrentCompiler(legacyDecoded, downlevelError);
    tests.expect(legacyRead && legacyDecoded.formatMinor == 40 &&
            legacyDecoded.genericTemplates.size() == metadata.genericTemplates.size() &&
            std::ranges::all_of(legacyDecoded.genericTemplates,
                [](const auto& generic) { return generic.sourceImports.empty(); }),
        "NSMETA 1.40 generic contracts remain readable without source imports (" + downlevelError + ")");
    LibraryMetadata invalidScope = metadata;
    const auto invalidTemplate = std::ranges::find_if(invalidScope.genericTemplates,
        [](const auto& generic) { return generic.name == "Narrow"; });
    if (invalidTemplate != invalidScope.genericTemplates.end() && !invalidTemplate->sourceImports.empty())
        invalidTemplate->sourceImports[0].namespaceName = "Unrelated";
    std::string invalidError;
    tests.expect(metadataRead && !codec.verifyCurrentCompiler(invalidScope, invalidError) &&
            invalidError.find("source import or lexical scope") != std::string::npos,
        "unrelated template import scopes are rejected (" + invalidError + ")");
    invalidScope = metadata;
    const auto invalidKindTemplate = std::ranges::find_if(invalidScope.genericTemplates,
        [](const auto& generic) { return generic.name == "Narrow"; });
    if (invalidKindTemplate != invalidScope.genericTemplates.end() && !invalidKindTemplate->sourceImports.empty())
        invalidKindTemplate->sourceImports[0].importKind = 6U;
    invalidError.clear();
    tests.expect(metadataRead && !codec.verifyCurrentCompiler(invalidScope, invalidError) &&
            invalidError.find("source import or lexical scope") != std::string::npos,
        "unknown template import kinds are rejected (" + invalidError + ")");
    invalidError.clear();
    tests.expect(metadataRead && !codec.writeModel(invalidScope, root / "invalid-import.nsmeta", invalidError) &&
            invalidError.find("source import or lexical scope") != std::string::npos,
        "writer rejects unknown template import kinds (" + invalidError + ")");
    LibraryMetadata changedImport = metadata;
    const auto changedTemplate = std::ranges::find_if(changedImport.genericTemplates,
        [](const auto& generic) { return generic.name == "Narrow"; });
    if (changedTemplate != changedImport.genericTemplates.end() && !changedTemplate->sourceImports.empty())
        changedTemplate->sourceImports[0].target = "GenericContracts.Other";
    tests.expect(metadataRead && LibraryApiDiffer::sourceFingerprint(metadata) !=
            LibraryApiDiffer::sourceFingerprint(changedImport) &&
            LibraryApiDiffer::abiSurfaceFingerprint(metadata) ==
            LibraryApiDiffer::abiSurfaceFingerprint(changedImport),
        "changing a reusable template import changes its source contract but not its ABI");
    const std::array<LibraryMetadata, 2> conflicting = {metadata, changedImport};
    LibraryMetadata combined;
    LibraryMetadataMerger merger;
    std::string mergeError;
    tests.expect(metadataRead && !merger.merge(conflicting, combined, mergeError),
        "conflicting template import scopes are not merged as identical (" + mergeError + ")");

    const std::string reference =
        "<Compile Include=\"**/*.ns\" />"
        "<LibraryReference Include=\"../Producer/GenericContracts.lib\" />";
    const std::filesystem::path consumer = writeProject(
        root / "Consumer", "Consumer", "Exe", R"NS(
using GenericContracts;
public class Program
{
    public static int Main()
    {
        Pod value;
        value.Value = 7;
        int number = 40;
        {
            ref int selected = ref Api.Narrow<Pod>(ref number, value);
            selected = selected + 1;
        }
        return number + 1;
    }
}
)NS", reference);
    const std::string consumerProjectText = consumer.string();
    const std::string consumerExeText = (consumer.parent_path() / "Consumer.exe").string();
    const std::array<std::string_view, 5> emitConsumer = {
        "--emit-project-oop-exe", consumerProjectText, "-O0", "-o", consumerExeText};
    output.clear(); error.clear();
    const bool consumerCompiled = runDriver(emitConsumer, output, error) == ExitCode::Success;
    tests.expect(consumerCompiled &&
            std::system(("\"" + consumerExeText + "\"").c_str()) == 42,
        "a source-free LibraryReference consumer rehydrates and executes the same contracts (" +
            error + output + ")");

    const std::filesystem::path rejected = writeProject(
        root / "Rejected", "Rejected", "Exe", R"NS(
using GenericContracts;
public class ReferenceValue { }
public class Program
{
    public static int Main()
    {
        int value = 0;
        ref int selected = ref Api.Narrow<ReferenceValue>(ref value, new ReferenceValue());
        return selected;
    }
}
)NS", reference);
    const std::string rejectedProjectText = rejected.string();
    const std::array<std::string_view, 3> rejectedCheck = {
        "--check-project-oop-hir", rejectedProjectText, "-O0"};
    output.clear(); error.clear();
    tests.expect(runDriver(rejectedCheck, output, error) != ExitCode::Success,
        "the rehydrated consumer rejects a type that violates exported ownership constraints (" +
            error + output + ")");
    return tests.resultKeeping(root);
}
