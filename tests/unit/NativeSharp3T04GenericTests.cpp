#include "nativesharp/Language/GenericSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    std::string error;
    ConstraintSet constraints;
    const std::array<std::string, 3> unordered = {"trait:Hash", "copy", "trait:Hash"};
    tests.expect(normalizeConstraints(unordered, constraints, error) &&
            constraints.canonical == "4:copy;10:trait:Hash;",
        "constraints are deduplicated and canonicalized for caches and metadata");
    const std::array<std::string, 2> contradiction = {"copy", "move-only"};
    tests.expect(!normalizeConstraints(contradiction, constraints, error),
        "contradictory ownership constraints are rejected");

    const ConstGenericValue four{ConstGenericKind::Integer, "builtin:int", "4"};
    tests.expect(validateConstGeneric(four, error) && genericInstantiationIdentity(
            "Vector", std::array<std::string, 1>{"builtin:float"},
            std::array<ConstGenericValue, 1>{four}) ==
            "Vector<T13:builtin:float;C0:builtin:int=4;>",
        "normalized const values participate in generic identity");
    const std::array<std::size_t, 2> zipped = {3, 3};
    const std::array<std::size_t, 2> mismatched = {3, 2};
    tests.expect(validatePackExpansion(zipped, 8, error) &&
            !validatePackExpansion(mismatched, 8, error),
        "type/value packs support bounded zip expansion and reject length ambiguity");
    tests.expect(bidirectionalInferenceAccepts("tuple<int,int>", "tuple<int,int>", false) &&
            !bidirectionalInferenceAccepts("int", "long", true),
        "bidirectional inference never guesses through arbitrary conversions");
    tests.expect(aliasIdentity(AliasKind::Transparent, "M", "Meters", "builtin:int") == "builtin:int" &&
            aliasIdentity(AliasKind::Opaque, "M", "Meters", "builtin:int") == "opaque:M::Meters",
        "transparent and opaque aliases have intentionally distinct identities");
    tests.expect(specializationStrictlyDominates(
            std::array<std::string, 2>{"copy", "trait:Hash"},
            std::array<std::string, 1>{"copy"}) &&
            !specializationStrictlyDominates(
                std::array<std::string, 1>{"trait:Hash"},
                std::array<std::string, 1>{"copy"}),
        "specialization selection requires a demonstrable strict subset ordering");
    InstantiationBudget budget(2, 3);
    tests.expect(budget.enter("A<int>", error) && budget.enter("B<int>", error) &&
            !budget.enter("C<int>", error),
        "generic instantiation depth has a deterministic budget");
    budget.leave();
    tests.expect(!budget.enter("A<int>", error),
        "direct or indirect recursive instantiation reports a compact stable cause");
    return tests.result();
}
