#include "ParserFoundationTestSupport.h"

#include <array>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;

void addCase(TestRegistry& registry, std::string id, TestFunction function,
    TestDisposition disposition = TestDisposition::Required, std::string profile = "PR")
{
    registry.add(TestCase{std::move(id), "CP-PAR-036", "Host.Parser", "component",
        std::move(profile), {"parser", "expression", "precedence"}, std::move(function), disposition});
}

AstNode* parseExpression(ParserFixture& fixture)
{
    return fixture.parser().parseExpression();
}

bool binary(const AstNode* node, TokenKind operation)
{
    return node != nullptr && node->kind() == AstNodeKind::BinaryExpression &&
        node->tokenKind() == operation;
}

void testAssignmentOperators(TestContext& context)
{
    struct Entry { std::string_view spelling; TokenKind kind; };
    constexpr std::array<Entry, 12> entries = {{{"=", TokenKind::Equal},
        {"+=", TokenKind::PlusEqual}, {"-=", TokenKind::MinusEqual},
        {"*=", TokenKind::StarEqual}, {"/=", TokenKind::SlashEqual},
        {"%=", TokenKind::PercentEqual}, {"&=", TokenKind::AmpEqual},
        {"|=", TokenKind::PipeEqual}, {"^=", TokenKind::CaretEqual},
        {"<<=", TokenKind::LessLessEqual}, {">>=", TokenKind::GreaterGreaterEqual},
        {"?" "?" "=", TokenKind::QuestionQuestionEqual}}};
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        const Entry& entry = entries[index];
        ParserFixture fixture("left " + std::string(entry.spelling) + " right");
        AstNode* const node = parseExpression(fixture);
        context.expect(node != nullptr && node->kind() == AstNodeKind::AssignmentExpression &&
                node->tokenKind() == entry.kind && node->first() != nullptr &&
                node->first()->text() == "left" && node->second() != nullptr &&
                node->second()->text() == "right" && fixture.diagnostics().diagnostics().empty(),
            "TP-PAR-427 every simple or compound assignment must retain its exact operator");
    }
}

