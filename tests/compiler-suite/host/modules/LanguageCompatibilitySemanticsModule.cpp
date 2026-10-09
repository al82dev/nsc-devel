#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/CompatibilitySemantics.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-004", "Host.Language", "unit",
        "PR", {"language", "compatibility"}, std::move(function)});
}

BinaryModuleContract module(std::string fingerprint = "abi", int metadata = 2)
{
    return {metadata, "windows-x64", std::move(fingerprint), true};
}

CompilationUnitContract unit(
    std::string name,
    std::string fingerprint,
    std::vector<std::string> imports = {})
{
    return {name, name + ".ns", std::move(imports), std::move(fingerprint)};
}

void registerRejectedCompatibilityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-040", [](TestContext& context) {
        const BinaryModuleContract valid = module();
        BinaryModuleContract changed = valid;
        changed.structurallyValid = false;
        context.expect(classifyModuleCompatibility(changed, valid) == ModuleCompatibility::Rejected &&
                classifyModuleCompatibility(valid, changed) == ModuleCompatibility::Rejected,
            "TL-LANG-040 either structurally invalid side must reject compatibility");
        changed = valid; changed.target = "linux-x64";
        context.expect(classifyModuleCompatibility(changed, valid) == ModuleCompatibility::Rejected &&
                classifyModuleCompatibility(valid, changed) == ModuleCompatibility::Rejected,
            "TL-LANG-040 either non-Windows-x64 target must reject compatibility");
        context.expect(classifyModuleCompatibility(module("abi", 3), module("abi", 2)) ==
                ModuleCompatibility::Rejected,
            "TL-LANG-040 a newer producer metadata major must reject compatibility");
        context.expect(classifyModuleCompatibility(module("abi", 2), module("other", 3)) ==
                ModuleCompatibility::ExplicitAdapterRequired,
            "TL-LANG-040 an older producer must continue to fingerprint classification");
        changed = valid; changed.structurallyValid = false; changed.abiFingerprint = "other";
        context.expect(classifyModuleCompatibility(changed, valid) == ModuleCompatibility::Rejected,
            "TL-LANG-040 structural rejection must have priority over fingerprint mismatch");
    });
}

void registerFingerprintCompatibilityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-041", [](TestContext& context) {
        context.expect(classifyModuleCompatibility(module("same"), module("same")) ==
                ModuleCompatibility::Direct,
            "TL-LANG-041 equal fingerprints must be directly compatible");
        context.expect(classifyModuleCompatibility(module("producer"), module("consumer")) ==
                ModuleCompatibility::ExplicitAdapterRequired,
            "TL-LANG-041 different valid fingerprints must require an explicit adapter");
    });
}

void registerValidCompilationPlanCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-042", [](TestContext& context) {
        std::string error = "stale";
        const std::vector<CompilationUnitContract> empty;
        context.expect(!validateCompilationPlan(empty, error) && error.empty(),
            "TL-LANG-042 an empty plan must fail with the current empty error channel");
        const std::array one{unit("A", "a1")};
        context.expect(validateCompilationPlan(one, error) && error.empty(),
            "TL-LANG-042 one complete unit must succeed and clear stale errors");
        const std::array many{
            unit("A", "a1"), unit("B", "b1", {"A"}), unit("C", "c1", {"A", "B"})};
        context.expect(validateCompilationPlan(many, error) && error.empty(),
            "TL-LANG-042 a coherent multi-unit import graph must succeed");
    });
}

void registerInvalidCompilationPlanCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-043", [](TestContext& context) {
        const std::string identityError =
            "compilation plan requires unique source identities and fingerprints";
        std::string error;
        auto expectIdentityError = [&](std::vector<CompilationUnitContract> units) {
            context.expect(!validateCompilationPlan(units, error) && error == identityError,
                "TL-LANG-043 missing or duplicate source identity data must use the exact first error");
        };
        expectIdentityError({{"", "a.ns", {}, "hash"}});
        expectIdentityError({{"A", "", {}, "hash"}});
        expectIdentityError({{"A", "a.ns", {}, ""}});
        expectIdentityError({unit("A", "a1"), {"B", "A.ns", {}, "b1"}});
        context.expect(!validateCompilationPlan(
                std::array{unit("A", "a1"), CompilationUnitContract{"A", "b.ns", {}, "b1"}}, error) &&
                error == "a compilation plan cannot define the same module more than once",
            "TL-LANG-043 duplicate canonical modules must use the exact module error");
        context.expect(!validateCompilationPlan(
                std::array{unit("A", "a1", {"Missing"})}, error) &&
                error == "compilation plan contains an unresolved module import",
            "TL-LANG-043 unresolved imports must use the exact import error");
        CompilationUnitContract invalid{"", "", {"Missing"}, ""};
        context.expect(!validateCompilationPlan(std::array{invalid}, error) && error == identityError,
            "TL-LANG-043 unit-shape errors must precede unresolved-import errors");
    });
}

void registerIncrementalClosureCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-044", [](TestContext& context) {
        const std::array previous{
            unit("A", "a1"), unit("B", "b1", {"A"}), unit("C", "c1", {"A"}),
            unit("D", "d1", {"B", "C"})};
        context.expect(incrementalRebuildSet(previous, previous).empty(),
            "TL-LANG-044 an unchanged graph must rebuild nothing");
        auto current = previous;
        current[3].sourceFingerprint = "d2";
        context.expect(incrementalRebuildSet(previous, current) == std::set<std::string>{"D"},
            "TL-LANG-044 a changed leaf must rebuild only itself");
        current = previous; current[0].sourceFingerprint = "a2";
        context.expect(incrementalRebuildSet(previous, current) ==
                std::set<std::string>{"A", "B", "C", "D"},
            "TL-LANG-044 a changed root must rebuild the complete transitive diamond");
        current = previous; current[1].sourceFingerprint = "b2";
        context.expect(incrementalRebuildSet(previous, current) == std::set<std::string>{"B", "D"},
            "TL-LANG-044 one changed diamond branch must rebuild itself and its join");
    });
}

void registerIncrementalMembershipCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-045", [](TestContext& context) {
        const std::array previous{unit("Removed", "r1"), unit("Keep", "k1")};
        const std::array withoutRemoved{unit("Keep", "k1")};
        context.expect(incrementalRebuildSet(previous, withoutRemoved).empty(),
            "TL-LANG-045 a removed module must not appear in the current rebuild set");
        const std::array withNew{
            unit("Keep", "k1"), unit("New", "n1"), unit("Consumer", "c1", {"New"})};
        const auto rebuilt = incrementalRebuildSet(withoutRemoved, withNew);
        context.expect(rebuilt == std::set<std::string>{"Consumer", "New"},
            "TL-LANG-045 a new module and its current dependents must rebuild deterministically");
    });
}

void registerMigrationDiagnosticCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-046", [](TestContext& context) {
        const std::array issues{
            MigrationIssue::NewlyContextualWord, MigrationIssue::OverloadResolution,
            MigrationIssue::CopyToMove, MigrationIssue::Nullability, MigrationIssue::ImportLookup,
            MigrationIssue::NumericNarrowing, MigrationIssue::CleanupOrder};
        const std::array codes{"MIG3001", "MIG3002", "MIG3003", "MIG3004",
            "MIG3005", "MIG3006", "MIG3007"};
        for (std::size_t index = 0; index < issues.size(); ++index)
            context.expect(migrationDiagnosticCode(issues[index]) == codes[index],
                "TL-LANG-046 every migration issue must map to its exact stable code");
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
        context.expect(migrationDiagnosticCode(static_cast<MigrationIssue>(999)) == "MIG3000",
            "TL-LANG-046 an external migration issue must use the current fallback code");
    });
}

void registerAutomaticFixCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-047", [](TestContext& context) {
        for (const bool replacement : std::array{false, true})
            for (const bool equivalent : std::array{false, true})
            {
                const MigrationFix fix{MigrationIssue::CopyToMove,
                    replacement ? "replacement" : "", equivalent};
                context.expect(mayApplyAutomaticFix(fix) == (replacement && equivalent),
                    "TL-LANG-047 automatic fixes must require replacement and proven equivalence");
            }
    });
}

void registerApiChangeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-048", [](TestContext& context) {
        for (unsigned mask = 0; mask < 16; ++mask)
        {
            const bool signature = (mask & 1U) != 0;
            const bool layout = (mask & 2U) != 0;
            const bool effect = (mask & 4U) != 0;
            const bool privateOnly = (mask & 8U) != 0;
            const auto actual = classifyApiChange(signature, layout, effect, privateOnly);
            const bool sourceBreak = !privateOnly && (signature || effect);
            const bool abiBreak = !privateOnly && (signature || layout);
            context.expect(actual.source == (sourceBreak ? SourceCompatibility::Breaking
                                                        : SourceCompatibility::Compatible) &&
                    actual.abi == (abiBreak ? AbiCompatibility::Breaking
                                           : AbiCompatibility::Neutral) &&
                    actual.requiresConsumerRebuild == (sourceBreak || abiBreak),
                "TL-LANG-048 all sixteen API-change flag combinations must classify exactly");
        }
    });
}

void registerMigrationGuideCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-049", [](TestContext& context) {
        const std::string expected =
            "migration chapters require ordered identities, before/after, and manual decisions";
        std::string error = "stale";
        const std::array valid{
            MigrationChapter{"one", 1, true, true, true},
            MigrationChapter{"two", 2, true, true, true}};
        context.expect(validateMigrationGuide(valid, error) && error.empty(),
            "TL-LANG-049 complete ordered chapters must succeed and clear stale errors");
        const std::vector<MigrationChapter> empty;
        context.expect(!validateMigrationGuide(empty, error) &&
                error == "migration guide has no executable chapters",
            "TL-LANG-049 an empty guide must use its exact error");
        for (const MigrationChapter& invalid : std::array{
                 MigrationChapter{"", 1, true, true, true},
                 MigrationChapter{"x", 1, false, true, true},
                 MigrationChapter{"x", 1, true, false, true},
                 MigrationChapter{"x", 1, true, true, false}})
            context.expect(!validateMigrationGuide(std::array{invalid}, error) && error == expected,
                "TL-LANG-049 each incomplete chapter must use the exact chapter error");
        const std::array unordered{
            MigrationChapter{"one", 2, true, true, true},
            MigrationChapter{"two", 1, true, true, true}};
        const std::array duplicate{
            MigrationChapter{"same", 1, true, true, true},
            MigrationChapter{"same", 2, true, true, true}};
        context.expect(!validateMigrationGuide(unordered, error) && error == expected &&
                !validateMigrationGuide(duplicate, error) && error == expected,
            "TL-LANG-049 order and identity uniqueness must be enforced exactly");
        for (const bool security : std::array{false, true})
            for (const bool deprecated : std::array{false, true})
                context.expect(removalAllowedInThreeX(security, deprecated) ==
                        (security || deprecated),
                    "TL-LANG-049 removal must require either documented justification");
    });
}
}

void registerLanguageCompatibilitySemanticsModule(TestRegistry& registry)
{
    registerRejectedCompatibilityCase(registry);
    registerFingerprintCompatibilityCase(registry);
    registerValidCompilationPlanCase(registry);
    registerInvalidCompilationPlanCase(registry);
    registerIncrementalClosureCase(registry);
    registerIncrementalMembershipCase(registry);
    registerMigrationDiagnosticCase(registry);
    registerAutomaticFixCase(registry);
    registerApiChangeCase(registry);
    registerMigrationGuideCase(registry);
}
}
