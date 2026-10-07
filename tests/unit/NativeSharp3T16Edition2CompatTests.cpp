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
        CompilationUnitContract{"Core", "Core.ns", {}, "core-a"},
        CompilationUnitContract{"App", "Main.ns", {"Core"}, "app-a"}};
    std::string error;
    check(validateCompilationPlan(units, error), "modules with cumulative semantics coexist");

    const BinaryModuleContract oldModule{2, "windows-x64", "abi-A", true};
    const BinaryModuleContract compatibleConsumer{3, "windows-x64", "abi-A", true};
    const BinaryModuleContract changedConsumer{3, "windows-x64", "abi-B", true};
    check(classifyModuleCompatibility(oldModule, compatibleConsumer) == ModuleCompatibility::Direct,
        "old metadata with compatible ABI loads directly");
    check(classifyModuleCompatibility(oldModule, changedConsumer) == ModuleCompatibility::ExplicitAdapterRequired,
        "ABI mismatch requires an explicit adapter");
    auto duplicateModule = units;
    duplicateModule[1].canonicalModule = "Core";
    check(!validateCompilationPlan(duplicateModule, error), "module identity cannot be duplicated");
    auto corrupt = units;
    corrupt[0].sourceFingerprint.clear();
    check(!validateCompilationPlan(corrupt, error), "missing source fingerprint is rejected");
    auto unresolved = units;
    unresolved[1].imports = {"Missing.Module"};
    check(!validateCompilationPlan(unresolved, error), "unresolved module import is rejected");

    auto changed = units;
    changed[0].sourceFingerprint = "legacy-b";
    const auto sourceRebuild = incrementalRebuildSet(units, changed);
    check(sourceRebuild.contains("Core") && sourceRebuild.contains("App"),
        "source change rebuilds module and dependent");
    check(incrementalRebuildSet(units, units).empty(), "unchanged graph performs no rebuild");

    if (failures != 0) return 1;
    std::cout << "PASS: " << checks << " cumulative compatibility checks\n";
    return 0;
}
