#include "ParserCallableTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using namespace parser_callable_support;
using nativesharp::ast::FfiUnwindSyntaxKind;
using nativesharp::ast::FloatingPointModeSyntaxKind;
using nativesharp::ast::OwnershipQualifierSyntaxKind;
using nativesharp::ast::TypeModifierSyntaxKind;

void testParametersAndDefaults(TestContext& context)
{
    DeclarationFixture fixture("class C { void M(int plain, long optional = 9) { } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->children().size() == 2 &&
            node->children()[0]->kind() == AstNodeKind::ParameterDeclaration &&
            node->children()[0]->text() == "plain" && node->children()[0]->first() == nullptr &&
            node->children()[1]->text() == "optional" && node->children()[1]->first() != nullptr &&
            node->children()[1]->first()->integerValue() == 9,
        "TP-PAR-283 parameters with and without defaults must retain exact AST shape");
}

void testReferenceOwnershipLifetime(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { void M(ref [a] int alias, owned Item value) { } }", {"Item"});
    AstNode* const node = member(fixture);
    AstNode* const alias = node != nullptr && node->children().size() > 0 ? node->children()[0] : nullptr;
    AstNode* const value = node != nullptr && node->children().size() > 1 ? node->children()[1] : nullptr;
    bool aliasPointer = false;
    if (alias != nullptr && alias->typeSyntax() != nullptr)
    {
        for (const auto& modifier : alias->typeSyntax()->modifiers())
            aliasPointer = aliasPointer || modifier.kind == TypeModifierSyntaxKind::Pointer;
    }
    context.expect(alias != nullptr && alias->auxiliaryText() == "pass:ref" &&
            alias->lifetimeName() == "a" && aliasPointer && value != nullptr &&
            value->typeSyntax() != nullptr &&
            value->typeSyntax()->ownershipQualifier() == OwnershipQualifierSyntaxKind::Owned,
        "TP-PAR-284 ref, ownership, and lifetime annotations must retain independent exact fields");
}

void testVariadicAndPacks(TestContext& context)
{
    DeclarationFixture fixture(
        "extern C { int printf(string format, ...); } "
        "class C { void Gather(params ReadOnlySpan<int> values) { } }", {"ReadOnlySpan"});
    AstNode* const externBlock = declaration(fixture);
    AstNode* const function = externBlock != nullptr && !externBlock->children().empty()
        ? externBlock->children()[0] : nullptr;
    AstNode* const type = declaration(fixture, 1);
    AstNode* const method = type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
    AstNode* const pack = method != nullptr && !method->children().empty() ? method->children()[0] : nullptr;
    context.expect(function != nullptr && function->isVariadic() && function->children().size() == 1 &&
            pack != nullptr && pack->auxiliaryText() == "params" &&
            method->children().size() == 1,
        "TP-PAR-285 C variadic marker and final params pack must retain exact positions");
}

void testDefaultBeforeRequired(TestContext& context)
{
    // ProjectMemberModel owns ordering diagnostics. Parser must publish both parameter nodes
    // losslessly and must not duplicate the semantic diagnostic at this boundary.
    DeclarationFixture fixture("class C { void M(int first = 1, int required) { } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->children().size() == 2 &&
            node->children()[0]->first() != nullptr && node->children()[1]->first() == nullptr &&
            fixture.diagnostics().empty(),
        "TP-PAR-286 parser must preserve default-before-required AST for the semantic diagnostic owner");
}

void testMissingParameterParts(TestContext& context)
{
    DeclarationFixture missingName("class C { void M(int) { } int kept; }");
    DeclarationFixture missingType("class C { void M(, int kept) { } }");
    context.expect(!missingName.diagnostics().empty() && !missingType.diagnostics().empty() &&
            declaration(missingName) != nullptr && declaration(missingType) != nullptr,
        "TP-PAR-287 missing parameter name or type must diagnose and terminate parsing");
}

void testFloatingPointModes(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { double S() fp(strict) { return 0.0; } "
        "double P() fp(precise) { return 0.0; } double F() fp(fast) { return 0.0; } }");
    AstNode* const type = declaration(fixture);
    context.expect(type != nullptr && type->children().size() == 3 &&
            type->children()[0]->floatingPointMode() == FloatingPointModeSyntaxKind::Strict &&
            type->children()[1]->floatingPointMode() == FloatingPointModeSyntaxKind::Precise &&
            type->children()[2]->floatingPointMode() == FloatingPointModeSyntaxKind::Fast,
        "TP-PAR-288 strict, precise, and fast must map to exact floating-point modes");
}

void testInvalidFloatingPointModes(TestContext& context)
{
    DeclarationFixture invalid("class C { double M() fp(random) { return 0.0; } }");
    DeclarationFixture duplicate("class C { double M() fp(strict) fp(fast) { return 0.0; } }");
    context.expect(diagnosticContains(invalid, "floating-point mode must be strict, precise, or fast") &&
            !duplicate.diagnostics().empty(),
        "TP-PAR-289 invalid or duplicate floating-point mode clauses must diagnose");
}

