#include "ParserExpressionTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace expression_support;
using namespace parser_support;

// Verifies the lambda grammar recognizes noheap/unsafe effect prefixes and still rejects async.
// Complexity is O(n).
void testCurrentLambdaFlags(TestContext& context)
{
    ParserFixture ordinary("x => x");
    ParserFixture asyncPrefix("async x => x");
    ParserFixture unsafePrefix("unsafe x => x");
    ParserFixture noheapPrefix("noheap x => x");
    AstNode* lambda = parse(ordinary);
    context.expect(lambda != nullptr && lambda->kind() == AstNodeKind::LambdaExpression && !lambda->flag(),
        "TP-PAR-451 ordinary expression lambda must retain the current body flag");
    context.expect(!asyncPrefix.parser().looksLikeLambdaExpression() &&
            unsafePrefix.parser().looksLikeLambdaExpression() &&
            noheapPrefix.parser().looksLikeLambdaExpression(),
        "TP-PAR-451 async remains unsupported; noheap/unsafe are recognized lambda effect prefixes");
}

// Verifies a parenthesized arithmetic expression is not a lambda head. Complexity is O(n).
void testParenthesisNotLambda(TestContext& context)
{
    ParserFixture fixture("(a + b)");
    context.expect(!fixture.parser().looksLikeLambdaExpression(),
        "TP-PAR-452 parenthesized arithmetic must not satisfy lambda lookahead");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::BinaryExpression,
        "TP-PAR-452 parenthesized arithmetic must continue through normal expression parsing");
}

// Verifies a complete parameter-like parenthesis without an arrow is not a lambda. Complexity is O(n).
void testArrowAbsent(TestContext& context)
{
    ParserFixture fixture("(value)");
    context.expect(!fixture.parser().looksLikeLambdaExpression(),
        "TP-PAR-453 missing fat arrow must prevent lambda classification");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::IdentifierExpression && node->text() == "value",
        "TP-PAR-453 non-lambda parenthesis must preserve the enclosed expression");
}

// Verifies nested brackets in typed parameters do not terminate lambda lookahead. Complexity is O(n).
void testNestedDelimiterLookahead(TestContext& context)
{
    ParserFixture fixture("(int[] values, int index) => values[index]");
    context.expect(fixture.parser().looksLikeLambdaExpression(),
        "TP-PAR-454 lambda lookahead must cross nested array brackets to the matching arrow");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::LambdaExpression &&
            node->children().size() == 2 && node->first() != nullptr &&
            node->first()->kind() == AstNodeKind::IndexExpression,
        "TP-PAR-454 typed lambda must retain parameters and indexed body");
}

// Verifies malformed lambda parameters diagnose and parsing terminates. Complexity is O(n).
void testMalformedLambda(TestContext& context)
{
    ParserFixture fixture("(x, ) => x");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedType) +
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-455 malformed lambda must diagnose and return finite parser output");
}

// Verifies nested lambdas preserve scope topology and enclosing ranges. Complexity is O(n).
void testNestedLambdas(TestContext& context)
{
    ParserFixture fixture("x => y => x + y");
    AstNode* outer = parse(fixture);
    AstNode* inner = outer != nullptr ? outer->first() : nullptr;
    context.expect(outer != nullptr && outer->kind() == AstNodeKind::LambdaExpression &&
            inner != nullptr && inner->kind() == AstNodeKind::LambdaExpression &&
            outer->children().size() == 1 && inner->children().size() == 1,
        "TP-PAR-456 nested lambdas must retain separate parameter scopes");
    context.expect(outer != nullptr && inner != nullptr && outer->range().begin().offset() == 0 &&
            outer->range().end().offset() == 15 && inner->range().begin().offset() > 0,
        "TP-PAR-456 nested lambda ranges must be properly contained");
}
}

// Registers TP-PAR-451..456 for lambda recognition and parsing.
void registerParserLambdaLookaheadModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-451", "CP-PAR-037", testCurrentLambdaFlags);
    addCase(registry, "TP-PAR-452", "CP-PAR-037", testParenthesisNotLambda);
    addCase(registry, "TP-PAR-453", "CP-PAR-037", testArrowAbsent);
    addCase(registry, "TP-PAR-454", "CP-PAR-037", testNestedDelimiterLookahead);
    addCase(registry, "TP-PAR-455", "CP-PAR-037", testMalformedLambda);
    addCase(registry, "TP-PAR-456", "CP-PAR-037", testNestedLambdas);
}
}
