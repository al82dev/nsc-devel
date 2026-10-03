#include "nativesharp/Language/TraitSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;

    TraitContract valid{"Demo.Hash", {{"Hash(self)->uint", false, false, false}}, false};
    TraitContract storage{"Demo.Bad", {}, true};
    tests.expect(validateTrait(valid, error) && !validateTrait(storage, error),
        "traits describe behavior and never introduce instance layout");

    const std::array<ImplCandidate, 1> local = {{{"Demo.Hash", "Demo.Key", "Demo", true, true}}};
    const std::array<ImplCandidate, 1> orphan = {{{"Foreign.Hash", "Foreign.Key", "Demo", false, false}}};
    tests.expect(validateImplCoherence(local, error) && !validateImplCoherence(orphan, error),
        "impl coherence enforces an orphan rule across separately compiled modules");

    const std::array<AssociatedProjection, 2> projections = {{{"T.Item", "U.Item"}, {"U.Item", "builtin:int"}}};
    std::string normalized;
    tests.expect(normalizeAssociatedType("T.Item", projections, normalized, error) &&
            normalized == "builtin:int",
        "associated projections normalize transitively");
    const std::array<AssociatedProjection, 2> cycle = {{{"T.Item", "U.Item"}, {"U.Item", "T.Item"}}};
    tests.expect(!normalizeAssociatedType("T.Item", cycle, normalized, error),
        "associated projection cycles are diagnosed");

    tests.expect(validateOpaqueResultTypes(
            std::array<std::string, 2>{"Demo.Iterator", "Demo.Iterator"}, error) &&
            !validateOpaqueResultTypes(
                std::array<std::string, 2>{"Demo.Left", "Demo.Right"}, error),
        "some Trait has one concrete type per declaration and never boxes");
    tests.expect(isObjectSafe(valid.methods, error) && !isObjectSafe(
            std::array<TraitMethod, 1>{{{"Map<T>(self)", false, true, false}}}, error),
        "dyn Trait rejects methods that cannot occupy a stable vtable slot");
    const auto object = windowsX64TraitObjectLayout();
    tests.expect(object.size == 8 && object.alignment == 8 && object.descriptorInterfaceMap &&
            object.nullIsZero && !object.conversionAllocates &&
            dispatchMetadata(DispatchKind::Static, "Demo.Hash") == "static:Demo.Hash" &&
            dispatchMetadata(DispatchKind::Dynamic, "Demo.Hash") == "dyn:Demo.Hash",
        "Win64 trait objects are declared descriptor-backed references and metadata preserves dispatch kind");
    tests.expect(resolveExtension(true, 2) == ExtensionResolution::InstanceMember &&
            resolveExtension(false, 1) == ExtensionResolution::UniqueExtension &&
            resolveExtension(false, 2) == ExtensionResolution::Ambiguous,
        "real members win and competing imported extensions remain explicit ambiguity");
    tests.expect(operatorTrait("+") == "lang:Add" && operatorTrait("==") == "lang:Equality" &&
            !operatorCanBeOverloaded("&&") && operatorCanBeOverloaded("+"),
        "operators map to stable lang traits while short-circuit operators remain intrinsic");
    return tests.result();
}