void testTypedThrows(TestContext& context)
{
    DeclarationFixture fixture("class C { int M() throws Error { return 0; } }", {"Error"});
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->throwsTypeSyntax() != nullptr &&
            node->throwsTypeSyntax()->baseName() == "Error",
        "TP-PAR-290 typed throws must retain its exact error TypeSyntax");
}

void testBareThrowsCurrentContract(TestContext& context)
{
    DeclarationFixture fixture("class C { int M() throws { return 0; } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->throwsTypeSyntax() == nullptr &&
            fixture.diagnosticCount(DiagnosticId::ExpectedType) == 1,
        "TP-PAR-291 bare throws must retain the current explicit-type rejection contract");
}

void testMalformedThrows(TestContext& context)
{
    DeclarationFixture duplicate(
        "class C { int M() throws Error throws Other { return 0; } }", {"Error", "Other"});
    DeclarationFixture malformed("class C { int M() throws ) { return 0; } }");
    context.expect(!duplicate.diagnostics().empty() &&
            malformed.diagnosticCount(DiagnosticId::ExpectedType) == 1,
        "TP-PAR-292 duplicate or malformed throws clauses must diagnose");
}

void testUnwindPolicies(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { int F() unwind(forbidden) { return 0; } "
        "int A() unwind(abort) { return 0; } }");
    AstNode* const type = declaration(fixture);
    context.expect(type != nullptr && type->children().size() == 2 &&
            type->children()[0]->ffiUnwindPolicy() == FfiUnwindSyntaxKind::Forbidden &&
            type->children()[1]->ffiUnwindPolicy() == FfiUnwindSyntaxKind::Abort,
        "TP-PAR-293 current forbidden and abort spellings must map to exact unwind policies");
}

void testInvalidUnwindPolicies(TestContext& context)
{
    DeclarationFixture invalid("class C { int M() unwind(allow) { return 0; } }");
    DeclarationFixture duplicate(
        "class C { int M() unwind(abort) unwind(forbidden) { return 0; } }");
    context.expect(diagnosticContains(invalid, "FFI unwind policy must be forbidden or abort") &&
            !duplicate.diagnostics().empty(),
        "TP-PAR-294 invalid or duplicate unwind policies must diagnose");
}

void testCombinedCallableClauses(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { double M() throws Error fp(fast) unwind(abort) { return 0.0; } }", {"Error"});
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->throwsTypeSyntax() != nullptr &&
            node->throwsTypeSyntax()->baseName() == "Error" &&
            node->floatingPointMode() == FloatingPointModeSyntaxKind::Fast &&
            node->ffiUnwindPolicy() == FfiUnwindSyntaxKind::Abort,
        "TP-PAR-295 throws, floating-point mode, and unwind policy must remain independent fields");
}

void testAlternateClauseOrder(TestContext& context)
{
    DeclarationFixture canonical(
        "class C { int A() throws Error fp(strict) unwind(forbidden) { return 0; } }", {"Error"});
    DeclarationFixture alternate(
        "class C { int B() fp(strict) throws Error unwind(forbidden) { return 0; } }", {"Error"});
    AstNode* const canonicalNode = member(canonical);
    AstNode* const alternateNode = member(alternate);
    context.expect(canonicalNode != nullptr && canonical.diagnostics().empty() &&
            alternateNode != nullptr && alternateNode->floatingPointMode() == FloatingPointModeSyntaxKind::Strict &&
            alternateNode->throwsTypeSyntax() == nullptr && !alternate.diagnostics().empty(),
        "TP-PAR-296 canonical clause order must pass and unsupported alternate order must diagnose exactly");
}
}

void registerParserCallableClauseModule(TestRegistry& registry)
{
    addCallableCase(registry, "TP-PAR-283", "CP-PAR-027", testParametersAndDefaults);
    addCallableCase(registry, "TP-PAR-284", "CP-PAR-027", testReferenceOwnershipLifetime);
    addCallableCase(registry, "TP-PAR-285", "CP-PAR-027", testVariadicAndPacks);
    addCallableCase(registry, "TP-PAR-286", "CP-PAR-027", testDefaultBeforeRequired);
    addCallableCase(registry, "TP-PAR-287", "CP-PAR-027", testMissingParameterParts);
    addCallableCase(registry, "TP-PAR-288", "CP-PAR-027", testFloatingPointModes);
    addCallableCase(registry, "TP-PAR-289", "CP-PAR-027", testInvalidFloatingPointModes);
    addCallableCase(registry, "TP-PAR-290", "CP-PAR-027", testTypedThrows);
    addCallableCase(registry, "TP-PAR-291", "CP-PAR-027", testBareThrowsCurrentContract);
    addCallableCase(registry, "TP-PAR-292", "CP-PAR-027", testMalformedThrows);
    addCallableCase(registry, "TP-PAR-293", "CP-PAR-027", testUnwindPolicies);
    addCallableCase(registry, "TP-PAR-294", "CP-PAR-027", testInvalidUnwindPolicies);
    addCallableCase(registry, "TP-PAR-295", "CP-PAR-027", testCombinedCallableClauses);
    addCallableCase(registry, "TP-PAR-296", "CP-PAR-027", testAlternateClauseOrder);
}
}