void testAssignmentRightAssociativity(TestContext& context)
{
    ParserFixture fixture("a = b = c");
    AstNode* const node = parseExpression(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::AssignmentExpression &&
            node->first() != nullptr && node->first()->text() == "a" &&
            node->second() != nullptr && node->second()->kind() == AstNodeKind::AssignmentExpression &&
            node->second()->first()->text() == "b" && node->second()->second()->text() == "c" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-428 chained assignment must associate to the right");
}

void testConditionalAssociativity(TestContext& context)
{
    ParserFixture fixture("a ? b : c ? d : e");
    AstNode* const node = parseExpression(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ConditionalExpression &&
            node->first()->text() == "a" && node->second()->text() == "b" &&
            node->third() != nullptr && node->third()->kind() == AstNodeKind::ConditionalExpression &&
            node->third()->first()->text() == "c" && node->third()->second()->text() == "d" &&
            node->third()->third()->text() == "e" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-429 nested conditional expression must associate through the false branch");
}

void testNullCoalescingAssociativity(TestContext& context)
{
    ParserFixture fixture("a ?? b ?? c");
    AstNode* const node = parseExpression(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::NullCoalescingExpression &&
            node->first()->text() == "a" && node->second() != nullptr &&
            node->second()->kind() == AstNodeKind::NullCoalescingExpression &&
            node->second()->first()->text() == "b" && node->second()->second()->text() == "c" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-430 null coalescing must associate to the right");
}

void testRangeForms(TestContext& context)
{
    ParserFixture exclusive("begin..end");
    ParserFixture inclusive("begin..=end");
    ParserFixture omittedEnd("begin..");
    ParserFixture omittedBegin("..end");
    AstNode* const exclusiveNode = parseExpression(exclusive);
    AstNode* const inclusiveNode = parseExpression(inclusive);
    AstNode* const omittedEndNode = parseExpression(omittedEnd);
    AstNode* const omittedBeginNode = parseExpression(omittedBegin);
    context.expect(exclusiveNode != nullptr && exclusiveNode->kind() == AstNodeKind::RangeExpression &&
            !exclusiveNode->flag() && exclusiveNode->first() != nullptr && exclusiveNode->second() != nullptr &&
            inclusiveNode != nullptr && inclusiveNode->kind() == AstNodeKind::RangeExpression &&
            inclusiveNode->flag() && inclusiveNode->tokenKind() == TokenKind::DotDotEqual &&
            omittedEndNode != nullptr && omittedEndNode->kind() == AstNodeKind::RangeExpression &&
            omittedEndNode->first() != nullptr && omittedEndNode->second() == nullptr &&
            omittedBeginNode != nullptr && omittedBeginNode->kind() == AstNodeKind::RangeExpression &&
            omittedBeginNode->first() == nullptr && omittedBeginNode->second() != nullptr &&
            exclusive.diagnostics().diagnostics().empty() && inclusive.diagnostics().diagnostics().empty() &&
            omittedEnd.diagnostics().diagnostics().empty() && omittedBegin.diagnostics().diagnostics().empty(),
        "TP-PAR-431 exclusive, inclusive, and either omitted endpoint must retain exact range shape");
}

void testLogicalPrecedence(TestContext& context)
{
    ParserFixture fixture("a || b && c");
    AstNode* const node = parseExpression(fixture);
    context.expect(binary(node, TokenKind::PipePipe) && node->first()->text() == "a" &&
            binary(node->second(), TokenKind::AmpAmp) && node->second()->first()->text() == "b" &&
            node->second()->second()->text() == "c" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-432 logical AND must bind tighter than logical OR");
}

void testBitwisePrecedence(TestContext& context)
{
    ParserFixture fixture("a | b ^ c & d");
    AstNode* const node = parseExpression(fixture);
    context.expect(binary(node, TokenKind::Pipe) && binary(node->second(), TokenKind::Caret) &&
            binary(node->second()->second(), TokenKind::Amp) &&
            node->second()->second()->second()->text() == "d" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-433 bitwise AND, XOR, and OR must retain their strict precedence hierarchy");
}

void testEqualityRelationalPrecedence(TestContext& context)
{
    ParserFixture fixture("a == b < c");
    AstNode* const node = parseExpression(fixture);
    context.expect(binary(node, TokenKind::EqualEqual) && node->first()->text() == "a" &&
            binary(node->second(), TokenKind::Less) && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-434 relational comparison must bind tighter than equality");
}

void testShiftAdditivePrecedence(TestContext& context)
{
    ParserFixture fixture("a << b + c");
    AstNode* const node = parseExpression(fixture);
    context.expect(binary(node, TokenKind::LessLess) && node->first()->text() == "a" &&
            binary(node->second(), TokenKind::Plus) && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-435 addition must bind tighter than shift");
}

void testAdditiveMultiplicativePrecedence(TestContext& context)
{
    ParserFixture fixture("a + b * c");
    AstNode* const node = parseExpression(fixture);
    context.expect(binary(node, TokenKind::Plus) && node->first()->text() == "a" &&
            binary(node->second(), TokenKind::Star) && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-436 multiplication must bind tighter than addition");
}

void testLeftAssociativity(TestContext& context)
{
    ParserFixture fixture("a - b - c");
    AstNode* const node = parseExpression(fixture);
    context.expect(binary(node, TokenKind::Minus) && binary(node->first(), TokenKind::Minus) &&
            node->first()->first()->text() == "a" && node->first()->second()->text() == "b" &&
            node->second()->text() == "c" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-437 operators at one binary level must associate to the left");
}

void testParenthesizedPrecedence(TestContext& context)
{
    ParserFixture fixture("(a + b) * c");
    AstNode* const node = parseExpression(fixture);
    context.expect(binary(node, TokenKind::Star) && binary(node->first(), TokenKind::Plus) &&
            node->second()->text() == "c" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-438 parentheses must override the ordinary precedence hierarchy");
}

void testPostfixPropagationBeforeAddition(TestContext& context)
{
    ParserFixture fixture("result? + 1");
    AstNode* const node = parseExpression(fixture);
    context.expect(binary(node, TokenKind::Plus) && node->first() != nullptr &&
            node->first()->kind() == AstNodeKind::ResultPropagationExpression &&
            node->first()->first() != nullptr && node->first()->first()->text() == "result" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-439 question without same-level colon must be postfix propagation before addition");
}

void testConditionalWithUnaryTrueBranch(TestContext& context)
{
    ParserFixture fixture("condition ? +1 : 0");
    AstNode* const node = parseExpression(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ConditionalExpression &&
            node->second() != nullptr && node->second()->kind() == AstNodeKind::UnaryExpression &&
            node->second()->tokenKind() == TokenKind::Plus && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-440 same-level colon must make question conditional even when true branch starts unary plus");
}

void testNestedConditionalDelimiters(TestContext& context)
{
    ParserFixture fixture("outer ? (inner ? one : two) : three");
    AstNode* const node = parseExpression(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ConditionalExpression &&
            node->second() != nullptr && node->second()->kind() == AstNodeKind::ConditionalExpression &&
            node->second()->third()->text() == "two" && node->third()->text() == "three" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-441 nested delimiters must preserve ownership of each conditional colon");
}

void testPropagationAtDelimiters(TestContext& context)
{
    ParserFixture comma("value?, next");
    ParserFixture semicolon("value?;");
    ParserFixture closing("value?)");
    AstNode* const commaNode = parseExpression(comma);
    AstNode* const semicolonNode = parseExpression(semicolon);
    AstNode* const closingNode = parseExpression(closing);
    context.expect(commaNode != nullptr && commaNode->kind() == AstNodeKind::ResultPropagationExpression &&
            comma.parser().current().kind() == TokenKind::Comma && semicolonNode != nullptr &&
            semicolonNode->kind() == AstNodeKind::ResultPropagationExpression &&
            semicolon.parser().current().kind() == TokenKind::Semicolon && closingNode != nullptr &&
            closingNode->kind() == AstNodeKind::ResultPropagationExpression &&
            closing.parser().current().kind() == TokenKind::RightParen,
        "TP-PAR-442 comma, semicolon, and closing delimiter must terminate postfix propagation");
}

void testMissingOperandRecoveryAtEveryLevel(TestContext& context)
{
    constexpr std::array<std::string_view, 10> sources = {
        "a ||", "a &&", "a |", "a ^", "a &", "a ==", "a <", "a <<", "a +", "a *"};
    for (const std::string_view source : sources)
    {
        ParserFixture fixture{std::string(source)};
        AstNode* const node = parseExpression(fixture);
        context.expect(node != nullptr && fixture.diagnostics().hasErrors() &&
                fixture.parser().current().kind() == TokenKind::EndOfFile,
            "TP-PAR-443 missing operand at every binary level must diagnose and make finite progress");
    }
}

void testGoldenFullPrecedenceTree(TestContext& context)
{
    ParserFixture fixture("target = fallback ?? condition ? a || b && c | d ^ e & f == g < h << i + j * k : other");
    AstNode* const assignment = parseExpression(fixture);
    const AstNode* const conditional = assignment != nullptr ? assignment->second() : nullptr;
    const AstNode* const coalescing = conditional != nullptr ? conditional->first() : nullptr;
    const AstNode* const logicalOr = conditional != nullptr ? conditional->second() : nullptr;
    context.expect(assignment != nullptr && assignment->kind() == AstNodeKind::AssignmentExpression &&
            conditional != nullptr && conditional->kind() == AstNodeKind::ConditionalExpression &&
            coalescing != nullptr && coalescing->kind() == AstNodeKind::NullCoalescingExpression &&
            logicalOr != nullptr && logicalOr->kind() == AstNodeKind::BinaryExpression &&
            logicalOr->tokenKind() == TokenKind::PipePipe && conditional->third() != nullptr &&
            conditional->third()->text() == "other" && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-444 complete mixed expression must retain the golden assignment/coalescing/conditional hierarchy");
}
}

void registerParserExpressionPrecedenceContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-427", testAssignmentOperators);
    addCase(registry, "TP-PAR-428", testAssignmentRightAssociativity);
    addCase(registry, "TP-PAR-429", testConditionalAssociativity);
    addCase(registry, "TP-PAR-430", testNullCoalescingAssociativity);
    addCase(registry, "TP-PAR-431", testRangeForms, TestDisposition::KnownProductDefect);
    addCase(registry, "TP-PAR-432", testLogicalPrecedence);
    addCase(registry, "TP-PAR-433", testBitwisePrecedence);
    addCase(registry, "TP-PAR-434", testEqualityRelationalPrecedence);
    addCase(registry, "TP-PAR-435", testShiftAdditivePrecedence);
    addCase(registry, "TP-PAR-436", testAdditiveMultiplicativePrecedence);
    addCase(registry, "TP-PAR-437", testLeftAssociativity);
    addCase(registry, "TP-PAR-438", testParenthesizedPrecedence);
    addCase(registry, "TP-PAR-439", testPostfixPropagationBeforeAddition);
    addCase(registry, "TP-PAR-440", testConditionalWithUnaryTrueBranch);
    addCase(registry, "TP-PAR-441", testNestedConditionalDelimiters);
    addCase(registry, "TP-PAR-442", testPropagationAtDelimiters);
    addCase(registry, "TP-PAR-443", testMissingOperandRecoveryAtEveryLevel);
    addCase(registry, "TP-PAR-444", testGoldenFullPrecedenceTree, TestDisposition::Required, "NIGHTLY");
}
}
