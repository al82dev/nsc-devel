#include "ParserFoundationTestSupport.h"

#include "nativesharp/AST/AstNodeKind.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;

// Returns the first parsed top-level declaration or null after complete rejection. Complexity is O(1).
AstNode* firstDeclaration(ParserFixture& fixture)
{
    AstNode* root = fixture.parse();
    return root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
}

// Registers one delegate parser case. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CP-PAR-015", "Host.Parser", "component", "PR",
        {"parser", "delegate"}, std::move(function)});
}

// Verifies the minimal delegate identity, return type, name, and complete range. Complexity is O(n).
void testMinimalDelegate(TestContext& context)
{
    ParserFixture fixture("delegate int Transform();");
    AstNode* node = firstDeclaration(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::DelegateDeclaration &&
            node->text() == "Transform" && node->typeSyntax() != nullptr &&
            node->typeSyntax()->baseTokenKind() == TokenKind::KwInt,
        "TP-PAR-144 minimal delegate must retain kind, name, and return type");
    context.expect(node != nullptr && node->range().begin().offset() == 0 &&
            node->range().end().offset() == 25 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-144 minimal delegate range must include its terminator exactly");
}

// Verifies parameter order and optional default expressions. Complexity is O(n).
void testDelegateParameters(TestContext& context)
{
    ParserFixture fixture("delegate void Work(int first, long second = 42);");
    AstNode* node = firstDeclaration(fixture);
    AstNode* first = node != nullptr && !node->children().empty() ? node->children()[0] : nullptr;
    AstNode* second = node != nullptr && node->children().size() > 1 ? node->children()[1] : nullptr;
    context.expect(node != nullptr && node->children().size() == 2 && first != nullptr &&
            first->text() == "first" && first->first() == nullptr && second != nullptr &&
            second->text() == "second",
        "TP-PAR-145 delegate parameters must retain source order and names");
    context.expect(second != nullptr && second->first() != nullptr &&
            second->first()->kind() == AstNodeKind::IntegerLiteralExpression &&
            second->first()->integerValue() == 42,
        "TP-PAR-145 optional parameter must retain its exact default expression");
}

// Verifies generic parameters and where clauses use the shared exact representation. Complexity is O(n).
void testGenericDelegate(TestContext& context)
{
    ParserFixture fixture("delegate int Select<T>(T value) where T : class;");
    AstNode* node = firstDeclaration(fixture);
    const bool exactConstraint = node != nullptr && node->genericConstraints().size() == 1 &&
        node->genericConstraints()[0].parameterName == "T" &&
        node->genericConstraints()[0].requiresClass;
    context.expect(node != nullptr && node->genericParameters().size() == 1 &&
            node->genericParameters()[0] == "T" && exactConstraint,
        "TP-PAR-146 delegate generic parameter and where clause must be exact");
    context.expect(fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-146 valid generic delegate must not diagnose");
}

// Verifies a typed throws clause remains separate from the return type. Complexity is O(n).
void testDelegateThrows(TestContext& context)
{
    ParserFixture fixture("delegate int Work(bool fail) throws Error;", {"Error"});
    AstNode* node = firstDeclaration(fixture);
    context.expect(node != nullptr && node->typeSyntax() != nullptr &&
            node->typeSyntax()->baseName() == "int" && node->throwsTypeSyntax() != nullptr &&
            node->throwsTypeSyntax()->baseName() == "Error",
        "TP-PAR-147 throws type must remain exact and independent from return type");
    context.expect(fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-147 valid typed throws clause must not diagnose");
}

// Verifies missing name, close parenthesis, and semicolon diagnose with finite progress. Complexity is O(n).
void testMalformedDelegates(TestContext& context)
{
    ParserFixture missingName("delegate int (); int after;");
    ParserFixture missingClose("delegate int D(int value; int after;");
    ParserFixture missingSemicolon("delegate int D() int after;");
    AstNode* nameRoot = missingName.parse();
    AstNode* closeRoot = missingClose.parse();
    AstNode* semicolonRoot = missingSemicolon.parse();
    context.expect(diagnosticCount(missingName.diagnostics(), DiagnosticId::ExpectedToken) >= 1 &&
            nameRoot != nullptr,
        "TP-PAR-148 missing delegate name must diagnose and terminate finitely");
    context.expect(diagnosticCount(missingClose.diagnostics(), DiagnosticId::ExpectedToken) >= 1 &&
            closeRoot != nullptr,
        "TP-PAR-148 missing close parenthesis must diagnose and terminate finitely");
    context.expect(diagnosticCount(missingSemicolon.diagnostics(), DiagnosticId::ExpectedToken) >= 1 &&
            semicolonRoot != nullptr,
        "TP-PAR-148 missing semicolon must diagnose and terminate finitely");
}

// Verifies valid global delegate modifiers propagate through ordinary and extern forms. Complexity is O(n).
void testDelegateModifiers(TestContext& context)
{
    ParserFixture fixture("public copy delegate int Managed(); internal extern C delegate int Native(int value);");
    AstNode* root = fixture.parse();
    AstNode* managed = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    AstNode* native = root != nullptr && root->children().size() > 1 ? root->children()[1] : nullptr;
    context.expect(managed != nullptr && managed->kind() == AstNodeKind::DelegateDeclaration &&
            managed->tokenKind() == TokenKind::KwPublic,
        "TP-PAR-149 public delegate accessibility must propagate");
    context.expect(native != nullptr && native->kind() == AstNodeKind::DelegateDeclaration &&
            native->tokenKind() == TokenKind::KwInternal && native->hasModifier(TokenKind::KwExtern),
        "TP-PAR-149 internal extern C delegate must preserve accessibility and extern marker");
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) == 1,
        "TP-PAR-149 invalid copy-on-delegate modifier must remain visibly diagnosed");
}

// Verifies malformed delegate recovery preserves the following valid declaration. Complexity is O(n).
void testDelegateRecovery(TestContext& context)
{
    ParserFixture fixture("delegate int (); int preserved;");
    AstNode* root = fixture.parse();
    AstNode* preserved = root != nullptr && !root->children().empty() ? root->children().back() : nullptr;
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-150 malformed delegate must diagnose");
    context.expect(preserved != nullptr && preserved->kind() == AstNodeKind::GlobalVariableDeclaration &&
            preserved->text() == "preserved",
        "TP-PAR-150 recovery must retain the following global declaration");
}
}

// Registers TP-PAR-144..150 for delegate syntax, metadata, and recovery.
void registerParserDelegateModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-144", testMinimalDelegate);
    addCase(registry, "TP-PAR-145", testDelegateParameters);
    addCase(registry, "TP-PAR-146", testGenericDelegate);
    addCase(registry, "TP-PAR-147", testDelegateThrows);
    addCase(registry, "TP-PAR-148", testMalformedDelegates);
    addCase(registry, "TP-PAR-149", testDelegateModifiers);
    addCase(registry, "TP-PAR-150", testDelegateRecovery);
}
}
