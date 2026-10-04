#include "nativesharp/Basic/ExitCode.h"
#include "nativesharp/Driver/Driver.h"
#include "nativesharp/Driver/MigrationAnalyzer.h"
#include "nativesharp/Driver/MigrationCompatibilityMatrix.h"

#include "../support/TestContext.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::filesystem::path writeSource(std::string_view text)
{
    const auto path = std::filesystem::temp_directory_path() /
        "nativesharp-t16-migration-lints.ns";
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
    return path;
}
}

int main()
{
    using namespace nativesharp;
    tests::TestContext tests;
    constexpr std::string_view source = R"NS(
using Old.Core;
int match = 0;
void Probe()
{
    Select(null);
    owned Box destination = source;
    Widget* pointer = null;
    byte narrow = 42;
    raii Guard guard;
    return;
}
// Select(null); byte ignored = 1; int yield = 2;
string text = "using Fake; int trait = 0;";
// nsc-migration:disable-next-line MIG3006
byte suppressed = 7;
)NS";
    const auto analysis = driver::analyzeMigrationSource("fixture.ns", source);
    tests.expect(analysis.findings.size() == 7,
        "migration analyzer reports each specified 2.x to 3.0 semantic risk once");
    for (std::size_t index = 0; index < analysis.findings.size(); ++index)
    {
        tests.expect(!analysis.findings[index].message.empty() &&
                !analysis.findings[index].alternative.empty(),
            "every migration lint has an actionable alternative");
        if (index != 0)
            tests.expect(analysis.findings[index - 1].offset <= analysis.findings[index].offset,
                "migration diagnostics are emitted in deterministic source order");
    }

    const auto generated = driver::analyzeMigrationSource(
        "generated.ns", "// nsc-migration:generated\nbyte value = 1;\n");
    tests.expect(generated.generatedSource && generated.findings.empty(),
        "generated migration sources are recognized and skipped explicitly");

    const std::filesystem::path path = writeSource(source);
    const std::string pathText = path.string();
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string_view> arguments = {"--analyze-migration", pathText};
    const driver::Driver compiler;
    const ExitCode result = compiler.run(arguments, output, error);
    tests.expect(result == ExitCode::Success && error.str().empty() &&
            output.str().find("NSMIGRATION-ANALYSIS 1") != std::string::npos &&
            output.str().find("MIG3001") != std::string::npos &&
            output.str().find("MIG3007") != std::string::npos,
        "nsc exposes the migration analyzer as a productive driver command");

    const std::filesystem::path migratedPath = path.parent_path() /
        "nativesharp-t16-migration-fixed.ns";
    const std::string migratedPathText = migratedPath.string();
    output.str({});
    error.str({});
    const std::vector<std::string_view> fixArguments = {
        "--apply-migration-fixes", pathText, "-o", migratedPathText};
    const ExitCode fixResult = compiler.run(fixArguments, output, error);
    std::ifstream migratedStream(migratedPath, std::ios::binary);
    const std::string migratedText{std::istreambuf_iterator<char>(migratedStream),
        std::istreambuf_iterator<char>()};
    tests.expect(fixResult == ExitCode::Success && error.str().empty() &&
            output.str().find("safe migration fixes applied: 1") != std::string::npos &&
            migratedText.find("byte narrow = (byte)42;") != std::string::npos &&
            migratedText.find("// Select(null); byte ignored = 1;") != std::string::npos,
        "nsc applies only proven edits while preserving formatting and comments");
    const auto fixedAnalysis = driver::analyzeMigrationSource("fixed.ns", migratedText);
    const bool hasAnotherSafeFix = std::any_of(fixedAnalysis.findings.begin(),
        fixedAnalysis.findings.end(), [](const driver::MigrationFinding& finding) {
            return finding.safeFix.has_value();
        });
    tests.expect(!hasAnotherSafeFix,
        "safe migration fixes are idempotent and ambiguous findings remain manual");

    interop::LibraryApiDiffReport apiDiff;
    apiDiff.sourceCompatible = false;
    apiDiff.abiCompatible = false;
    apiDiff.oldSourceFingerprint = "source-v2";
    apiDiff.newSourceFingerprint = "source-v3";
    apiDiff.oldAbiSurfaceFingerprint = "surface-v2";
    apiDiff.newAbiSurfaceFingerprint = "surface-v3";
    apiDiff.changes = {
        {interop::LibraryApiChangeImpact::Additive, "source", "Added", "added"},
        {interop::LibraryApiChangeImpact::SourceBreaking, "source", "Renamed", "changed"},
        {interop::LibraryApiChangeImpact::AbiBreaking, "abi", "Layout", "changed"}};
    const driver::MigrationCompatibilityMatrix matrix =
        driver::buildMigrationCompatibilityMatrix(apiDiff, "metadata-v2", "metadata-v3");
    const std::string matrixText = driver::renderMigrationCompatibilityMatrix(matrix);
    tests.expect(matrix.changes.size() == 3 &&
            matrix.changes[0].sourceClassification == "source-compatible" &&
            matrix.changes[0].abiClassification == "abi-neutral" &&
            !matrix.changes[0].requiresConsumerRebuild &&
            matrix.changes[1].sourceClassification == "source-breaking" &&
            matrix.changes[1].requiresConsumerRebuild &&
            matrix.changes[2].abiClassification == "abi-breaking" &&
            matrix.changes[2].requiresConsumerRebuild,
        "each API change receives independent source/ABI and rebuild classifications");
    tests.expect(matrixText.find("old-source-fingerprint=source-v2") != std::string::npos &&
            matrixText.find("new-abi-surface-fingerprint=surface-v3") != std::string::npos &&
            matrixText.find("old-metadata-abi-fingerprint=metadata-v2") != std::string::npos &&
            matrixText.find("consumer-rebuild=yes") != std::string::npos,
        "migration matrix ties every classification to source, ABI-surface, and NSMETA fingerprints");
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
    std::filesystem::remove(migratedPath, removeError);
    return tests.result();
}
