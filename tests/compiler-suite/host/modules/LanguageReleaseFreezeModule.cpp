#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/ReleaseSemantics.h"

#include <array>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-018", "Host.Language", "unit", "PR",
        {"language", "release"}, std::move(function)});
}

FreezeManifest completeFreezeManifest()
{
    return {"syntax-v1", "semantics-v1", "win64-abi-v1", "nsmeta-v1"};
}

SyntaxFreezeManifest completeSyntaxManifest()
{
    return {"grammar-v1", "grammar-sha", "precedence-sha", "reserved-sha", "recovery-sha",
        true, true, true, true, true};
}

SemanticFreezeManifest completeSemanticManifest()
{
    return {"overload-v1", "conversion-v1", "inference-v1", "ownership-v1", "error-v1",
        "async-v1", "memory-v1", {"3.1", "3.2.4", "4.0"}, true, true};
}

Win64AbiFreezeManifest completeAbiManifest()
{
    return {"windows-x64", "mangling-v1", "layout-v1", "calling-v1", "variant-v1",
        "trait-v1", "error-v1", "coroutine-v1", "nsmeta-v1", true, true, true, true};
}

void registerBaseFreezeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-231", [](TestContext& context) {
        std::string error = "stale";
        context.expect(validateFreezeManifest(completeFreezeManifest(), error) && error.empty(),
            "TL-LANG-231 a complete freeze manifest must pass and clear stale errors");
        for (std::size_t field = 0; field < 4; ++field)
        {
            auto manifest = completeFreezeManifest();
            std::array<std::string*, 4> revisions = {&manifest.syntaxRevision,
                &manifest.semanticRevision, &manifest.win64AbiRevision, &manifest.metadataRevision};
            revisions[field]->clear();
            context.expect(!validateFreezeManifest(manifest, error) &&
                    error == "freeze manifest requires syntax, semantic, Win64 ABI, and metadata revisions",
                "TL-LANG-231 each absent freeze revision must report the exact shared error");
        }
    });
}

void registerSyntaxFreezeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-232", [](TestContext& context) {
        std::string error;
        context.expect(validateSyntaxFreeze(completeSyntaxManifest(), error) && error.empty(),
            "TL-LANG-232 a complete syntax freeze must pass");
        for (std::size_t field = 0; field < 5; ++field)
        {
            auto manifest = completeSyntaxManifest();
            std::array<std::string*, 5> revisions = {&manifest.grammarRevision,
                &manifest.canonicalGrammarHash, &manifest.precedenceHash,
                &manifest.reservedWordsHash, &manifest.recoveryRulesHash};
            revisions[field]->clear();
            context.expect(!validateSyntaxFreeze(manifest, error) &&
                    error == "syntax freeze requires versioned hashes for every grammar surface",
                "TL-LANG-232 each absent syntax revision or hash must report the exact error");
        }
        auto sourceMissing = completeSyntaxManifest();
        sourceMissing.generatedFromCanonicalSource = false;
        context.expect(!validateSyntaxFreeze(sourceMissing, error) &&
                error == "reference grammar must be generated from its canonical source",
            "TL-LANG-232 missing canonical generation must report the exact error");
        for (std::size_t gate = 0; gate < 4; ++gate)
        {
            auto manifest = completeSyntaxManifest();
            std::array<bool*, 4> gates = {&manifest.parserCorpusPassed,
                &manifest.ambiguityCheckPassed, &manifest.formatterRoundTripPassed,
                &manifest.specificationSnippetsPassed};
            *gates[gate] = false;
            context.expect(!validateSyntaxFreeze(manifest, error) &&
                    error == "syntax freeze evidence is incomplete",
                "TL-LANG-232 each absent syntax evidence gate must report the exact error");
        }
    });
}

void registerSemanticFreezeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-233", [](TestContext& context) {
        std::string error;
        context.expect(validateSemanticFreeze(completeSemanticManifest(), error) && error.empty(),
            "TL-LANG-233 milestones 3.1, 3.2, and 4.x must pass with complete evidence");
        for (const char* milestone : {"", "3.0", "2.9", "5.0", "next"})
        {
            auto manifest = completeSemanticManifest();
            manifest.deferredIssueMilestones = {milestone};
            context.expect(!validateSemanticFreeze(manifest, error) &&
                    error == "semantic freeze exceptions must target a post-3.0 milestone",
                "TL-LANG-233 every milestone outside 3.1, 3.2, or 4.x must report the exact error");
        }
        auto missingRevision = completeSemanticManifest();
        missingRevision.ownershipAndDropRevision.clear();
        context.expect(!validateSemanticFreeze(missingRevision, error) &&
                error == "semantic freeze requires a revision for every normative subsystem",
            "TL-LANG-233 a missing normative subsystem revision must fail first");
        auto missingEvidence = completeSemanticManifest();
        missingEvidence.differentialConfigurationsDeterministic = false;
        context.expect(!validateSemanticFreeze(missingEvidence, error) &&
                error == "semantic freeze evidence is incomplete",
            "TL-LANG-233 incomplete semantic evidence must report the exact error");
    });
}

void registerAbiFreezeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-234", [](TestContext& context) {
        std::string error;
        context.expect(validateWin64AbiFreeze(completeAbiManifest(), error) && error.empty(),
            "TL-LANG-234 complete windows-x64 ABI evidence must pass");
        for (const char* target : {"", "Windows-x64", "windows-arm64", "linux-x64"})
        {
            auto manifest = completeAbiManifest();
            manifest.target = target;
            context.expect(!validateWin64AbiFreeze(manifest, error) &&
                    error == "NativeSharp 3.0 ABI freeze is restricted to windows-x64",
                "TL-LANG-234 every noncanonical target must report the exact target error");
        }
        for (std::size_t field = 0; field < 8; ++field)
        {
            auto manifest = completeAbiManifest();
            std::array<std::string*, 8> revisions = {&manifest.manglingRevision,
                &manifest.layoutRevision, &manifest.callingConventionRevision,
                &manifest.variantRevision, &manifest.traitObjectRevision, &manifest.errorRevision,
                &manifest.publicCoroutineRevision, &manifest.moduleMetadataRevision};
            revisions[field]->clear();
            context.expect(!validateWin64AbiFreeze(manifest, error) &&
                    error == "Win64 ABI freeze requires a revision for every public ABI surface",
                "TL-LANG-234 each absent public ABI revision must report the exact error");
        }
        for (std::size_t gate = 0; gate < 4; ++gate)
        {
            auto manifest = completeAbiManifest();
            std::array<bool*, 4> gates = {&manifest.preservedDllFixturesPassed,
                &manifest.crossVersionConsumerPassed, &manifest.symbolSnapshotsPassed,
                &manifest.pdbContractPassed};
            *gates[gate] = false;
            context.expect(!validateWin64AbiFreeze(manifest, error) &&
                    error == "Win64 ABI freeze evidence is incomplete",
                "TL-LANG-234 each absent ABI evidence gate must report the exact error");
        }
    });
}
}

void registerLanguageReleaseFreezeModule(TestRegistry& registry)
{
    registerBaseFreezeCase(registry);
    registerSyntaxFreezeCase(registry);
    registerSemanticFreezeCase(registry);
    registerAbiFreezeCase(registry);
}
}
