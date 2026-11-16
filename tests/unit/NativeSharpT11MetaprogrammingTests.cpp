#include "nativesharp/Language/MetaprogrammingSemantics.h"
#include "nativesharp/Language/MetaprogrammingGeneration.h"

#include "../support/TestContext.h"

#include <array>
#include <string>
#include <vector>

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

    MetaprogrammingAstBuilder builder;
    const GeneratedDeclaration field = builder.buildField(
        "Cache", "int", 3, "Main.ns", false, false, true);
    tests.expect(field.valid() && field.kind == GeneratedDeclarationKind::Member &&
            field.name.spelling == "Cache" && field.name.scopeId == 3 &&
            field.isReadonly && field.originPath == "Main.ns",
        "generated field carries origin, hygiene scope, modifiers, and a stable ID");

    const GeneratedDeclaration reserved = builder.buildField("__nsc_secret", "int", 1, "Main.ns");
    tests.expect(!reserved.valid() && reserved.error.find("reserved") != std::string::npos,
        "generated declarations cannot forge the reserved __nsc_ namespace");

    const GeneratedDeclaration empty = builder.buildMethod("", "int", 1, "Main.ns");
    tests.expect(!empty.valid(), "generated declarations reject empty names");

    const GeneratedDeclaration impl = builder.buildImpl("IValue", "Pair", 5, "Main.ns");
    tests.expect(impl.valid() && impl.kind == GeneratedDeclarationKind::Impl &&
            impl.name.spelling == "IValue" && impl.typeKey == "Pair",
        "generated impl carries its trait and target type");

    const GeneratedDeclaration helper = builder.buildHelper("Aux", "int", 2, "Main.ns", true);
    tests.expect(helper.valid() && helper.kind == GeneratedDeclarationKind::Helper && helper.isPublic,
        "generated helper carries its type and accessibility");

    const std::vector<HygienicName> existing = {{"Cache", 3}, {"Other", 3}};
    tests.expect(builder.wouldCollide(existing, field) && !builder.wouldCollide(existing, helper),
        "generated declarations collide with an existing same-scope binding only when spelled identically");

    tests.expect(generatedMemberIdentity("Demo.Pair", "Clone") == "Demo.Pair::Clone" &&
            sameGeneratedIdentity(generatedMemberIdentity("Demo.Pair", "Clone"),
                generatedMemberIdentity("Demo.Pair", "Clone")) &&
            !sameGeneratedIdentity("Demo.Pair::Clone", "Demo.Pair::ToString"),
        "generated members unify to a deterministic fully-qualified identity for partial/extension merge");

    const std::array<ComptimeCapability, 5> allCaps = {
        ComptimeCapability::ReadDeclaredFile, ComptimeCapability::ReadDeclaredEnvironment,
        ComptimeCapability::Clock, ComptimeCapability::Random, ComptimeCapability::Ffi};
    const std::array<ComptimeInput, 2> inputs = {{{"A", "11"}, {"B", "22"}}};
    const std::string key = comptimeCacheKey(allCaps, inputs, "src-digest");
    tests.expect(key == comptimeCacheKey(allCaps, inputs, "src-digest") &&
            key.find("comptime-cache-v1|caps=0,1,") == 0 &&
            key.find("source=src-digest") != std::string::npos,
        "the cache key is deterministic and excludes non-hermetic clock/network/global state");
    const std::array<ComptimeCapability, 2> hermeticCaps = {
        ComptimeCapability::ReadDeclaredFile, ComptimeCapability::ReadDeclaredEnvironment};
    tests.expect(comptimeCacheKey(hermeticCaps, inputs, "s") == comptimeCacheKey(allCaps, inputs, "s"),
        "non-hermetic capabilities do not change the hermetic cache key");
    return tests.result();
}
