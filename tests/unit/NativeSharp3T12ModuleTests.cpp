#include "nativesharp/Language/ModuleSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    tests.expect(canonicalWindowsModuleIdentity("Demo", "Math\\Vectors") ==
            canonicalWindowsModuleIdentity("demo", "math/vectors"),
        "module identity is deterministic under Windows case/path rules");

    std::string error;
    const std::array<ModuleDependency, 2> bindingCycle = {{{"A", "B", ModuleDependencyKind::Binding},
        {"B", "A", ModuleDependencyKind::Binding}}};
    const std::array<ModuleDependency, 2> layoutCycle = {{{"A", "B", ModuleDependencyKind::Layout},
        {"B", "A", ModuleDependencyKind::Layout}}};
    tests.expect(moduleGraphResolvable(bindingCycle, error) && !moduleGraphResolvable(layoutCycle, error),
        "binding cycles are allowed while unresolved layout cycles fail");

    std::vector<Import> normalized;
    const std::array<Import, 2> imports = {{{ImportKind::Symbol, "Demo.Z", "Zed"},
        {ImportKind::Trait, "Demo.A", "Alpha"}}};
    tests.expect(normalizeImports(imports, normalized, error) && normalized[0].alias == "Alpha",
        "explicit imports normalize independently of textual order");
    const std::array<Import, 2> collision = {{{ImportKind::Symbol, "A.Value", "Value"},
        {ImportKind::Symbol, "B.Value", "Value"}}};
    tests.expect(!normalizeImports(collision, normalized, error),
        "colliding imports require an explicit disambiguating alias");

    tests.expect(canAccess(Visibility::Package, false, true, false) &&
            !canAccess(Visibility::Module, false, true, false) &&
            canAccess(Visibility::Private, false, false, true),
        "granular visibility and explicit friend access share one access matrix");
    tests.expect(reexportedIdentity("Core.Vector", "Facade.Vector") == "Core.Vector",
        "re-export facades preserve original symbol identity and linker instance");

    const Availability available{3, 4, "windows-x64"};
    tests.expect(availabilityStatus(available, 3, "windows-x64", false) == AvailabilityStatus::Available &&
            availabilityStatus(available, 4, "windows-x64", false) == AvailabilityStatus::Removed &&
            availabilityStatus(available, 3, "linux-x64", false) == AvailabilityStatus::WrongTarget,
        "availability applies edition and architecture without package version solving");

    const std::array<PublicApiSymbol, 3> api = {{{"B", "fn()", "noheap", "", true},
        {"Private", "fn()", "", "", false}, {"A", "struct", "", "size=8", true}}};
    const std::array<PublicApiSymbol, 2> reordered = {{{"A", "struct", "", "size=8", true},
        {"B", "fn()", "noheap", "", true}}};
    tests.expect(publicApiFingerprintInput(api) == publicApiFingerprintInput(reordered),
        "public API fingerprint ignores private symbols and declaration order");
    return tests.result();
}
