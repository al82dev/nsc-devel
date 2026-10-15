#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/MetaprogrammingSemantics.h"
#include "nativesharp/Language/MetaprogrammingGeneration.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-014", "Host.Language", "unit", "PR",
        {"language", "metaprogramming"}, std::move(function)});
}

bool sameAttribute(const AttributeUse& left, const AttributeUse& right)
{
    return left.identity == right.identity && left.target == right.target &&
        left.repeatable == right.repeatable && left.retention == right.retention;
}

void registerVisibleMembersCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-172", [](TestContext& context) {
        const std::array<IntrospectionMember, 5> members = {{
            {"LatePublic", "string", true, 8},
            {"EarlyPrivate", "bool", false, 1},
            {"MiddlePublic", "double", true, 4},
            {"EarlyPublic", "int", true, 2},
            {"LatePrivate", "byte", false, 6},
        }};
        const auto external = visibleIntrospectionMembers(members, false);
        context.expect(external.size() == 3 && external[0].name == "EarlyPublic" &&
                external[1].name == "MiddlePublic" && external[2].name == "LatePublic",
            "TL-LANG-172 external introspection must filter private members and sort by source ordinal");
        const auto internal = visibleIntrospectionMembers(members, true);
        context.expect(internal.size() == 5 && internal[0].name == "EarlyPrivate" &&
                internal[1].name == "EarlyPublic" && internal[2].name == "MiddlePublic" &&
                internal[3].name == "LatePrivate" && internal[4].name == "LatePublic",
            "TL-LANG-172 same-module introspection must retain all members in source order");
        context.expect(members[0].name == "LatePublic" && members[4].name == "LatePrivate",
            "TL-LANG-172 filtering and sorting must not mutate the caller's source sequence");
    });
}

void registerRuntimeMetadataCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-173", [](TestContext& context) {
        context.expect(!introspectionRequiresRuntimeMetadata(),
            "TL-LANG-173 introspection must remain compile-time only");
        context.expect(!introspectionRequiresRuntimeMetadata(),
            "TL-LANG-173 repeated metadata queries must remain false");
    });
}

void registerValidAttributesCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-174", [](TestContext& context) {
        std::vector<AttributeUse> attributes = {
            {"Demo.Tag", "type", false, AttributeRetention::CompileTime},
            {"Demo.Note", "method", true, AttributeRetention::NativeRuntimeSection},
            {"Demo.Note", "method", true, AttributeRetention::NativeRuntimeSection},
            {"Demo.Tag", "field", false, AttributeRetention::CompileTime},
        };
        const auto original = attributes;
        std::string error = "stale";
        context.expect(validateAttributes(attributes, error) && error.empty(),
            "TL-LANG-174 valid and repeatable attributes must be accepted and clear stale errors");
        bool unchanged = attributes.size() == original.size();
        for (std::size_t index = 0; unchanged && index < attributes.size(); ++index)
            unchanged = sameAttribute(attributes[index], original[index]);
        context.expect(unchanged,
            "TL-LANG-174 validation must preserve attribute values and caller order");
    });
}

void registerInvalidAttributesCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-175", [](TestContext& context) {
        const std::array<AttributeUse, 1> emptyIdentity = {{{"", "type", false}}};
        const std::array<AttributeUse, 1> emptyTarget = {{{"Demo.Tag", "", false}}};
        const std::array<AttributeUse, 3> duplicate = {{{"Demo.Tag", "type", false},
            {"Demo.Note", "type", false}, {"Demo.Tag", "type", false}}};
        std::string error;
        context.expect(!validateAttributes(emptyIdentity, error) &&
                error == "attribute requires identity and target",
            "TL-LANG-175 an empty identity must report the exact first validation error");
        context.expect(!validateAttributes(emptyTarget, error) &&
                error == "attribute requires identity and target",
            "TL-LANG-175 an empty target must report the exact first validation error");
        context.expect(!validateAttributes(duplicate, error) &&
                error == "non-repeatable attribute appears more than once",
            "TL-LANG-175 a repeated non-repeatable identity/target pair must report the exact error");
    });
}

void registerDependencyKeyCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-176", [](TestContext& context) {
        const std::array<std::string, 4> fields = {"Y:int", "X:int", "X:int", "Long:string"};
        const std::string expected =
            "derive:Demo.Equality|11:Long:string;5:X:int;5:X:int;5:Y:int;";
        context.expect(deriveDependencyKey("Demo.Equality", fields) == expected,
            "TL-LANG-176 fields must be sorted with duplicates retained and length-prefixed exactly");
        const std::array<std::string, 2> boundaryFields = {"", "A"};
        context.expect(deriveDependencyKey("", boundaryFields) == "derive:|0:;1:A;",
            "TL-LANG-176 empty generator and field identities must retain their exact encoding");
    });
}

void registerHygienicBindingCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-177", [](TestContext& context) {
        context.expect(sameBinding({"value", 7}, {"value", 7}),
            "TL-LANG-177 identical spelling and scope must identify the same binding");
        context.expect(!sameBinding({"value", 7}, {"value", 8}),
            "TL-LANG-177 equal spelling in different scopes must identify different bindings");
        context.expect(!sameBinding({"value", 7}, {"other", 7}),
            "TL-LANG-177 different spelling in the same scope must identify different bindings");
        context.expect(sameBinding({"", 0}, {"", 0}),
            "TL-LANG-177 empty spelling and zero scope must remain a comparable boundary value");
    });
}

void registerHermeticCapabilityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-178", [](TestContext& context) {
        constexpr std::array capabilities = {
            ComptimeCapability::ReadDeclaredFile,
            ComptimeCapability::ReadDeclaredEnvironment,
            ComptimeCapability::Clock,
            ComptimeCapability::Random,
            ComptimeCapability::Ffi,
        };
        constexpr std::array expected = {true, true, false, false, false};
        for (std::size_t index = 0; index < capabilities.size(); ++index)
        {
            context.expect(capabilityAllowedInHermeticProfile(capabilities[index]) == expected[index],
                "TL-LANG-178 the complete declared capability matrix must remain exact");
        }
    });
}

void registerComptimeIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-179", [](TestContext& context) {
        const std::array<ComptimeInput, 4> inputs = {{{"B", "22"}, {"A", "11"},
            {"C", "payload:with=markers;"}, {"A", "11"}}};
        const std::string expected =
            "comptime-input-v1|A=11;A=11;B=22;C=payload:with=markers;;";
        context.expect(comptimeInputIdentity(inputs) == expected,
            "TL-LANG-179 inputs must sort by identity while preserving duplicate entries and payload bytes");
        context.expect(comptimeInputIdentity({}) == "comptime-input-v1|",
            "TL-LANG-179 an empty input set must retain the versioned identity prefix");
    });
}

void registerDiagnosticCodeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-180", [](TestContext& context) {
        struct Sample final
        {
            std::string package;
            std::string code;
            bool expected;
        };
        const std::array samples = {
            Sample{"PKG42", "PKG42-001", true},
            Sample{"PKG42", "PKG42", true},
            Sample{"PKG42", "PKG42OTHER", true},
            Sample{"PKG42", "BAD42", false},
            Sample{"PKG42", "NS3001", false},
            Sample{"NS", "NS3001", false},
            Sample{"NS3", "NS3001", false},
            Sample{"", "PKG42-001", false},
            Sample{"PKG42", "", false},
        };
        for (const Sample& sample : samples)
        {
            context.expect(validPackageDiagnosticCode(sample.package, sample.code) == sample.expected,
                "TL-LANG-180 package diagnostic acceptance must match the complete declared matrix");
        }
    });
}

void registerComptimeBudgetCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-181", [](TestContext& context) {
        constexpr const char* expectedError =
            "compile-time evaluation exceeded its deterministic resource budget";
        std::string error = "stale";

        ComptimeBudget cumulative(10, 20, 3);
        context.expect(cumulative.consume(4, 5, 2, error) && error.empty(),
            "TL-LANG-181 a below-budget consume must succeed and clear stale errors");
        context.expect(cumulative.consume(6, 15, 3, error) && error.empty(),
            "TL-LANG-181 remaining step and byte counters must be consumable exactly");
        context.expect(cumulative.consume(0, 0, 3, error),
            "TL-LANG-181 depth is a per-call ceiling and must not be decremented");
        context.expect(!cumulative.consume(1, 0, 0, error) && error == expectedError,
            "TL-LANG-181 a step request above the remaining counter must fail exactly");
        context.expect(!cumulative.consume(0, 1, 0, error) && error == expectedError,
            "TL-LANG-181 a byte request above the remaining counter must fail exactly");

        ComptimeBudget steps(10, 20, 3);
        context.expect(!steps.consume(11, 0, 0, error) && error == expectedError &&
                steps.consume(10, 20, 3, error),
            "TL-LANG-181 a rejected step overflow must not discount any resource counter");
        ComptimeBudget bytes(10, 20, 3);
        context.expect(!bytes.consume(0, 21, 0, error) && error == expectedError &&
                bytes.consume(10, 20, 3, error),
            "TL-LANG-181 a rejected byte overflow must not discount any resource counter");
        ComptimeBudget depth(10, 20, 3);
        context.expect(!depth.consume(0, 0, 4, error) && error == expectedError &&
                depth.consume(10, 20, 3, error),
            "TL-LANG-181 a rejected depth overflow must not discount step or byte counters");
    });
}
}

void registerGeneratedDeclarationCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-500", [](TestContext& context) {
        const MetaprogrammingAstBuilder builder;
        const GeneratedDeclaration field = builder.buildField(
            "Cache", "int", 3, "Main.ns", false, false, true);
        context.expect(field.valid() && field.kind == GeneratedDeclarationKind::Member &&
                field.name.spelling == "Cache" && field.name.scopeId == 3 &&
                field.isReadonly && field.originPath == "Main.ns",
            "TL-LANG-500 a generated field must carry origin, hygiene scope, modifiers, and a stable ID");

        const GeneratedDeclaration reserved = builder.buildField("__nsc_secret", "int", 1, "Main.ns");
        context.expect(!reserved.valid() && reserved.error.find("reserved") != std::string::npos,
            "TL-LANG-500 generated declarations must not forge the reserved __nsc_ namespace");

        const GeneratedDeclaration empty = builder.buildMethod("", "int", 1, "Main.ns");
        context.expect(!empty.valid(),
            "TL-LANG-500 generated declarations must reject an empty name");

        const GeneratedDeclaration impl = builder.buildImpl("IValue", "Pair", 5, "Main.ns");
        context.expect(impl.valid() && impl.kind == GeneratedDeclarationKind::Impl &&
                impl.name.spelling == "IValue" && impl.typeKey == "Pair",
            "TL-LANG-500 a generated impl must carry its trait and target type");

        const GeneratedDeclaration helper = builder.buildHelper("Aux", "int", 2, "Main.ns", true);
        context.expect(helper.valid() && helper.kind == GeneratedDeclarationKind::Helper &&
                helper.isPublic,
            "TL-LANG-500 a generated helper must carry its type and accessibility");

        const std::vector<HygienicName> existing = {{"Cache", 3}, {"Other", 3}};
        context.expect(builder.wouldCollide(existing, field) &&
                !builder.wouldCollide(existing, helper),
            "TL-LANG-500 the same-scope binder gate must collide only on identical spelling");
    });
}

void registerGeneratedIdentityCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-501", [](TestContext& context) {
        context.expect(generatedMemberIdentity("Demo.Pair", "Clone") == "Demo.Pair::Clone",
            "TL-LANG-501 a generated member must unify to the source fully-qualified identity form");
        context.expect(sameGeneratedIdentity(
                generatedMemberIdentity("Demo.Pair", "Clone"),
                generatedMemberIdentity("Demo.Pair", "Clone")),
            "TL-LANG-501 identical generated identities must compare equal");
        context.expect(!sameGeneratedIdentity("Demo.Pair::Clone", "Demo.Pair::ToString"),
            "TL-LANG-501 distinct generated identities must not compare equal");
        context.expect(generatedMemberIdentity("Demo.Pair", "Clone") ==
                generatedMemberIdentity("Demo.Pair", "Clone"),
            "TL-LANG-501 generated identity is deterministic for cycle resolution");
        context.expect(generatedMemberIdentity("", "") == "::",
            "TL-LANG-501 empty enclosing and member identities retain the exact separator form");
    });
}

void registerLanguageMetaprogrammingModule(TestRegistry& registry)
{
    registerVisibleMembersCase(registry);
    registerRuntimeMetadataCase(registry);
    registerValidAttributesCase(registry);
    registerInvalidAttributesCase(registry);
    registerDependencyKeyCase(registry);
    registerHygienicBindingCase(registry);
    registerHermeticCapabilityCase(registry);
    registerComptimeIdentityCase(registry);
    registerDiagnosticCodeCase(registry);
    registerComptimeBudgetCase(registry);
    registerGeneratedDeclarationCase(registry);
    registerGeneratedIdentityCase(registry);
}
}
