#include "DriverTestSupport.h"

#include "nativesharp/Driver/ExecutableMigrationGuide.h"
#include "nativesharp/Driver/FinalBaselineContract.h"
#include "nativesharp/Driver/MigrationAnalyzer.h"
#include "nativesharp/Driver/MigrationBaseline.h"
#include "nativesharp/Driver/MigrationCompatibilityMatrix.h"
#include "nativesharp/Driver/MigrationGuide.h"
#include "nativesharp/Driver/NumericContract.h"
#include "nativesharp/Driver/PromotionAct.h"
#include "nativesharp/Driver/ReleaseCandidateFreeze.h"
#include "nativesharp/Driver/ToolchainContract.h"

namespace nsc::suite
{
namespace
{
using namespace driver_support;
using namespace nativesharp::driver;

using CurrentDocument = std::string (*)();
using WriteDocument = bool (*)(const std::filesystem::path&, std::string&);
using VerifyDocument = bool (*)(const std::filesystem::path&, std::string&);

void canonicalDocument(
    TestContext& context,
    std::size_t variant,
    std::string_view identity,
    CurrentDocument current,
    WriteDocument write,
    VerifyDocument verify)
{
    TemporaryTree tree(identity);
    std::string error;
    const auto path = tree.root() / "nested" / "document.txt";
    if (variant == 3U)
    {
        context.expect(!verify(tree.root() / "missing.txt", error) && !error.empty(),
            "missing release document is rejected");
        return;
    }
    if (variant == 4U)
    {
        const auto invalid = tree.write("invalid.txt", "invalid\n");
        context.expect(!verify(invalid, error) && !error.empty(),
            "mutated release document is rejected");
        return;
    }
    if (variant == 5U)
    {
        context.expect(!write({}, error) && !error.empty(),
            "empty release document destination is rejected");
        return;
    }
    const bool written = write(path, error);
    const std::string bytes = readText(path);
    context.expect(written && bytes == current() && verify(path, error) && error.empty(),
        "release document bytes and verification match the canonical producer");
}

void migrationLexicalContract(TestContext& context, std::size_t variant)
{
    const std::string source = variant == 3U ?
        "// nsc-migration:disable-next-line all\nint async;\n" : variant == 4U ?
        "// nsc-migration:generated\nint async;\n" : variant == 5U ?
        "string text = \"int async;\"; // int match;\n" : variant == 8U ?
        std::string(20000U, ' ') + "int async;\n" : "int async;\n";
    const auto analysis = analyzeMigrationSource("sample.ns", source);
    const bool empty = variant == 3U || variant == 4U || variant == 5U;
    context.expect(analysis.generatedSource == (variant == 4U) &&
            analysis.findings.empty() == empty &&
            (empty || analysis.findings.front().line >= 1U),
        "migration tokenizer honors suppression, generated-source marker, comments, and strings");
}

void migrationFixContract(TestContext& context, std::size_t variant)
{
    const std::string source = "byte value = 42;\n";
    MigrationAnalysis analysis = analyzeMigrationSource("value.ns", source);
    if (variant == 3U)
    {
        analysis.findings.front().safeFix->offset = 999U;
    }
    if (variant == 4U)
    {
        MigrationFinding duplicate = analysis.findings.front();
        analysis.findings.push_back(std::move(duplicate));
    }
    if (variant == 5U)
    {
        analysis.findings.front().safeFix->equivalenceProof.clear();
    }
    std::string migrated;
    std::string error;
    const bool result = applySafeMigrationFixes(source, analysis, migrated, error);
    if (variant >= 3U && variant <= 5U)
    {
        context.expect(!result && !error.empty(),
            "out-of-bounds, overlapping, and unproved edits are rejected atomically");
        return;
    }
    context.expect(result && error.empty() && migrated == "byte value = (byte)42;\n",
        "proved nonoverlapping numeric narrowing fix preserves all other source bytes");
}

void executableGuideContract(TestContext& context, std::size_t variant)
{
    const std::string source = variant == 3U ? "" : "byte value = 42;\n";
    const auto analysis = analyzeMigrationSource("value.ns", source);
    auto guide = buildExecutableMigrationGuide(analysis, source);
    std::string error;
    const bool valid = validateExecutableMigrationGuide(guide, error);
    const std::string rendered = renderExecutableMigrationGuide(guide);
    if (variant == 4U && !guide.chapters.empty())
    {
        guide.chapters.front().identity.clear();
        context.expect(!validateExecutableMigrationGuide(guide, error) && !error.empty(),
            "guide validation rejects a chapter without stable identity");
        return;
    }
    context.expect(valid && error.empty() && contains(rendered, "NSMIGRATION-GUIDE"),
        "executable guide construction, validation, and rendering agree");
}

void baselineContract(TestContext& context, std::size_t variant)
{
    canonicalDocument(context, variant, "baseline", currentMigrationBaseline,
        writeMigrationBaseline, verifyMigrationBaseline);
}

void compatibilityMatrixContract(TestContext& context, std::size_t variant)
{
    nativesharp::interop::LibraryApiDiffReport report;
    report.oldSourceFingerprint = "old-source";
    report.newSourceFingerprint = "new-source";
    report.oldAbiSurfaceFingerprint = "old-abi";
    report.newAbiSurfaceFingerprint = "new-abi";
    report.sourceCompatible = variant != 3U;
    report.abiCompatible = variant != 4U;
    report.changes.push_back({variant == 4U ?
        nativesharp::interop::LibraryApiChangeImpact::AbiBreaking :
        variant == 3U ? nativesharp::interop::LibraryApiChangeImpact::SourceBreaking :
        nativesharp::interop::LibraryApiChangeImpact::Additive,
        "type", "Game.Player", "directed change"});
    const auto matrix = buildMigrationCompatibilityMatrix(report, "old-meta", "new-meta");
    const std::string rendered = renderMigrationCompatibilityMatrix(matrix);
    context.expect(matrix.changes.size() == 1U &&
            matrix.changes.front().requiresConsumerRebuild == (variant == 3U || variant == 4U) &&
            contains(rendered, variant == 4U ? "abi=abi-breaking" :
                variant == 3U ? "source=source-breaking" : "source=source-compatible"),
        "migration matrix preserves fingerprints and classifies each API impact exactly");
}

void guideContract(TestContext& context, std::size_t variant)
{
    canonicalDocument(context, variant, "guide", currentMigrationGuide,
        writeMigrationGuide, verifyMigrationGuide);
}

void numericContract(TestContext& context, std::size_t variant)
{
    using namespace nativesharp::llvm_backend;
    FloatingPointState state;
    if (variant == 3U) state.mode = FloatingPointMode::Strict;
    if (variant == 4U) state.mode = FloatingPointMode::Fast;
    if (variant == 5U) state.roundingMode = FloatingPointRoundingMode::TowardZero;
    TemporaryTree tree("numeric");
    const auto path = tree.root() / "numeric.txt";
    std::string error;
    const std::string manifest = numericContractManifest(state);
    const std::string fingerprint = numericContractFingerprintHex(state);
    const bool written = writeNumericContract(path, state, error);
    if (variant == 6U)
    {
        FloatingPointState other = state;
        other.denormalMode = FloatingPointDenormalMode::FlushToZero;
        context.expect(written && !verifyNumericContractFile(path, other, error) && !error.empty(),
            "numeric contract verification rejects a different floating-point state");
        return;
    }
    context.expect(written && readText(path) == manifest && fingerprint.size() == 64U &&
            verifyNumericContractFile(path, state, error),
        "numeric manifest, SHA-256 fingerprint, and file verification share one state projection");
}

void toolchainContract(TestContext& context, std::size_t variant)
{
    canonicalDocument(context, variant, "toolchain", currentToolchainContract,
        writeToolchainContract, verifyToolchainContract);
}

void finalBaselineContract(TestContext& context, std::size_t variant)
{
    canonicalDocument(context, variant, "final-baseline", currentFinalBaselineContract,
        writeFinalBaselineContract, verifyFinalBaselineContract);
}

void freezeContract(TestContext& context, std::size_t variant)
{
    canonicalDocument(context, variant, "freeze", currentReleaseCandidateFreeze,
        writeReleaseCandidateFreeze, verifyReleaseCandidateFreeze);
}

void promotionContract(TestContext& context, std::size_t variant)
{
    canonicalDocument(context, variant, "promotion", currentPromotionAct,
        writePromotionAct, verifyPromotionAct);
}
}

void registerDriverMigration345To432Module(TestRegistry& registry)
{
    addDriverContract(registry, 44U, "unit", migrationLexicalContract);
    addDriverContract(registry, 45U, "integration", migrationFixContract);
    addDriverContract(registry, 46U, "integration", executableGuideContract);
    addDriverContract(registry, 47U, "unit", baselineContract);
    addDriverContract(registry, 48U, "unit", compatibilityMatrixContract);
    addDriverContract(registry, 49U, "unit", guideContract);
    addDriverContract(registry, 50U, "unit", numericContract);
    addDriverContract(registry, 51U, "unit", toolchainContract);
    addDriverContract(registry, 52U, "unit", finalBaselineContract);
    addDriverContract(registry, 53U, "unit", freezeContract);
    addDriverContract(registry, 54U, "unit", promotionContract);
}
}
