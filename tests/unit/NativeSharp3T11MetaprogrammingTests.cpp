#include "nativesharp/Language/MetaprogrammingSemantics.h"

#include "../support/TestContext.h"

#include <array>

int main()
{
    nativesharp::tests::TestContext tests;
    using namespace nativesharp::language;
    const std::array<IntrospectionMember, 3> members = {{{"Private", "int", false, 2},
        {"First", "int", true, 0}, {"Second", "int", true, 1}}};
    const auto external = visibleIntrospectionMembers(members, false);
    tests.expect(external.size() == 2 && external[0].name == "First" && external[1].name == "Second" &&
            !introspectionRequiresRuntimeMetadata(),
        "typed introspection is stable, privacy-aware, and compile-time only");

    std::string error;
    const std::array<AttributeUse, 2> duplicate = {{{"Demo.Tag", "type", false},
        {"Demo.Tag", "type", false}}};
    tests.expect(!validateAttributes(duplicate, error),
        "non-repeatable user attributes reject duplicate use on the same target");
    tests.expect(deriveDependencyKey("Demo.Equality", std::array<std::string, 2>{"Y:int", "X:int"}) ==
            deriveDependencyKey("Demo.Equality", std::array<std::string, 2>{"X:int", "Y:int"}),
        "derive dependencies have deterministic order-independent cache identity");

    tests.expect(sameBinding({"temp", 4}, {"temp", 4}) && !sameBinding({"temp", 4}, {"temp", 5}),
        "macro hygiene distinguishes identical spelling from different expansion scopes");
    tests.expect(capabilityAllowedInHermeticProfile(ComptimeCapability::ReadDeclaredFile) &&
            !capabilityAllowedInHermeticProfile(ComptimeCapability::Clock) &&
            !capabilityAllowedInHermeticProfile(ComptimeCapability::Ffi),
        "hermetic comptime exposes declared inputs but no clock or FFI");
    tests.expect(comptimeInputIdentity(
            std::array<ComptimeInput, 2>{{{"B", "22"}, {"A", "11"}}}) ==
            "comptime-input-v1|A=11;B=22;",
        "every declared external input participates in a stable incremental identity");
    tests.expect(validPackageDiagnosticCode("PKG42", "PKG42-001") &&
            !validPackageDiagnosticCode("PKG42", "BAD42"),
        "package diagnostics cannot collide with compiler-owned NS3xxx codes");

    ComptimeBudget budget(100, 1024, 8);
    tests.expect(budget.consume(50, 256, 4, error) && !budget.consume(51, 1, 1, error),
        "comptime step, memory, and recursion budgets fail deterministically");
    return tests.result();
}
