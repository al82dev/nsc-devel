#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using nativesharp::frontend::ProjectCompilation;
using nativesharp::language::EscapeClass;
using nativesharp::tests::TestContext;

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
    std::string_view extraItems = {})
{
    const std::filesystem::path project = directory / (std::string(name) + ".nsproj");
    writeFile(project,
        "<Project><PropertyGroup><Name>" + std::string(name) +
        "</Name><OutputType>" + std::string(outputType) +
        "</OutputType></PropertyGroup><ItemGroup>"
        "<Compile Include=\"**/*.ns\" />" + std::string(extraItems) +
        "</ItemGroup></Project>");
    writeFile(directory / "Main.ns", source);
    return project;
}

std::string diagnosticsText(const ProjectCompilation& compilation)
{
    std::string result;
    for (const auto& diagnostic : compilation.diagnostics().diagnostics())
    {
        if (!result.empty()) result += " | ";
        result += diagnostic.message();
    }
    return result;
}

std::vector<EscapeClass> summaryFor(
    const ProjectCompilation& compilation,
    std::string_view memberName)
{
    if (compilation.memberModel() == nullptr || compilation.methodBodyModel() == nullptr)
        return {};
    const auto members = compilation.memberModel()->members();
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal)
    {
        if (members[ordinal].name != memberName) continue;
        const auto summary = compilation.methodBodyModel()->parameterEscapeSummary(ordinal);
        return {summary.begin(), summary.end()};
    }
    return {};
}
}

int main()
{
    TestContext tests;
    const std::filesystem::path root =
        std::filesystem::path(NSC_BINARY_DIR) / "ns3_t06_escape_summaries";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);

    const std::string producerSource = R"NS(
