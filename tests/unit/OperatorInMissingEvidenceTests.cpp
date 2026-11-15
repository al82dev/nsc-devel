#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Interop/LibraryMetadata.h"

#include "../support/TestContext.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

// NSC-NEW-010: the archived legacy-escape fixture cannot be decoded by content (its records have an
// older shape and no label is used to choose a reading). This suite keeps the behavior it guarded:
// a consumer must not pass owning temporaries to a precompiled `in` operator whose metadata does not
// prove that the operands do not escape. The evidence is the content flag
// kMetadataOperatorInOperandsNoEscape; the test removes it from a current file instead of relying on
// an older compiler.
namespace
{
void writeFile(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

nativesharp::ExitCode runDriver(
    std::span<const std::string_view> arguments,
    std::string& output,
    std::string& error)
{
    nativesharp::driver::Driver driver;
    std::ostringstream out;
    std::ostringstream err;
    const nativesharp::ExitCode code = driver.run(arguments, out, err);
    output = out.str();
    error = err.str();
    return code;
}

constexpr std::uint32_t kUserOperator = 1U << 16U;

bool isUserOperator(const nativesharp::interop::LibraryMetadataMember& member)
{
    constexpr std::uint8_t kMethodKind = 2U;
    return member.kind == kMethodKind && (member.flags & kUserOperator) != 0U;
}

struct CheckResult
{
    bool accepted = false;
    std::string diagnostics;
};

CheckResult checkConsumer(
    const std::filesystem::path& root,
    std::string_view name,
    const std::filesystem::path& library,
    const std::filesystem::path& metadata,
    std::string_view body)
{
    const auto consumer = root / std::string(name);
    writeFile(consumer / "Consumer.nsproj",
        "<Project><PropertyGroup><Name>Consumer</Name></PropertyGroup>"
        "<ItemGroup><Compile Include=\"Main.ns\" /><LibraryReference Include=\"" + library.string() +
        "\" Metadata=\"" + metadata.string() + "\" /></ItemGroup></Project>");
    writeFile(consumer / "Main.ns", "int Main() { " + std::string(body) + " }");
    const std::string project = (consumer / "Consumer.nsproj").string();
    const std::array<std::string_view, 2> arguments = {"--check-project-method-bodies", project};
    std::string output;
    std::string error;
    CheckResult result;
    result.accepted = runDriver(arguments, output, error) == nativesharp::ExitCode::Success;
    result.diagnostics = output + error;
    return result;
}
}

int main()
{
    using nativesharp::ExitCode;
    using nativesharp::tests::TestContext;
    namespace interop = nativesharp::interop;

    TestContext tests;
    const auto root = std::filesystem::path(NSC_BINARY_DIR) / "operator_in_missing_evidence_tests";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root, ignored);

    // The operator only reads its operands, so the current compiler proves non-escape.
    const auto producer = root / "producer";
    writeFile(producer / "Producer.nsproj",
        "<Project><PropertyGroup><Name>Producer</Name><OutputType>StaticLibrary</OutputType></PropertyGroup>"
        "<ItemGroup><Compile Include=\"Types.ns\" /></ItemGroup></Project>");
    writeFile(producer / "Types.ns", R"NS(
public class Payload { public int Value; public Payload() { Value = 21; } }
public struct Direct {
 public owned Payload item;
 public Direct(int value) { item = new Payload(); }
 public static int operator +(in Direct a, in Direct b) { return a.item.Value + b.item.Value; }
}
)NS");
    const auto library = producer / "Producer.lib";
    const std::string producerProject = (producer / "Producer.nsproj").string();
    const std::string libraryText = library.string();
    const std::array<std::string_view, 5> producerArguments = {
        "-O0", "--emit-project-oop-lib", producerProject, "-o", libraryText};
    std::string output;
    std::string error;
    const bool produced = runDriver(producerArguments, output, error) == ExitCode::Success;
    tests.expect(produced, "the producer library is emitted (" + error + ")");

