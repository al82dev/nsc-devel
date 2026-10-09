#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/TraitSemantics.h"

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
    registry.add(TestCase{std::move(id), "CL-LANG-021", "Host.Language", "unit", "PR",
        {"language", "trait", "validation"}, std::move(function)});
}

void registerValidTraitCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-271", [](TestContext& context) {
        const TraitContract minimal{"Demo.Empty", {}, false};
        const TraitContract methods{"Demo.Convertible",
            {{"convert(Self) -> Value", false, false, false},
                {"create<T>() -> Self", true, true, true}},
            false};
        std::string error = "stale";
        context.expect(validateTrait(minimal, error) && error.empty(),
            "TL-LANG-271 a qualified trait without storage or methods must be valid and clear errors");
        error = "stale";
        context.expect(validateTrait(methods, error) && error.empty(),
            "TL-LANG-271 unique non-empty signatures must pass independently of method capabilities");
    });
}

void registerInvalidTraitCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-272", [](TestContext& context) {
        std::string error;
        const TraitContract allInvalid{"", {{"", false, false, false}}, true};
        context.expect(!validateTrait(allInvalid, error) &&
                error == "trait requires a qualified identity",
            "TL-LANG-272 qualified identity must be the first validation boundary");

        const TraitContract storage{"Demo.Stored", {{"", false, false, false}}, true};
        context.expect(!validateTrait(storage, error) &&
                error == "traits cannot declare instance storage",
            "TL-LANG-272 instance storage must precede method-signature validation");

        const TraitContract emptySignature{"Demo.EmptySignature", {{"", false, false, false}}, false};
        context.expect(!validateTrait(emptySignature, error) &&
                error == "trait method signatures must be non-empty and unique",
            "TL-LANG-272 an empty signature must report the exact shared signature error");

        const TraitContract duplicate{"Demo.Duplicate",
            {{"run()", false, false, false}, {"run()", true, true, true}}, false};
        context.expect(!validateTrait(duplicate, error) &&
                error == "trait method signatures must be non-empty and unique",
            "TL-LANG-272 a duplicate signature must report the exact shared signature error");
    });
}

void registerValidCoherenceCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-273", [](TestContext& context) {
        std::string error = "stale";
        context.expect(validateImplCoherence(std::array<ImplCandidate, 0>{}, error) && error.empty(),
            "TL-LANG-273 an empty implementation set must be coherent and clear errors");
        const std::array<ImplCandidate, 3> implementations = {{
            {"Trait.A", "Type.A", "Module.Trait", true, false},
            {"Trait.A", "Type.B", "Module.Type", false, true},
            {"Trait.B", "Type.A", "Module.Both", true, true},
        }};
        context.expect(validateImplCoherence(implementations, error) && error.empty(),
            "TL-LANG-273 trait ownership, type ownership and both ownership must all be accepted");
    });
}

void registerInvalidCoherenceCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-274", [](TestContext& context) {
        std::string error;
        const std::array<ImplCandidate, 1> orphan = {{{
            "Trait.Orphan", "Type.Orphan", "Module.Orphan", false, false}}};
        context.expect(!validateImplCoherence(orphan, error) &&
                error == "orphan impl: the module owns neither trait nor type",
            "TL-LANG-274 an orphan implementation must report the exact ownership error");

        const std::array<ImplCandidate, 2> duplicate = {{
            {"Trait.Shared", "Type.Shared", "Module.First", true, false},
            {"Trait.Shared", "Type.Shared", "Module.Second", false, true},
        }};
        context.expect(!validateImplCoherence(duplicate, error) &&
                error == "duplicate coherent impl for Trait.Shared for Type.Shared",
            "TL-LANG-274 duplicate identity must ignore the candidate module string");

        auto orphanDuplicate = duplicate;
        orphanDuplicate[1].traitOwnedByModule = false;
        orphanDuplicate[1].typeOwnedByModule = false;
        context.expect(!validateImplCoherence(orphanDuplicate, error) &&
                error == "orphan impl: the module owns neither trait nor type",
            "TL-LANG-274 orphan validation must precede duplicate identity validation per candidate");
    });
}

void registerProjectionSuccessCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-275", [](TestContext& context) {
        std::string result = "stale-result";
        std::string error = "stale-error";
        context.expect(normalizeAssociatedType("Terminal", std::array<AssociatedProjection, 0>{},
                result, error) && result == "Terminal" && error.empty(),
            "TL-LANG-275 an absent projection must preserve the input and clear errors");

        const std::array<AssociatedProjection, 3> chain = {{{"Middle", "Terminal"},
            {"Start", "Middle"}, {"Unused", "Other"}}};
        context.expect(normalizeAssociatedType("Start", chain, result, error) &&
                result == "Terminal" && error.empty(),
            "TL-LANG-275 an acyclic projection chain must reach the exact terminal type");
    });
}

void registerProjectionFailureCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-276", [](TestContext& context) {
        std::string result = "unchanged";
        std::string error;
        const std::array<AssociatedProjection, 2> duplicate = {
            {{"Assoc", "First"}, {"Assoc", "Second"}}};
        context.expect(!normalizeAssociatedType("Assoc", duplicate, result, error) &&
                result == "unchanged" && error == "ambiguous associated type projection",
            "TL-LANG-276 duplicate projection keys must fail before mutating the result");

        const std::array<AssociatedProjection, 2> cycle = {{{"Left", "Right"}, {"Right", "Left"}}};
        result = "unchanged";
        context.expect(!normalizeAssociatedType("Left", cycle, result, error) &&
                result == "unchanged" && error == "cyclic associated type projection",
            "TL-LANG-276 a multi-node projection cycle must fail with the exact cycle error");

        const std::array<AssociatedProjection, 1> selfCycle = {{{"Self", "Self"}}};
        result = "unchanged";
        context.expect(!normalizeAssociatedType("Self", selfCycle, result, error) &&
                result == "unchanged" && error == "cyclic associated type projection",
            "TL-LANG-276 a self projection must use the same iterative cycle boundary");
    });
}

void registerOpaqueResultCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-277", [](TestContext& context) {
        std::string error = "stale";
        context.expect(!validateOpaqueResultTypes(std::array<std::string, 0>{}, error) &&
                error == "opaque result declaration has no return type",
            "TL-LANG-277 an empty return set must report the exact absence error");
        const std::array<std::string, 1> one = {"Concrete"};
        context.expect(validateOpaqueResultTypes(one, error) && error.empty(),
            "TL-LANG-277 one concrete return type must be accepted and clear errors");
        const std::array<std::string, 3> equal = {"Concrete", "Concrete", "Concrete"};
        context.expect(validateOpaqueResultTypes(equal, error) && error.empty(),
            "TL-LANG-277 repeated equal concrete return types must be accepted");
        const std::array<std::string, 3> distinct = {"Concrete", "Concrete", "Other"};
        context.expect(!validateOpaqueResultTypes(distinct, error) &&
                error == "opaque result paths resolve to different concrete types",
            "TL-LANG-277 the first differing concrete return must report the exact mismatch error");
    });
}

void registerObjectSafetyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-278", [](TestContext& context) {
        std::string error = "stale";
        const std::array<TraitMethod, 2> normal = {{{"read()", false, false, false},
            {"write(Value)", true, false, false}}};
        context.expect(isObjectSafe(normal, error) && error.empty(),
            "TL-LANG-278 ordinary and default methods must be object-safe and clear errors");
        const std::array<TraitMethod, 1> generic = {{{"map<T>()", false, true, false}}};
        context.expect(!isObjectSafe(generic, error) &&
                error == "dyn trait methods cannot be generic or return Self by value",
            "TL-LANG-278 a generic method must report the exact object-safety error");
        const std::array<TraitMethod, 1> selfByValue = {{{"clone() -> Self", false, false, true}}};
        context.expect(!isObjectSafe(selfByValue, error) &&
                error == "dyn trait methods cannot be generic or return Self by value",
            "TL-LANG-278 a Self-by-value method must report the exact object-safety error");
        const std::array<TraitMethod, 1> both = {{{"make<T>() -> Self", false, true, true}}};
        context.expect(!isObjectSafe(both, error) &&
                error == "dyn trait methods cannot be generic or return Self by value",
            "TL-LANG-278 combined violations must stop at the same first exact error");
    });
}
}

void registerLanguageTraitValidationModule(TestRegistry& registry)
{
    registerValidTraitCase(registry);
    registerInvalidTraitCase(registry);
    registerValidCoherenceCase(registry);
    registerInvalidCoherenceCase(registry);
    registerProjectionSuccessCase(registry);
    registerProjectionFailureCase(registry);
    registerOpaqueResultCase(registry);
    registerObjectSafetyCase(registry);
}
}