public delegate int Reader();
namespace EscapeContracts
{
    public class EscapeApi
    {
        public static int* Stored;
        public static int Read(int* value) { return *value; }
        public static int* Identity(int* value) { return value; }
        public static void Retain(int* value) { Stored = value; }
        public static void Forward(int* value) { Retain(value); }
        public static Reader Capture(int* value)
        {
            return () => { unsafe { return *value; } };
        }
    }
    public void RetainFunction(int* value) { EscapeApi.Stored = value; }
}
)NS";
    ProjectCompilation producer;
    std::string error;
    const std::filesystem::path producerProject = writeProject(
        root / "producer", "EscapeProducer", producerSource, "StaticLibrary");
    const bool producerCompiled = producer.compileProjectOopMir(producerProject, error);
    tests.expect(
        producerCompiled && !producer.diagnostics().hasErrors() &&
            summaryFor(producer, "Read") == std::vector<EscapeClass>{EscapeClass::NoEscape} &&
            summaryFor(producer, "Identity") == std::vector<EscapeClass>{EscapeClass::Return} &&
            summaryFor(producer, "Retain") == std::vector<EscapeClass>{EscapeClass::Global} &&
            summaryFor(producer, "Forward") == std::vector<EscapeClass>{EscapeClass::Global} &&
            summaryFor(producer, "Capture") == std::vector<EscapeClass>{EscapeClass::Global},
        "body analysis derives noescape, return, global, transitive and closure summaries (" +
            error + diagnosticsText(producer) + ")");

    nativesharp::interop::LibraryMetadataCodec codec;
    std::string metadataError;
    const std::filesystem::path metadataPath = root / "EscapeProducer.nsmeta";
    const std::filesystem::path producerLibrary = root / "EscapeProducer.lib";
    const std::string producerProjectText = producerProject.string();
    const std::string producerLibraryText = producerLibrary.string();
    const std::array<std::string_view, 5> producerArguments = {
        "--emit-project-oop-lib", producerProjectText, "-O0", "-o", producerLibraryText};
    std::string driverOutput;
    std::string driverError;
    const bool metadataWritten = producerCompiled &&
        runDriver(producerArguments, driverOutput, driverError) == nativesharp::ExitCode::Success;
    nativesharp::interop::LibraryMetadata roundTrip;
    const bool metadataRead = metadataWritten &&
        codec.read(metadataPath, roundTrip, metadataError);
    const auto retain = std::ranges::find_if(roundTrip.members, [](const auto& member)
    {
        return member.name == "Retain";
    });
    tests.expect(
        metadataRead && roundTrip.formatMinor ==
            nativesharp::interop::kMetadataFormatMinor &&
            retain != roundTrip.members.end() &&
            retain->parameterEscapeSummary ==
                std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                    nativesharp::interop::LibraryMetadataEscapeClass::Global},
        "NSMETA round-trips the public escape contract (" + driverError + driverOutput +
            metadataError + ")");

    nativesharp::interop::LibraryMetadata changed = roundTrip;
    const auto changedRead = std::ranges::find_if(changed.members, [](const auto& member)
    {
        return member.name == "Read";
    });
    if (changedRead != changed.members.end())
        changedRead->parameterEscapeSummary = {
            nativesharp::interop::LibraryMetadataEscapeClass::Global};
    const std::filesystem::path changedPath = root / "EscapeProducer.changed.nsmeta";
    nativesharp::interop::LibraryMetadata changedRoundTrip;
    const bool changedWritten = metadataRead &&
        codec.writeModel(changed, changedPath, metadataError) &&
        codec.read(changedPath, changedRoundTrip, metadataError);
    tests.expect(changedWritten &&
            changedRoundTrip.abiFingerprint != roundTrip.abiFingerprint,
        "changing an escape contract invalidates the public metadata fingerprint (" +
            metadataError + ")");

    nativesharp::interop::LibraryMetadata downlevel = roundTrip;
    downlevel.formatMinor = 34;
    downlevel.trivialCleanupRootTypeKeys.clear();
    downlevel.sourceNamespaces.clear();
    for (auto& generic : downlevel.genericTemplates)
    {
        generic.candidateReceiverTypeKeys.clear();
        generic.sourceImports.clear();
    }
    for (auto& member : downlevel.members) member.parameterEscapeSummary.clear();
    for (auto& function : downlevel.namespaceFunctions)
        function.parameterEscapeSummary.clear();
    const std::filesystem::path downlevelPath = root / "EscapeProducer.1.34.nsmeta";
    nativesharp::interop::LibraryMetadata downlevelRoundTrip;
    const bool downlevelRead = codec.writeModel(downlevel, downlevelPath, metadataError) &&
        codec.read(downlevelPath, downlevelRoundTrip, metadataError);
    tests.expect(downlevelRead && std::ranges::all_of(
            downlevelRoundTrip.members,
            [](const auto& member) { return member.parameterEscapeSummary.empty(); }),
        "NSMETA 1.34 remains readable and exposes missing summaries for conservative hydration (" +
            metadataError + ")");

    nativesharp::interop::LibraryMetadata corrupt = roundTrip;
    if (auto member = std::ranges::find_if(corrupt.members, [](const auto& candidate)
        { return candidate.name == "Read"; }); member != corrupt.members.end())
    {
        member->parameterEscapeSummary.push_back(
            nativesharp::interop::LibraryMetadataEscapeClass::Unknown);
    }
    std::string corruptError;
    tests.expect(!codec.writeModel(corrupt, root / "corrupt.nsmeta", corruptError) &&
            corruptError.find("escape-summary parameter count") != std::string::npos,
        "a corrupt escape catalogue fails closed (" + corruptError + ")");

    constexpr std::string_view producerReference =
        "<LibraryReference Include=\"../EscapeProducer.lib\" />";

    // A present but unknown contract is not proof that a frame address is safe.
    nativesharp::interop::LibraryMetadata unknown = roundTrip;
    if (auto member = std::ranges::find_if(unknown.members, [](const auto& candidate)
        { return candidate.name == "Retain"; }); member != unknown.members.end())
        member->parameterEscapeSummary.assign(1, nativesharp::interop::LibraryMetadataEscapeClass::Unknown);
    if (auto function = std::ranges::find_if(unknown.namespaceFunctions, [](const auto& candidate)
        { return candidate.name == "RetainFunction"; }); function != unknown.namespaceFunctions.end())
        function->parameterEscapeSummary.assign(1, nativesharp::interop::LibraryMetadataEscapeClass::Unknown);
    const std::filesystem::path unknownPath = root / "EscapeProducer.unknown.nsmeta";
    const bool unknownWritten = codec.writeModel(unknown, unknownPath, metadataError);
    tests.expect(unknownWritten, "write explicit unknown escape contract (" + metadataError + ")");
    const std::filesystem::path unknownProject = writeProject(
        root / "unknown", "EscapeUnknown", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int local = 42; EscapeApi.Retain(&local); return local; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownProjectText = unknownProject.string();
    const std::array<std::string_view, 3> unknownArguments = {
        "--dump-project-method-bodies", unknownProjectText, "-O0"};
    std::string unknownOutput;
    std::string unknownError;
    const nativesharp::ExitCode unknownResult = runDriver(unknownArguments, unknownOutput, unknownError);
    tests.expect(unknownWritten && unknownResult != nativesharp::ExitCode::Success &&
            (unknownError + unknownOutput).find("escape contract") != std::string::npos,
        "an unknown native escape contract rejects current-frame storage (" + unknownError + unknownOutput + ")");
    const std::filesystem::path unknownAliasProject = writeProject(
        root / "unknown-alias", "EscapeUnknownAlias", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int local = 42; int* alias = &local; EscapeApi.Retain(alias); return local; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownAliasProjectText = unknownAliasProject.string();
    const std::array<std::string_view, 3> unknownAliasArguments = {
        "--dump-project-method-bodies", unknownAliasProjectText, "-O0"};
    std::string unknownAliasOutput;
    std::string unknownAliasError;
    const nativesharp::ExitCode unknownAliasResult = runDriver(unknownAliasArguments, unknownAliasOutput, unknownAliasError);
    tests.expect(unknownWritten && unknownAliasResult != nativesharp::ExitCode::Success &&
            (unknownAliasError + unknownAliasOutput).find("escape contract") != std::string::npos,
        "an unknown native escape contract rejects an alias to frame storage (" +
            unknownAliasError + unknownAliasOutput + ")");
    const std::filesystem::path unknownFunctionProject = writeProject(
        root / "unknown-function", "EscapeUnknownFunction", R"NS(
using EscapeContracts;
public class Program { public static int Main() { int local = 42; RetainFunction(&local); return local; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownFunctionProjectText = unknownFunctionProject.string();
    const std::array<std::string_view, 3> unknownFunctionArguments = {
        "--dump-project-method-bodies", unknownFunctionProjectText, "-O0"};
    std::string unknownFunctionOutput;
    std::string unknownFunctionError;
    const nativesharp::ExitCode unknownFunctionResult =
        runDriver(unknownFunctionArguments, unknownFunctionOutput, unknownFunctionError);
    tests.expect(unknownWritten && unknownFunctionResult != nativesharp::ExitCode::Success &&
            (unknownFunctionError + unknownFunctionOutput).find("escape contract") != std::string::npos,
        "an unknown native function escape contract rejects current-frame storage (" +
            unknownFunctionError + unknownFunctionOutput + ")");
    const std::filesystem::path unknownForwardProject = writeProject(
        root / "unknown-forward", "EscapeUnknownForward", R"NS(
using EscapeContracts;
public class Program {
    static void Forward(int* value) { EscapeApi.Retain(value); }
    public static int Main() { int local = 42; Forward(&local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::filesystem::path forwardLibraryProject = writeProject(
        root / "forward-library", "ForwardLibrary", R"NS(
using EscapeContracts;
public class Forwarder { public static void Forward(int* value) { EscapeApi.Retain(value); } }
)NS", "StaticLibrary", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string forwardLibraryProjectText = forwardLibraryProject.string();
    const std::string forwardLibraryPath = (root / "ForwardLibrary.lib").string();
    const std::array<std::string_view, 5> forwardLibraryArguments = {
        "--emit-project-oop-lib", forwardLibraryProjectText, "-O0", "-o", forwardLibraryPath};
    std::string forwardLibraryOutput;
    std::string forwardLibraryError;
    nativesharp::interop::LibraryMetadata forwardLibraryMetadata;
    const bool forwardLibraryRead = runDriver(forwardLibraryArguments, forwardLibraryOutput, forwardLibraryError) ==
        nativesharp::ExitCode::Success && codec.read(root / "ForwardLibrary.nsmeta", forwardLibraryMetadata, metadataError);
    const auto forwardMember = std::ranges::find_if(forwardLibraryMetadata.members, [](const auto& candidate)
        { return candidate.name == "Forward"; });
    tests.expect(forwardLibraryRead && forwardMember != forwardLibraryMetadata.members.end() &&
            forwardMember->parameterEscapeSummary == std::vector<nativesharp::interop::LibraryMetadataEscapeClass>{
                nativesharp::interop::LibraryMetadataEscapeClass::Unknown},
        "forwarding preserves the unknown escape summary (" + forwardLibraryError + forwardLibraryOutput + metadataError + ")");
    const std::string unknownForwardProjectText = unknownForwardProject.string();
    const std::array<std::string_view, 3> unknownForwardArguments = {
        "--dump-project-method-bodies", unknownForwardProjectText, "-O0"};
    std::string unknownForwardOutput;
    std::string unknownForwardError;
    const nativesharp::ExitCode unknownForwardResult =
        runDriver(unknownForwardArguments, unknownForwardOutput, unknownForwardError);
    tests.expect(unknownWritten && unknownForwardResult != nativesharp::ExitCode::Success &&
            (unknownForwardError + unknownForwardOutput).find("escape contract") != std::string::npos,
        "a forwarded unknown native contract rejects current-frame storage (" +
            unknownForwardError + unknownForwardOutput + ")");
    const std::filesystem::path unknownForwardAliasProject = writeProject(
        root / "unknown-forward-alias", "EscapeUnknownForwardAlias", R"NS(
using EscapeContracts;
public class Program {
    static void Forward(int* value) { int* alias = value; EscapeApi.Retain(alias); }
    public static int Main() { int local = 42; Forward(&local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardAliasProjectText = unknownForwardAliasProject.string();
    const std::array<std::string_view, 3> unknownForwardAliasArguments = {
        "--dump-project-method-bodies", unknownForwardAliasProjectText, "-O0"};
    std::string unknownForwardAliasOutput;
    std::string unknownForwardAliasError;
    const nativesharp::ExitCode unknownForwardAliasResult =
        runDriver(unknownForwardAliasArguments, unknownForwardAliasOutput, unknownForwardAliasError);
    tests.expect(unknownWritten && unknownForwardAliasResult != nativesharp::ExitCode::Success &&
            (unknownForwardAliasError + unknownForwardAliasOutput).find("escape contract") != std::string::npos,
        "a local pointer alias preserves a forwarded unknown native contract (" +
            unknownForwardAliasError + unknownForwardAliasOutput + ")");
    const std::filesystem::path unknownForwardAliasGlobalProject = writeProject(
        root / "unknown-forward-alias-global", "EscapeUnknownForwardAliasGlobal", R"NS(
using EscapeContracts;
public class Program { public static int Data;
    static void Forward(int* value) { int* alias = value; EscapeApi.Retain(alias); }
    public static int Main() { Forward(&Data); return 42; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardAliasGlobalProjectText = unknownForwardAliasGlobalProject.string();
    const std::array<std::string_view, 3> unknownForwardAliasGlobalArguments = {
        "--dump-project-method-bodies", unknownForwardAliasGlobalProjectText, "-O0"};
    std::string unknownForwardAliasGlobalOutput;
    std::string unknownForwardAliasGlobalError;
    tests.expect(unknownWritten && runDriver(unknownForwardAliasGlobalArguments, unknownForwardAliasGlobalOutput,
            unknownForwardAliasGlobalError) == nativesharp::ExitCode::Success,
        "a forwarded pointer alias permits global storage (" +
            unknownForwardAliasGlobalError + unknownForwardAliasGlobalOutput + ")");
    const std::filesystem::path safeForwardAliasProject = writeProject(
        root / "safe-forward-alias", "EscapeSafeForwardAlias", R"NS(
using EscapeContracts;
public class Program {
    static int Forward(int* value) { int* alias = value; return EscapeApi.Read(alias); }
    public static int Main() { int local = 42; return Forward(&local); }
}
)NS", "Exe", producerReference);
    const std::string safeForwardAliasProjectText = safeForwardAliasProject.string();
    const std::array<std::string_view, 3> safeForwardAliasArguments = {
        "--dump-project-method-bodies", safeForwardAliasProjectText, "-O0"};
    std::string safeForwardAliasOutput;
    std::string safeForwardAliasError;
    tests.expect(runDriver(safeForwardAliasArguments, safeForwardAliasOutput, safeForwardAliasError) ==
            nativesharp::ExitCode::Success,
        "a forwarded pointer alias preserves a known nonescaping contract (" +
            safeForwardAliasError + safeForwardAliasOutput + ")");
    const std::filesystem::path unknownForwardFunctionProject = writeProject(
        root / "unknown-forward-function", "EscapeUnknownForwardFunction", R"NS(
using EscapeContracts;
public class Program {
    static void First(int* value) { RetainFunction(value); }
    static void Second(int* value) { First(value); }
    public static int Main() { int local = 42; Second(&local); return local; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardFunctionProjectText = unknownForwardFunctionProject.string();
    const std::array<std::string_view, 3> unknownForwardFunctionArguments = {
        "--dump-project-method-bodies", unknownForwardFunctionProjectText, "-O0"};
    std::string unknownForwardFunctionOutput;
    std::string unknownForwardFunctionError;
    const nativesharp::ExitCode unknownForwardFunctionResult =
        runDriver(unknownForwardFunctionArguments, unknownForwardFunctionOutput, unknownForwardFunctionError);
    tests.expect(unknownWritten && unknownForwardFunctionResult != nativesharp::ExitCode::Success &&
            (unknownForwardFunctionError + unknownForwardFunctionOutput).find("escape contract") != std::string::npos,
        "two wrappers preserve an unknown native function contract (" +
            unknownForwardFunctionError + unknownForwardFunctionOutput + ")");
    const std::filesystem::path unknownForwardGlobalProject = writeProject(
        root / "unknown-forward-global", "EscapeUnknownForwardGlobal", R"NS(
using EscapeContracts;
public class Program {
    public static int Data;
    static void Forward(int* value) { EscapeApi.Retain(value); }
    public static int Main() { Forward(&Data); return 42; }
}
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownForwardGlobalProjectText = unknownForwardGlobalProject.string();
    const std::array<std::string_view, 3> unknownForwardGlobalArguments = {
        "--dump-project-method-bodies", unknownForwardGlobalProjectText, "-O0"};
    std::string unknownForwardGlobalOutput;
    std::string unknownForwardGlobalError;
    tests.expect(unknownWritten && runDriver(unknownForwardGlobalArguments, unknownForwardGlobalOutput,
            unknownForwardGlobalError) == nativesharp::ExitCode::Success,
        "a forwarded unknown native contract permits global storage (" +
            unknownForwardGlobalError + unknownForwardGlobalOutput + ")");
    const std::filesystem::path unknownGlobalProject = writeProject(
        root / "unknown-global", "EscapeUnknownGlobal", R"NS(
using EscapeContracts;
public class Program { public static int Data; public static int Main() { EscapeApi.Retain(&Data); return 42; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownGlobalProjectText = unknownGlobalProject.string();
    const std::array<std::string_view, 3> unknownGlobalArguments = {
        "--dump-project-method-bodies", unknownGlobalProjectText, "-O0"};
    std::string unknownGlobalOutput;
    std::string unknownGlobalError;
    tests.expect(unknownWritten && runDriver(unknownGlobalArguments, unknownGlobalOutput, unknownGlobalError) ==
            nativesharp::ExitCode::Success,
        "an unknown native escape contract permits global storage (" + unknownGlobalError + unknownGlobalOutput + ")");
    const std::filesystem::path unknownFunctionGlobalProject = writeProject(
        root / "unknown-function-global", "EscapeUnknownFunctionGlobal", R"NS(
using EscapeContracts;
public class Program { public static int Data; public static int Main() { RetainFunction(&Data); return 42; } }
)NS", "Exe", "<LibraryReference Include=\"../EscapeProducer.lib\" Metadata=\"../EscapeProducer.unknown.nsmeta\" />");
    const std::string unknownFunctionGlobalProjectText = unknownFunctionGlobalProject.string();
    const std::array<std::string_view, 3> unknownFunctionGlobalArguments = {
        "--dump-project-method-bodies", unknownFunctionGlobalProjectText, "-O0"};
    std::string unknownFunctionGlobalOutput;
    std::string unknownFunctionGlobalError;
    tests.expect(unknownWritten && runDriver(unknownFunctionGlobalArguments, unknownFunctionGlobalOutput,
            unknownFunctionGlobalError) == nativesharp::ExitCode::Success,
        "an unknown native function escape contract permits global storage (" +
            unknownFunctionGlobalError + unknownFunctionGlobalOutput + ")");

    const std::filesystem::path safeProject = writeProject(
        root / "safe", "EscapeSafe", R"NS(
using EscapeContracts;
public class Program
{
    public static int Main()
    {
        int local = 42;
        return EscapeApi.Read(&local);
    }
}
)NS", "Exe", producerReference);
    const std::string safeProjectText = safeProject.string();
    const std::array<std::string_view, 3> safeArguments = {
        "--dump-project-method-bodies", safeProjectText, "-O0"};
    std::string safeOutput;
    std::string safeError;
    tests.expect(runDriver(safeArguments, safeOutput, safeError) ==
            nativesharp::ExitCode::Success,
        "a proven noescape callee accepts current-frame storage (" + safeError +
            safeOutput + ")");

    const std::filesystem::path unsafeProject = writeProject(
        root / "unsafe", "EscapeUnsafe", R"NS(
using EscapeContracts;
public class Program
{
    public static int Main()
    {
        int local = 42;
        EscapeApi.Retain(&local);
        return local;
    }
}
)NS", "Exe", producerReference);
    const std::string unsafeProjectText = unsafeProject.string();
    const std::array<std::string_view, 3> unsafeArguments = {
        "--dump-project-method-bodies", unsafeProjectText, "-O0"};
    std::string unsafeOutput;
    std::string unsafeError;
    const nativesharp::ExitCode unsafeResult =
        runDriver(unsafeArguments, unsafeOutput, unsafeError);
    const std::string unsafeDiagnostics = unsafeError + unsafeOutput;
    tests.expect(unsafeResult != nativesharp::ExitCode::Success &&
            unsafeDiagnostics.find("retained by the callee's published escape contract") !=
                std::string::npos,
        "a proven global escape rejects current-frame storage with a causal diagnostic (" +
            unsafeDiagnostics + ")");

    const std::filesystem::path directGlobalProject = writeProject(
        root / "direct-global", "EscapeDirectGlobal", R"NS(
public class Program
{
    private static int* Stored;
    private static void Retain(int* value) { Stored = value; }
    public static int Main() { int local = 42; Retain(&local); return local; }
}
)NS");
    const std::string directGlobalProjectText = directGlobalProject.string();
    const std::array<std::string_view, 3> directGlobalArguments = {
        "--dump-project-method-bodies", directGlobalProjectText, "-O0"};
    std::string directGlobalOutput;
    std::string directGlobalError;
    const nativesharp::ExitCode directGlobalResult =
        runDriver(directGlobalArguments, directGlobalOutput, directGlobalError);
    const std::string directGlobalDiagnostics = directGlobalError + directGlobalOutput;
    tests.expect(directGlobalResult != nativesharp::ExitCode::Success &&
            directGlobalDiagnostics.find("retained by the callee's published escape contract") !=
                std::string::npos,
        "a direct global escape rejects current-frame storage without an edition gate (" +
            directGlobalDiagnostics + ")");

    // ES: Un local puntero se valida por a dónde apunta, no por su propio slot: una dirección del
    // marco que llega por un alias se rechaza, y un puntero a almacenamiento global copiado en
    // locales se acepta (antes se rechazaba: KrLinearAllocator de nsc-debugger no compilaba).
    // EN: A pointer local is validated by where it points, not by its own slot: a frame address that
    // arrives through an alias is rejected, and a pointer to global storage copied into locals is
    // accepted (it used to be rejected: nsc-debugger's KrLinearAllocator did not compile).
    const std::filesystem::path aliasFrameProject = writeProject(
        root / "pointer-alias-frame", "EscapeNegative", R"NS(
namespace EscapeNegative
{
    int* Kept = null;

    void Retain(int* pointer)
    {
        Kept = pointer;
    }

    int Main()
    {
        int local = 5;
        int* alias = null;
        alias = &local;
        Retain(alias);
        return 0;
    }
}
)NS");
    const std::string aliasFrameProjectText = aliasFrameProject.string();
    const std::array<std::string_view, 3> aliasFrameArguments = {
        "--dump-project-method-bodies", aliasFrameProjectText, "-O0"};
    std::string aliasFrameOutput;
    std::string aliasFrameError;
    const nativesharp::ExitCode aliasFrameResult =
        runDriver(aliasFrameArguments, aliasFrameOutput, aliasFrameError);
    const std::string aliasFrameDiagnostics = aliasFrameError + aliasFrameOutput;
    tests.expect(aliasFrameResult != nativesharp::ExitCode::Success &&
            aliasFrameDiagnostics.find("retained by the callee's published escape contract") !=
                std::string::npos,
        "a frame address passed through a pointer alias is rejected (" +
            aliasFrameDiagnostics + ")");

    const std::filesystem::path aliasGlobalProject = writeProject(
        root / "pointer-alias-global", "EscapePositive", R"NS(
namespace EscapePositive
{
    byte[16] Storage;
    byte* Kept = null;

    void Retain(byte* pointer)
    {
        Kept = pointer;
    }

    byte* Take()
    {
        return &Storage[0];
    }

    int Main()
    {
        byte* block = Take();
        byte* copy = block;
        Retain(copy);
        Retain(&Storage[1]);
        return Kept == &Storage[1] ? 0 : 1;
    }
}
)NS");
    const std::string aliasGlobalProjectText = aliasGlobalProject.string();
    const std::array<std::string_view, 3> aliasGlobalArguments = {
        "--dump-project-method-bodies", aliasGlobalProjectText, "-O0"};
    std::string aliasGlobalOutput;
    std::string aliasGlobalError;
    tests.expect(runDriver(aliasGlobalArguments, aliasGlobalOutput, aliasGlobalError) ==
            nativesharp::ExitCode::Success,
        "a pointer local that holds a global address may reach a retained parameter (" +
            aliasGlobalError + aliasGlobalOutput + ")");

    return tests.resultKeeping(root);
}
#include <span>
#include <sstream>