    interop::LibraryMetadataCodec codec;
    interop::LibraryMetadata proven;
    std::string metadataError;
    const auto provenPath = producer / "Producer.nsmeta";
    const bool provenRead = produced && codec.read(provenPath, proven, metadataError);
    std::size_t operators = 0;
    std::size_t provenOperators = 0;
    for (const auto& member : proven.members)
    {
        if (!isUserOperator(member)) continue;
        ++operators;
        if ((member.flags & interop::kMetadataOperatorInOperandsNoEscape) != 0U) ++provenOperators;
    }
    tests.expect(provenRead && operators == 1 && provenOperators == 1,
        "the operator publishes non-escape evidence for its in operands (" + metadataError + ")");

    // The same content without the evidence: nothing else changes, in particular no label.
    interop::LibraryMetadata unproven = proven;
    for (auto& member : unproven.members)
        if (isUserOperator(member)) member.flags &= ~interop::kMetadataOperatorInOperandsNoEscape;
    const auto unprovenPath = producer / "Unproven.nsmeta";
    metadataError.clear();
    const bool unprovenWritten = provenRead && codec.writeModel(unproven, unprovenPath, metadataError);
    interop::LibraryMetadata unprovenRead;
    const bool unprovenDecoded = unprovenWritten && codec.read(unprovenPath, unprovenRead, metadataError) &&
        codec.verifyCurrentCompiler(unprovenRead, metadataError);
    tests.expect(unprovenDecoded, "metadata without the evidence is valid content (" + metadataError + ")");

    for (const std::string_view kind : {std::string_view("Direct")})
    {
        const std::string temporaries =
            "return new " + std::string(kind) + "(1) + new " + std::string(kind) + "(2);";
        const std::string locals = std::string(kind) + " a = new " + std::string(kind) + "(1); " +
            std::string(kind) + " b = new " + std::string(kind) + "(2); return a + b;";
        const CheckResult provenTemporaries = checkConsumer(
            root, std::string(kind) + "-proven-temporaries", library, provenPath, temporaries);
        tests.expect(provenTemporaries.accepted,
            std::string(kind) + ": temporaries are accepted when non-escape is proven (" +
                provenTemporaries.diagnostics + ")");
        const CheckResult unprovenTemporaries = checkConsumer(
            root, std::string(kind) + "-unproven-temporaries", library, unprovenPath, temporaries);
        tests.expect(!unprovenTemporaries.accepted &&
                unprovenTemporaries.diagnostics.find("no viable") != std::string::npos,
            std::string(kind) + ": temporaries are rejected without non-escape evidence (" +
                unprovenTemporaries.diagnostics + ")");
        const CheckResult unprovenLocals = checkConsumer(
            root, std::string(kind) + "-unproven-locals", library, unprovenPath, locals);
        tests.expect(unprovenLocals.accepted,
            std::string(kind) + ": addressable locals stay accepted without the evidence (" +
                unprovenLocals.diagnostics + ")");
    }

    // The evidence is only meaningful on an operator; claiming it elsewhere is contradictory content.
    interop::LibraryMetadata contradictory = proven;
    bool flagged = false;
    for (auto& member : contradictory.members)
    {
        constexpr std::uint8_t kConstructorKind = 3U;
        if (flagged || member.kind != kConstructorKind) continue;
        member.flags |= interop::kMetadataOperatorInOperandsNoEscape;
        flagged = true;
    }
    metadataError.clear();
    const bool contradictoryVerified = codec.verifyCurrentCompiler(contradictory, metadataError);
    tests.expect(provenRead && flagged && !contradictoryVerified &&
            metadataError.find("operator in-operand non-escape contract has an invalid signature") !=
                std::string::npos,
        "the evidence on a constructor is rejected as an invalid contract (" + metadataError + ")");
    metadataError.clear();
    tests.expect(provenRead && codec.verifyCurrentCompiler(proven, metadataError),
        "the producer metadata itself verifies (" + metadataError + ")");

    std::filesystem::remove_all(root, ignored);
    return tests.result();
}
