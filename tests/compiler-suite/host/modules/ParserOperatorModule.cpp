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

// Returns the first member of the parsed top-level type. Complexity is O(1).
AstNode* firstMember(ParserFixture& fixture)
{
    AstNode* root = fixture.parse();
    AstNode* type = root != nullptr && !root->children().empty() ? root->children()[0] : nullptr;
    return type != nullptr && !type->children().empty() ? type->children()[0] : nullptr;
}

// Registers one user-operator parser case. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CP-PAR-024", "Host.Parser", "component", "PR",
        {"parser", "operators"}, std::move(function)});
}

// Verifies a supported unary operator maps to its canonical method and one parameter. Complexity is O(n).
void testUnaryOperator(TestContext& context)
{
    ParserFixture fixture("class C { public static C operator -(C value) { return value; } }");
    AstNode* method = firstMember(fixture);
    context.expect(method != nullptr && method->kind() == AstNodeKind::MethodDeclaration &&
            method->text() == "op_UnaryNegation" && method->children().size() == 1,
        "TP-PAR-245 unary minus must map to op_UnaryNegation with one parameter");
}

// Verifies a supported binary operator maps to its canonical method and two parameters. Complexity is O(n).
void testBinaryOperator(TestContext& context)
{
    ParserFixture fixture("class C { public static C operator +(C left, C right) { return left; } }");
    AstNode* method = firstMember(fixture);
    context.expect(method != nullptr && method->text() == "op_Addition" && method->children().size() == 2 &&
            method->children()[0]->text() == "left" && method->children()[1]->text() == "right",
        "TP-PAR-246 binary plus must map to op_Addition with two ordered parameters");
}

// Verifies increment/decrement naming remains canonical while the unsupported feature diagnoses. Complexity is O(n).
void testIncrementDecrement(TestContext& context)
{
    ParserFixture increment("class C { public static C operator ++(C value) { return value; } }");
    ParserFixture decrement("class C { public static C operator --(C value) { return value; } }");
    AstNode* inc = firstMember(increment);
    AstNode* dec = firstMember(decrement);
    context.expect(inc != nullptr && inc->text() == "op_Increment" && dec != nullptr &&
            dec->text() == "op_Decrement",
        "TP-PAR-247 increment and decrement must retain canonical names before rejection");
    context.expect(diagnosticCount(increment.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1 &&
            diagnosticCount(decrement.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1,
        "TP-PAR-247 unsupported user increment/decrement must remain visibly diagnosed");
}

// Verifies arity validation rejects an otherwise supported operator token. Complexity is O(n).
void testOperatorArity(TestContext& context)
{
    ParserFixture fixture("class C { public static C operator +(C a, C b, C c) { return a; } }");
    fixture.parse();
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidArgumentCount) == 1,
        "TP-PAR-248 three-argument plus must report exactly one InvalidArgumentCount");
}

// Verifies a non-overloadable short-circuit token is rejected by operator dispatch. Complexity is O(n).
void testUnsupportedOperatorToken(TestContext& context)
{
    ParserFixture fixture("class C { public static bool operator &&(C a, C b) { return true; } }");
    firstMember(fixture);
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 1,
        "TP-PAR-249 non-overloadable short-circuit token must report the current declaration diagnostic");
}

// Verifies public and static requirements are each diagnosed when absent. Complexity is O(n).
void testOperatorRequiredModifiers(TestContext& context)
{
    ParserFixture fixture("class C { C operator +(C a, C b) { return a; } }");
    AstNode* method = firstMember(fixture);
    context.expect(method != nullptr && method->hasModifier(TokenKind::KwPublic) &&
            method->hasModifier(TokenKind::KwStatic),
        "TP-PAR-250 recovered operator node must carry canonical public static modifiers");
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedDeclaration) >= 2,
        "TP-PAR-250 missing public and static requirements must each diagnose");
}
}

// Registers TP-PAR-245..250 for user-defined operators.
void registerParserOperatorModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-245", testUnaryOperator);
    addCase(registry, "TP-PAR-246", testBinaryOperator);
    addCase(registry, "TP-PAR-247", testIncrementDecrement);
    addCase(registry, "TP-PAR-248", testOperatorArity);
    addCase(registry, "TP-PAR-249", testUnsupportedOperatorToken);
    addCase(registry, "TP-PAR-250", testOperatorRequiredModifiers);
}
}
