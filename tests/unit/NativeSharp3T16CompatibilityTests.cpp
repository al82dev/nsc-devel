#include "nativesharp/Language/CompatibilitySemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    const BinaryModuleContract producer{2, "windows-x64", "abi-A", true};
    const BinaryModuleContract compatibleConsumer{3, "windows-x64", "abi-A", true};
    const BinaryModuleContract changedConsumer{3, "windows-x64", "abi-B", true};
    const BinaryModuleContract corrupt{2, "windows-x64", "abi-A", false};
    tests.expect(classifyModuleCompatibility(producer, compatibleConsumer) ==
            ModuleCompatibility::Direct &&
            classifyModuleCompatibility(producer, changedConsumer) ==
                ModuleCompatibility::ExplicitAdapterRequired &&
            classifyModuleCompatibility(corrupt, compatibleConsumer) ==
                ModuleCompatibility::Rejected,
        "modules load directly only when structural and ABI contracts are compatible");

    tests.expect(migrationDiagnosticCode(MigrationIssue::CopyToMove) == "MIG3003" &&
            migrationDiagnosticCode(MigrationIssue::CleanupOrder) == "MIG3007",
        "migration issues have stable MIG3xxx codes separate from language errors");
    tests.expect(mayApplyAutomaticFix({MigrationIssue::NewlyContextualWord, "@record", true}) &&
            !mayApplyAutomaticFix({MigrationIssue::OverloadResolution, "Select<int>", false}),
        "automatic fix-its require a proof of semantic equivalence");

    const auto privateChange = classifyApiChange(false, true, false, true);
    const auto layoutChange = classifyApiChange(false, true, false, false);
    const auto effectChange = classifyApiChange(false, false, true, false);
    tests.expect(privateChange.abi == AbiCompatibility::Neutral && !privateChange.requiresConsumerRebuild &&
            layoutChange.abi == AbiCompatibility::Breaking && layoutChange.requiresConsumerRebuild &&
            effectChange.source == SourceCompatibility::Breaking,
        "source and ABI compatibility are classified independently and drive rebuild decisions");

    std::string error;
    const std::array<MigrationChapter, 2> guide = {{{"syntax", 1, true, true, true},
        {"ownership", 2, true, true, true}}};
    tests.expect(validateMigrationGuide(guide, error),
        "migration guide chapters contain executable before/after and manual-decision inventories");
    tests.expect(!removalAllowedInThreeX(false, false) && removalAllowedInThreeX(false, true) &&
            removalAllowedInThreeX(true, false) && removalAllowedInThreeX(true, true),
        "3.x removal requires prior deprecation unless a documented security/UB exception applies");
    return tests.result();
}
