#include "ParserExpressionTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using namespace expression_support;
using namespace parser_support;

// Verifies the literal family retains kind, decoded value, and non-empty range. Complexity is O(n).
void testLiterals(TestContext& context)
{
    const std::array<std::pair<std::string, AstNodeKind>, 7> cases{{
        {"42", AstNodeKind::IntegerLiteralExpression}, {"1.5", AstNodeKind::FloatingLiteralExpression},
        {"'A'", AstNodeKind::CharacterLiteralExpression}, {"\"text\"", AstNodeKind::StringLiteralExpression},
        {"true", AstNodeKind::BooleanLiteralExpression}, {"false", AstNodeKind::BooleanLiteralExpression},
        {"null", AstNodeKind::NullLiteralExpression}}};
    for (const auto& [source, kind] : cases)
    {
        ParserFixture fixture(source);
        AstNode* node = parse(fixture);
        context.expect(node != nullptr && node->kind() == kind &&
                node->range().begin().offset() < node->range().end().offset(),
            "TP-PAR-495 literal must retain exact kind and non-empty source range");
    }
    ParserFixture integer("42");
    ParserFixture floating("1.5");
    ParserFixture character("'A'");
    ParserFixture stringLiteral("\"text\"");
    ParserFixture truth("true");
    AstNode* integerNode = parse(integer);
    AstNode* floatingNode = parse(floating);
    AstNode* characterNode = parse(character);
    AstNode* stringNode = parse(stringLiteral);
    AstNode* truthNode = parse(truth);
    context.expect(integerNode->integerValue() == 42 && floatingNode->floatingValue() == 1.5 &&
            characterNode->integerValue() == 65 && stringNode->text() == "text" &&
            truthNode->tokenKind() == TokenKind::KwTrue && truthNode->text() == "true",
        "TP-PAR-495 decoded literal values must match their exact source values");
}

// Verifies identifier, this, base, and null dispatch to exact primary kinds/text. Complexity is O(n).
void testNamedPrimaries(TestContext& context)
{
    ParserFixture identifier("value");
    ParserFixture thisFixture("this");
    ParserFixture baseFixture("base");
    ParserFixture nullFixture("null");
    AstNode* id = parse(identifier);
    AstNode* thisNode = parse(thisFixture);
    AstNode* baseNode = parse(baseFixture);
    AstNode* nullNode = parse(nullFixture);
    context.expect(id != nullptr && id->kind() == AstNodeKind::IdentifierExpression && id->text() == "value" &&
            thisNode != nullptr && thisNode->kind() == AstNodeKind::ThisExpression && baseNode != nullptr &&
            baseNode->kind() == AstNodeKind::BaseExpression && nullNode != nullptr &&
            nullNode->kind() == AstNodeKind::NullLiteralExpression,
        "TP-PAR-496 identifier, this, base, and null must dispatch to exact primary nodes");
}

// Verifies parenthesized expression preserves child identity while expanding its range. Complexity is O(n).
void testParenthesizedExpression(TestContext& context)
{
    ParserFixture fixture("(value)");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::IdentifierExpression &&
            node->text() == "value" && node->range().begin().offset() == 0 && node->range().end().offset() == 7,
        "TP-PAR-497 parenthesized expression must reuse its child node and expand the contractual range");
}

// Verifies empty/multiple tuples and the current non-tuple unary parenthesis. Complexity is O(n).
void testTuples(TestContext& context)
{
    ParserFixture empty("()");
    ParserFixture unary("(value)");
    ParserFixture multiple("(first, second)");
    AstNode* emptyNode = parse(empty);
    AstNode* unaryNode = parse(unary);
    AstNode* multipleNode = parse(multiple);
    context.expect(emptyNode != nullptr && emptyNode->kind() == AstNodeKind::TupleExpression &&
            emptyNode->children().empty() && multipleNode != nullptr &&
            multipleNode->kind() == AstNodeKind::TupleExpression && multipleNode->children().size() == 2,
        "TP-PAR-498 empty and multiple tuples must retain structural tuple nodes");
    context.expect(unaryNode != nullptr && unaryNode->kind() == AstNodeKind::IdentifierExpression,
        "TP-PAR-498 unary parenthesis is grouping, not a one-element tuple in the current grammar");
}

// Verifies match, loop, and comprehension prefixes dispatch to their dedicated primaries. Complexity is O(n).
void testStructuredPrimaries(TestContext& context)
{
    ParserFixture match("match (value) { _ => 0 }");
    ParserFixture loop("loop { break 1; }");
    ParserFixture comprehension("comprehend(seed) { yield return 1; }");
    AstNode* matchNode = parse(match);
    AstNode* loopNode = parse(loop);
    AstNode* comprehensionNode = parse(comprehension);
    context.expect(matchNode != nullptr && matchNode->kind() == AstNodeKind::MatchExpression &&
            loopNode != nullptr && loopNode->kind() == AstNodeKind::LoopExpression,
        "TP-PAR-499 match and loop must dispatch to dedicated primary nodes");
    context.expect(comprehensionNode != nullptr && comprehensionNode->kind() == AstNodeKind::LoopExpression &&
            comprehensionNode->auxiliaryText() == "comprehension:explicit-builder" &&
            comprehensionNode->first() != nullptr,
        "TP-PAR-499 comprehension must dispatch through its current lowered primary contract");
}

// Verifies an interpolated token without holes retains the current interpolated node kind. Complexity is O(n).
void testStaticInterpolated(TestContext& context)
{
    ParserFixture fixture("$\"plain\"");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::InterpolatedStringExpression &&
            node->integerValue() == 1 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-500 interpolation without holes must retain the current interpolated-string node");
}
}

// Registers TP-PAR-495..500 for primary expression dispatch.
void registerParserPrimaryExpressionModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-495", "CP-PAR-040", testLiterals);
    addCase(registry, "TP-PAR-496", "CP-PAR-040", testNamedPrimaries);
    addCase(registry, "TP-PAR-497", "CP-PAR-040", testParenthesizedExpression);
    addCase(registry, "TP-PAR-498", "CP-PAR-040", testTuples);
    addCase(registry, "TP-PAR-499", "CP-PAR-040", testStructuredPrimaries);
    addCase(registry, "TP-PAR-500", "CP-PAR-040", testStaticInterpolated);
}
}
