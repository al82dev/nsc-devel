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

    return tests.resultKeeping(root);
}
#include <span>
#include <sstream>
