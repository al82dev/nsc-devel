#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/ModuleSemantics.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-015", "Host.Language", "unit", std::move(profile),
        {"language", "modules"}, std::move(function)});
}

void registerCanonicalIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-182", "PR", [](TestContext& context) {
        context.expect(canonicalWindowsModuleIdentity("Demo", "Math\\Vectors") ==
                "demo::math/vectors",
            "TL-LANG-182 Windows module identities must lowercase ASCII and normalize backslashes");
        context.expect(canonicalWindowsModuleIdentity("DEMO", "MATH/VECTORS") ==
                "demo::math/vectors",
            "TL-LANG-182 case and slash variants must produce identical canonical bytes");
        context.expect(canonicalWindowsModuleIdentity("", "") == "::" &&
                canonicalWindowsModuleIdentity("Demo", "") == "demo::" &&
                canonicalWindowsModuleIdentity("", "Math\\Vector") == "::math/vector",
            "TL-LANG-182 empty package and module boundaries must retain the separator exactly");
    });
}

void registerResolvableGraphsCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-183", "PR", [](TestContext& context) {
        std::string error = "stale";
        context.expect(moduleGraphResolvable({}, error) && error.empty(),
            "TL-LANG-183 an empty module graph must resolve and clear stale errors");
        const std::array<ModuleDependency, 2> linear = {{{"A", "B", ModuleDependencyKind::Layout},
            {"B", "C", ModuleDependencyKind::Layout}}};
        context.expect(moduleGraphResolvable(linear, error) && error.empty(),
            "TL-LANG-183 a linear layout DAG must resolve without an error");
        const std::array<ModuleDependency, 4> diamond = {{{"A", "B", ModuleDependencyKind::Layout},
            {"A", "C", ModuleDependencyKind::Layout}, {"B", "D", ModuleDependencyKind::Layout},
            {"C", "D", ModuleDependencyKind::Layout}}};
        context.expect(moduleGraphResolvable(diamond, error) && error.empty(),
            "TL-LANG-183 a diamond layout DAG must resolve without an error");
    });
}

void registerCyclicGraphsCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-184", "PR", [](TestContext& context) {
        constexpr const char* expectedError = "module layout dependency cycle is not resolvable";
        std::string error;
        const std::array<ModuleDependency, 1> selfCycle = {{{"A", "A", ModuleDependencyKind::Layout}}};
        context.expect(!moduleGraphResolvable(selfCycle, error) && error == expectedError,
            "TL-LANG-184 a layout self-cycle must fail with the exact diagnostic");
        const std::array<ModuleDependency, 3> layoutCycle = {{{"A", "B", ModuleDependencyKind::Layout},
            {"B", "C", ModuleDependencyKind::Layout}, {"C", "A", ModuleDependencyKind::Layout}}};
        context.expect(!moduleGraphResolvable(layoutCycle, error) && error == expectedError,
            "TL-LANG-184 a multi-node layout cycle must fail with the exact diagnostic");
        const std::array<ModuleDependency, 3> bindingCycle = {{{"A", "B", ModuleDependencyKind::Binding},
            {"B", "C", ModuleDependencyKind::Binding}, {"C", "A", ModuleDependencyKind::Binding}}};
        context.expect(moduleGraphResolvable(bindingCycle, error) && error.empty(),
            "TL-LANG-184 binding cycles must remain resolvable and clear the prior error");
    });
}

void registerDeepGraphCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-185", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t nodeCount = 1024;
        std::vector<ModuleDependency> chain;
        chain.reserve(nodeCount - 1);
        for (std::size_t index = 0; index + 1 < nodeCount; ++index)
        {
            chain.push_back({"M" + std::to_string(index), "M" + std::to_string(index + 1),
                ModuleDependencyKind::Layout});
        }
        std::string error;
        context.expect(moduleGraphResolvable(chain, error) && error.empty(),
            "TL-LANG-185 the current recursive resolver must accept a 1024-node acyclic chain");
    });
}

void registerNormalizedImportsCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-186", "PR", [](TestContext& context) {
        const std::array<Import, 6> imports = {{{ImportKind::Symbol, "Demo.Z", "Zed"},
            {ImportKind::Extensions, "Demo.Ext", ""},
            {ImportKind::Trait, "Demo.A", "Alpha"},
            {ImportKind::RestrictedGlob, "Demo.All", ""},
            {ImportKind::Module, "Demo.Core", ""},
            {ImportKind::Symbol, "Demo.Z", "Zed"}}};
        std::vector<Import> result;
        std::string error;
        context.expect(normalizeImports(imports, result, error) && error.empty() && result.size() == 5,
            "TL-LANG-186 valid imports must normalize and remove exact duplicates");
        context.expect(result.size() == 5 && result[0].identity == "Demo.All" &&
                result[1].identity == "Demo.Core" && result[2].identity == "Demo.Ext" &&
                result[3].alias == "Alpha" && result[4].alias == "Zed",
            "TL-LANG-186 normalized imports must follow alias, identity, and kind order exactly");
    });
}

void registerInvalidImportsCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-187", "PR", [](TestContext& context) {
        std::vector<Import> result = {{ImportKind::Module, "stale", ""}};
        std::string error;
        const std::array<Import, 1> emptyTarget = {{{ImportKind::Module, "", ""}}};
        context.expect(!normalizeImports(emptyTarget, result, error) && result.empty() &&
                error == "import target cannot be empty",
            "TL-LANG-187 an empty target must clear output and report the exact first error");
        const std::array<Import, 1> invalidAlias = {{{ImportKind::Extensions, "Demo.Ext", "Alias"}}};
        context.expect(!normalizeImports(invalidAlias, result, error) && result.empty() &&
                error == "extension and restricted-glob imports cannot declare an alias",
            "TL-LANG-187 an extension alias must clear output and report the exact error");
        const std::array<Import, 2> collision = {{{ImportKind::Symbol, "A.Value", "Value"},
            {ImportKind::Trait, "B.Value", "value"}}};
        context.expect(!normalizeImports(collision, result, error) && result.size() == 2 &&
                error == "import name collision: value",
            "TL-LANG-187 a case-insensitive collision must retain sorted output and report its visible name");
    });
}

void registerAccessMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-188", "PR", [](TestContext& context) {
        constexpr std::array visibilities = {
            Visibility::Private, Visibility::Module, Visibility::Package, Visibility::Public};
        for (const Visibility visibility : visibilities)
        {
            for (unsigned mask = 0; mask < 16; ++mask)
            {
                const bool sameModule = (mask & 1U) != 0;
                const bool samePackage = (mask & 2U) != 0;
                const bool explicitFriend = (mask & 4U) != 0;
                const bool sameDeclaration = (mask & 8U) != 0;
                bool expected = true;
                if (visibility == Visibility::Private) expected = sameDeclaration || explicitFriend;
                else if (visibility == Visibility::Module) expected = sameModule || explicitFriend;
                else if (visibility == Visibility::Package) expected = samePackage || explicitFriend;
                context.expect(canAccess(visibility, sameModule, samePackage, explicitFriend,
                        sameDeclaration) == expected,
                    "TL-LANG-188 the complete visibility and relationship matrix must remain exact");
            }
        }
    });
}

void registerReexportIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-189", "PR", [](TestContext& context) {
        context.expect(reexportedIdentity("Core.Vector", "Facade.Vector") == "Core.Vector",
            "TL-LANG-189 a named facade must preserve the original identity");
        context.expect(reexportedIdentity("Core.Vector", "") == "Core.Vector",
            "TL-LANG-189 an empty facade must preserve the original identity");
        context.expect(reexportedIdentity("", "Facade.Vector").empty() &&
                reexportedIdentity("", "").empty(),
            "TL-LANG-189 an empty original identity must remain empty for every facade");
    });
}

void registerAvailabilityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-190", "PR", [](TestContext& context) {
        const Availability windows{"windows-x64"};
        context.expect(availabilityStatus(windows, "windows-x64", false) == AvailabilityStatus::Available &&
                availabilityStatus(windows, "windows-x64", true) == AvailabilityStatus::Deprecated,
            "TL-LANG-190 matching architecture must select available or deprecated exactly");
        context.expect(availabilityStatus(windows, "linux-x64", false) == AvailabilityStatus::WrongTarget &&
                availabilityStatus(windows, "linux-x64", true) == AvailabilityStatus::WrongTarget,
            "TL-LANG-190 architecture mismatch must take priority over deprecation");
        const Availability any{""};
        context.expect(availabilityStatus(any, "arm64", false) == AvailabilityStatus::Available &&
                availabilityStatus(any, "arm64", true) == AvailabilityStatus::Deprecated,
            "TL-LANG-190 empty architecture must match every requested target");
    });
}

void registerPublicApiFingerprintCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-191", "PR", [](TestContext& context) {
        const std::array<PublicApiSymbol, 4> symbols = {{{"B", "fn()", "noheap", "", true},
            {"Private", "fn(secret)", "", "", false},
            {"A", "struct", "readonly", "size=8", true},
            {"C", "field:int", "", "offset=4", true}}};
        const std::string expected =
            "ns-api-v1|A|struct|readonly|size=8;B|fn()|noheap|;C|field:int||offset=4;";
        context.expect(publicApiFingerprintInput(symbols) == expected,
            "TL-LANG-191 only public symbols must contribute in canonical identity order");
        const std::array<PublicApiSymbol, 3> reordered = {{{"C", "field:int", "", "offset=4", true},
            {"B", "fn()", "noheap", "", true}, {"A", "struct", "readonly", "size=8", true}}};
        context.expect(publicApiFingerprintInput(reordered) == expected,
            "TL-LANG-191 declaration order and private symbols must not change fingerprint input bytes");
    });
}
}

void registerLanguageModuleSemanticsModule(TestRegistry& registry)
{
    registerCanonicalIdentityCase(registry);
    registerResolvableGraphsCase(registry);
    registerCyclicGraphsCase(registry);
    registerDeepGraphCase(registry);
    registerNormalizedImportsCase(registry);
    registerInvalidImportsCase(registry);
    registerAccessMatrixCase(registry);
    registerReexportIdentityCase(registry);
    registerAvailabilityCase(registry);
    registerPublicApiFingerprintCase(registry);
}
}
