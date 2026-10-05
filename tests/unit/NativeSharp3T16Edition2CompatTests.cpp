#include "nativesharp/Language/CompatibilitySemantics.h"

#include <array>
#include <iostream>

using namespace nativesharp::language;

namespace
{
int checks = 0;
int failures = 0;
void check(bool value, const char* message)
{
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
}

int main()
{
    const std::array units = {
        EditionCompilationUnit{"Legacy.Core", "Legacy.ns", 2, EditionSemantics::NativeSharp2, {}, "legacy-a"},
        EditionCompilationUnit{"Modern.App", "Main.ns", 3, EditionSemantics::NativeSharp3,
            {"Legacy.Core"}, "modern-a"}};
    std::string error;
    check(validateEditionCompatibilityPlan(units, error), "edition 2 and 3 modules coexist explicitly");

    const BinaryModuleContract oldModule{2, 2, "windows-x64", "abi-A", true};
    const BinaryModuleContract compatibleConsumer{3, 3, "windows-x64", "abi-A", true};
    const BinaryModuleContract changedConsumer{3, 3, "windows-x64", "abi-B", true};
    check(classifyModuleCompatibility(oldModule, compatibleConsumer) == ModuleCompatibility::Direct,
        "old metadata with compatible ABI loads directly");
    check(classifyModuleCompatibility(oldModule, changedConsumer) == ModuleCompatibility::ExplicitAdapterRequired,
        "ABI mismatch requires an explicit adapter");
    auto wrongSemantics = units;
    wrongSemantics[0].semantics = EditionSemantics::NativeSharp3;
    check(!validateEditionCompatibilityPlan(wrongSemantics, error), "edition 2 cannot use edition 3 semantics");
    auto mixedModule = units;
    mixedModule[1].canonicalModule = "Legacy.Core";
    check(!validateEditionCompatibilityPlan(mixedModule, error), "module cannot silently mix editions");
    auto corrupt = units;
    corrupt[0].sourceFingerprint.clear();
    check(!validateEditionCompatibilityPlan(corrupt, error), "missing source fingerprint is rejected");
    auto unresolved = units;
    unresolved[1].imports = {"Missing.Module"};
    check(!validateEditionCompatibilityPlan(unresolved, error), "unresolved mixed-edition import is rejected");

    auto changed = units;
    changed[0].sourceFingerprint = "legacy-b";
    const auto sourceRebuild = editionIncrementalRebuildSet(units, changed);
    check(sourceRebuild.contains("Legacy.Core") && sourceRebuild.contains("Modern.App"),
        "source change rebuilds module and dependent");
    changed = units;
    changed[0].languageEdition = 3;
    changed[0].semantics = EditionSemantics::NativeSharp3;
    const auto editionRebuild = editionIncrementalRebuildSet(units, changed);
    check(editionRebuild.size() == 2, "edition change invalidates module and dependent");
    check(editionIncrementalRebuildSet(units, units).empty(), "unchanged graph performs no rebuild");

    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " edition 2 compatibility checks\n";
    return 0;
}
