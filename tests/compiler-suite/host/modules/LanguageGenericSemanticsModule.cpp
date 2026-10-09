#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/GenericSemantics.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-009", "Host.Language", "unit", "PR",
        {"language", "generics"}, std::move(function)});
}

void addKnownDefectCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-009", "Host.Language", "unit", "PR",
        {"language", "generics"}, std::move(function), TestDisposition::KnownProductDefect});
}

void registerConstraintNormalizationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-112", [](TestContext& context) {
        ConstraintSet result{{"stale"}, "stale"};
        std::string error = "stale";
        const std::array<std::string, 0> empty{};
        context.expect(normalizeConstraints(empty, result, error) && result.atoms.empty() &&
                result.canonical.empty() && error.empty(),
            "TL-LANG-112 an empty constraint set must normalize to exact empty state");

        const std::array<std::string, 5> constraints = {
            "unmanaged", "copy", "struct", "copy", "unmanaged"};
        context.expect(normalizeConstraints(constraints, result, error) &&
                result.atoms == std::vector<std::string>{"copy", "struct", "unmanaged"} &&
                result.canonical == "4:copy;6:struct;9:unmanaged;" && error.empty(),
            "TL-LANG-112 sorting, deduplication, and length-prefixed canonical form must be exact");
    });
}

void registerConstraintFailureCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-113", [](TestContext& context) {
        struct Vector final
        {
            std::vector<std::string> constraints;
            std::string error;
            std::vector<std::string> atoms;
        };
        const std::array vectors = {
            Vector{{"copy", ""}, "constraint atom cannot be empty", {"", "copy"}},
            Vector{{"move-only", "copy"},
                "a generic parameter cannot require both copy and move-only", {"copy", "move-only"}},
            Vector{{"struct", "class"},
                "a generic parameter cannot require both class and struct", {"class", "struct"}},
            Vector{{"unmanaged", "class"},
                "a generic parameter cannot require both class and unmanaged", {"class", "unmanaged"}},
        };
        for (const Vector& vector : vectors)
        {
            ConstraintSet result;
            std::string error;
            context.expect(!normalizeConstraints(vector.constraints, result, error) &&
                    error == vector.error && result.atoms == vector.atoms && result.canonical.empty(),
                "TL-LANG-113 invalid constraints must return the first exact error and defined result state");
        }

        const std::array<std::string, 6> allConflicts = {
            "class", "struct", "unmanaged", "copy", "move-only", "copy"};
        ConstraintSet result;
        std::string error;
        context.expect(!normalizeConstraints(allConflicts, result, error) &&
                error == "a generic parameter cannot require both copy and move-only",
            "TL-LANG-113 conflict priority must remain copy/move-only before class conflicts");
    });
}

void registerConstGenericValidationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-114", [](TestContext& context) {
        struct Vector final { ConstGenericValue value; bool accepted; };
        const std::array vectors = {
            Vector{{ConstGenericKind::Integer, "i32", "42"}, true},
            Vector{{ConstGenericKind::Boolean, "bool", "false"}, true},
            Vector{{ConstGenericKind::Enumeration, "Color", "Color.Red"}, true},
            Vector{{ConstGenericKind::Structural, "Point", "{x=1,y=2}"}, true},
            Vector{{ConstGenericKind::Integer, "", "42"}, false},
            Vector{{ConstGenericKind::Integer, "i32", ""}, false},
            Vector{{ConstGenericKind::Integer, "", ""}, false},
        };
        for (const Vector& vector : vectors)
        {
            std::string error = "stale";
            const bool accepted = validateConstGeneric(vector.value, error);
            const std::string expected = vector.accepted ? std::string{} :
                "const generic requires a type and deterministic normalized value";
            context.expect(accepted == vector.accepted && error == expected,
                "TL-LANG-114 type and normalized value presence matrix must be exact");
        }
    });
}

void registerConstGenericMarkerRoundTripCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-115", [](TestContext& context) {
        struct Vector final
        {
            std::string type;
            std::string value;
            std::string marker;
        };
        const std::array vectors = {
            Vector{"i32", "0", "$nsc_const$3:i32:0"},
            Vector{"type:Map<i32,string>", "-1", "$nsc_const$20:type:Map<i32,string>:-1"},
            Vector{std::string(64, 'T'), "true", "$nsc_const$64:" + std::string(64, 'T') + ":true"},
        };
        for (const Vector& vector : vectors)
        {
            const std::string marker = constGenericArgumentMarker(vector.type, vector.value);
            std::string parsedType = "stale";
            std::string parsedValue = "stale";
            context.expect(marker == vector.marker &&
                    parseConstGenericArgumentMarker(marker, parsedType, parsedValue) &&
                    parsedType == vector.type && parsedValue == vector.value,
                "TL-LANG-115 marker bytes and parsed fields must round-trip exactly at length boundaries");
        }
    });
}

void registerConstGenericMarkerRejectionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-116", [](TestContext& context) {
        struct InvalidVector final
        {
            std::string marker;
            std::string expectedType;
            std::string expectedValue;
        };
        const std::array vectors = {
            InvalidVector{"nsc_const$3:i32:1", "", ""},
            InvalidVector{"$nsc_const$3i32:1", "", ""},
            InvalidVector{"$nsc_const$x:i32:1", "", ""},
            InvalidVector{"$nsc_const$999999999999999999999999999999999999:i32:1", "", ""},
            InvalidVector{"$nsc_const$4:i32:1", "", ""},
            InvalidVector{"$nsc_const$3:i32", "", ""},
            InvalidVector{"$nsc_const$3:i32:", "i32", ""},
        };
        for (const InvalidVector& vector : vectors)
        {
            std::string type = "stale-type";
            std::string value = "stale-value";
            context.expect(!parseConstGenericArgumentMarker(vector.marker, type, value) &&
                    type == vector.expectedType && value == vector.expectedValue,
                "TL-LANG-116 malformed markers must fail with exact defined output state");
        }

        std::string type = "stale-type";
        std::string value = "stale-value";
        context.expect(parseConstGenericArgumentMarker("$nsc_const$3:i32:1:tail", type, value) &&
                type == "i32" && value == "1:tail",
            "TL-LANG-116 a valid marker must consume the complete suffix as normalized value");
    });
}

void registerInstantiationIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-117", [](TestContext& context) {
        const std::array<std::string, 0> noTypes{};
        const std::array<ConstGenericValue, 0> noValues{};
        context.expect(genericInstantiationIdentity("Vector", noTypes, noValues) == "Vector<>",
            "TL-LANG-117 an instantiation without arguments must retain exact delimiters");

        const std::array<std::string, 2> types = {"i32", "Map<i32,string>"};
        context.expect(genericInstantiationIdentity("Pair", types, noValues) ==
                "Pair<T3:i32;T15:Map<i32,string>;>",
            "TL-LANG-117 type arguments must remain length-prefixed and source ordered");

        const std::array values = {
            ConstGenericValue{ConstGenericKind::Integer, "i32", "42"},
            ConstGenericValue{ConstGenericKind::Boolean, "bool", "true"},
            ConstGenericValue{ConstGenericKind::Enumeration, "Color", "Color.Red"},
            ConstGenericValue{ConstGenericKind::Structural, "Point", "{x=1,y=2}"},
        };
        context.expect(genericInstantiationIdentity("Mixed", types, values) ==
                "Mixed<T3:i32;T15:Map<i32,string>;C0:i32=42;C1:bool=true;"
                "C2:Color=Color.Red;C3:Point={x=1,y=2};>",
            "TL-LANG-117 type and const arguments must preserve kind, spelling, and group order");
    });
}

void registerPackExpansionCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-118", [](TestContext& context) {
        std::string error = "stale";
        const std::array<std::size_t, 0> empty{};
        const std::array<std::size_t, 1> one{4};
        const std::array<std::size_t, 3> equal{4, 4, 4};
        const std::array<std::size_t, 3> unequal{4, 3, 4};
        const std::array<std::size_t, 2> overBudgetAndUnequal{5, 4};
        context.expect(validatePackExpansion(empty, 0, error) && error.empty(),
            "TL-LANG-118 zero packs must be accepted and clear stale diagnostics");
        context.expect(validatePackExpansion(one, 4, error) && error.empty() &&
                validatePackExpansion(equal, 4, error) && error.empty(),
            "TL-LANG-118 one or many equal packs at the budget must be accepted");
        context.expect(!validatePackExpansion(unequal, 10, error) &&
                error == "zipped generic packs have different lengths",
            "TL-LANG-118 unequal zipped packs must produce the exact mismatch diagnostic");
        context.expect(!validatePackExpansion(overBudgetAndUnequal, 4, error) &&
                error == "generic pack expansion exceeds its resource budget",
            "TL-LANG-118 resource budget failure must take priority over a later length mismatch");
    });
}

void registerBidirectionalInferenceCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-119", [](TestContext& context) {
        struct Vector final
        {
            std::string_view produced;
            std::string_view expected;
            bool arbitrary;
            bool accepted;
        };
        constexpr std::array vectors = {
            Vector{"i32", "i32", false, true},
            Vector{"i32", "i64", false, false},
            Vector{"i32", "i32", true, false},
            Vector{"", "", false, true},
            Vector{"", "i32", false, false},
        };
        for (const Vector& vector : vectors)
            context.expect(bidirectionalInferenceAccepts(
                    vector.produced, vector.expected, vector.arbitrary) == vector.accepted,
                "TL-LANG-119 inference must accept only exact equality without arbitrary conversion");
    });
}

void registerAliasIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-120", [](TestContext& context) {
        context.expect(aliasIdentity(AliasKind::Transparent, "Core", "Index", "u64") == "u64" &&
                aliasIdentity(AliasKind::Transparent, "", "", "") == "",
            "TL-LANG-120 transparent aliases must collapse exactly to their representation");
        context.expect(aliasIdentity(AliasKind::Opaque, "Core", "Index", "u64") ==
                "opaque:Core::Index" &&
                aliasIdentity(AliasKind::Opaque, "", "", "ignored") == "opaque:::",
            "TL-LANG-120 opaque aliases must preserve the exact nominal identity contract");
    });
}

void registerSpecializationDominanceCase(TestRegistry& registry)
{
    addKnownDefectCase(registry, "TL-LANG-121", [](TestContext& context) {
        const std::array<std::string, 2> equalCandidate{"copy", "struct"};
        const std::array<std::string, 2> equalFallback{"copy", "struct"};
        const std::array<std::string, 1> subset{"copy"};
        const std::array<std::string, 2> superset{"copy", "struct"};
        const std::array<std::string, 2> incomparable{"copy", "unmanaged"};
        context.expect(!specializationStrictlyDominates(equalCandidate, equalFallback) &&
                !specializationStrictlyDominates(subset, superset) &&
                specializationStrictlyDominates(superset, subset) &&
                !specializationStrictlyDominates(superset, incomparable),
            "TL-LANG-121 equal, subset, strict superset, and incomparable relations must be exact");

        const std::array<std::string, 3> duplicateCandidate{"copy", "copy", "struct"};
        const std::array<std::string, 2> duplicateFallback{"copy", "copy"};
        context.expect(specializationStrictlyDominates(duplicateCandidate, subset) &&
                specializationStrictlyDominates(superset, duplicateFallback) &&
                !specializationStrictlyDominates(duplicateFallback, subset),
            "TL-LANG-121 duplicate spellings must not alter strict normalized-set dominance");
    });
}

void registerInstantiationBudgetCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-122", [](TestContext& context) {
        InstantiationBudget budget(2, 3);
        std::string error = "stale";
        context.expect(budget.maximumDepth() == 2 && budget.maximumCount() == 3 &&
                budget.count() == 0 && budget.maximumObservedDepth() == 0 && budget.activeStack().empty(),
            "TL-LANG-122 a new budget must expose its configured limits and zero state");
        context.expect(budget.enter("A", error) && error.empty() && budget.count() == 1 &&
                budget.maximumObservedDepth() == 1 && budget.activeStack().size() == 1 &&
                budget.activeStack().front() == "A",
            "TL-LANG-122 first entry must update count, depth, and stack exactly");
        context.expect(budget.enter("B", error) && error.empty() && budget.count() == 2 &&
                budget.maximumObservedDepth() == 2 && budget.activeStack().size() == 2,
            "TL-LANG-122 nested entry must update monotonic counters exactly");
        context.expect(!budget.enter("C", error) &&
                error == "generic instantiation depth limit (2) exceeded; causal chain: A -> B -> C" &&
                budget.count() == 2 && budget.activeStack().size() == 2,
            "TL-LANG-122 depth failure must be exact and transactional");
        budget.leave();
        context.expect(!budget.enter("A", error) &&
                error == "recursive generic instantiation; causal chain: A -> A" &&
                budget.count() == 2 && budget.activeStack().size() == 1,
            "TL-LANG-122 recursion failure must be exact and transactional");
        context.expect(budget.enter("C", error) && budget.count() == 3 &&
                budget.maximumObservedDepth() == 2 && budget.activeStack().size() == 2,
            "TL-LANG-122 a valid entry after failures must consume the next count only once");
        budget.leave();
        budget.leave();
        context.expect(!budget.enter("D", error) &&
                error == "generic instantiation count limit (3) exceeded; causal chain: D" &&
                budget.count() == 3 && budget.activeStack().empty(),
            "TL-LANG-122 count failure must preserve the empty active stack and monotonic count");
    });
}

void registerInstantiationBudgetBoundaryCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-123", [](TestContext& context) {
        InstantiationBudget empty(1, 1);
        empty.leave();
        empty.leave();
        context.expect(empty.count() == 0 && empty.maximumObservedDepth() == 0 &&
                empty.activeStack().empty(),
            "TL-LANG-123 leave on an empty budget must not underflow or mutate counters");

        InstantiationBudget budget(13, 100);
        std::string error;
        bool entered = true;
        for (std::size_t index = 0; index < 13; ++index)
            entered = budget.enter("F" + std::to_string(index), error) && entered;
        context.expect(entered && error.empty() && budget.count() == 13 &&
                budget.maximumObservedDepth() == 13 && budget.activeStack().size() == 13,
            "TL-LANG-123 thirteen distinct frames must reach the configured boundary exactly");
        context.expect(!budget.enter("F13", error) &&
                error == "generic instantiation depth limit (13) exceeded; causal chain: ... -> "
                    "F1 -> F2 -> F3 -> F4 -> F5 -> F6 -> F7 -> F8 -> F9 -> F10 -> F11 -> F12 -> F13" &&
                budget.count() == 13 && budget.activeStack().size() == 13,
            "TL-LANG-123 causal reports beyond twelve active frames must truncate exactly");
        for (std::size_t index = 0; index < 14; ++index) budget.leave();
        context.expect(budget.activeStack().empty() && budget.count() == 13 &&
                budget.maximumObservedDepth() == 13,
            "TL-LANG-123 extra leave operations must preserve monotonic counters without underflow");
    });
}
}

void registerLanguageGenericSemanticsModule(TestRegistry& registry)
{
    registerConstraintNormalizationCase(registry);
    registerConstraintFailureCase(registry);
    registerConstGenericValidationCase(registry);
    registerConstGenericMarkerRoundTripCase(registry);
    registerConstGenericMarkerRejectionCase(registry);
    registerInstantiationIdentityCase(registry);
    registerPackExpansionCase(registry);
    registerBidirectionalInferenceCase(registry);
    registerAliasIdentityCase(registry);
    registerSpecializationDominanceCase(registry);
    registerInstantiationBudgetCase(registry);
    registerInstantiationBudgetBoundaryCase(registry);
}
}
