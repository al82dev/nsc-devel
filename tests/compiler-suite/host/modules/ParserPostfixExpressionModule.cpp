#include "ParserExpressionTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace expression_support;
using namespace parser_support;

// Verifies empty and populated calls preserve callee and ordered arguments. Complexity is O(n).
void testCalls(TestContext& context)
{
    ParserFixture empty("run()");
    ParserFixture populated("run(first, second)");
    AstNode* emptyCall = parse(empty);
    AstNode* populatedCall = parse(populated);
    context.expect(emptyCall != nullptr && emptyCall->kind() == AstNodeKind::CallExpression &&
            emptyCall->first() != nullptr && emptyCall->children().empty(),
        "TP-PAR-477 empty call must retain callee and zero arguments");
    context.expect(populatedCall != nullptr && populatedCall->children().size() == 2 &&
            populatedCall->children()[0]->text() == "first" && populatedCall->children()[1]->text() == "second",
        "TP-PAR-477 populated call must retain argument order");
}

// Verifies named, by-reference, move, and open-range arguments preserve exact metadata. Complexity is O(n).
void testArgumentMetadata(TestContext& context)
{
    ParserFixture fixture("run(named: value, ref target, move(owner))");
    AstNode* call = parse(fixture);
    AstNode* named = call != nullptr && !call->children().empty() ? call->children()[0] : nullptr;
    AstNode* reference = call != nullptr && call->children().size() > 1 ? call->children()[1] : nullptr;
    AstNode* moved = call != nullptr && call->children().size() > 2 ? call->children()[2] : nullptr;
    context.expect(named != nullptr && named->argumentName() == "named" && reference != nullptr &&
            reference->kind() == AstNodeKind::UnaryExpression && reference->auxiliaryText() == "pass:ref" &&
            moved != nullptr && moved->kind() == AstNodeKind::MoveExpression,
        "TP-PAR-478 named, ref, and move arguments must retain exact metadata");
    ParserFixture rangeFixture("run(..items)");
    AstNode* rangeCall = parse(rangeFixture);
    AstNode* range = rangeCall != nullptr && !rangeCall->children().empty()
        ? rangeCall->children()[0] : nullptr;
    context.expect(range != nullptr && range->kind() == AstNodeKind::RangeExpression &&
            range->tokenKind() == TokenKind::DotDot && range->first() == nullptr &&
            range->second() != nullptr && range->second()->text() == "items" &&
            rangeFixture.diagnostics().diagnostics().empty(),
        "TP-PAR-478 open range call argument must preserve its omitted start and end expression");
}

// Verifies explicit generic call arguments remain separate from value arguments. Complexity is O(n).
void testGenericCall(TestContext& context)
{
    ParserFixture fixture("run<int, string>(value)");
    AstNode* call = parse(fixture);
    context.expect(call != nullptr && call->kind() == AstNodeKind::CallExpression &&
            call->typeSyntaxes().size() == 2 && call->typeSyntaxes()[0]->baseName() == "int" &&
            call->typeSyntaxes()[1]->baseName() == "string" && call->children().size() == 1,
        "TP-PAR-479 generic call must preserve type and value argument lists independently");
}

// Verifies one- and multi-index expressions retain every index in order. Complexity is O(n).
void testIndices(TestContext& context)
{
    ParserFixture one("items[index]");
    ParserFixture many("matrix[row, column]");
    AstNode* oneNode = parse(one);
    AstNode* manyNode = parse(many);
    context.expect(oneNode != nullptr && oneNode->kind() == AstNodeKind::IndexExpression &&
            oneNode->children().size() == 1 && oneNode->second() == oneNode->children()[0],
        "TP-PAR-480 one-index expression must expose the index through second and children");
    context.expect(manyNode != nullptr && manyNode->children().size() == 2 &&
            manyNode->children()[0]->text() == "row" && manyNode->children()[1]->text() == "column",
        "TP-PAR-480 multi-index expression must retain index order");
}

// Verifies slice index owns a range whose end may be from-end. Complexity is O(n).
void testSliceIndex(TestContext& context)
{
    ParserFixture fixture("items[1..^1]");
    AstNode* index = parse(fixture);
    AstNode* range = index != nullptr ? index->second() : nullptr;
    context.expect(range != nullptr && range->kind() == AstNodeKind::RangeExpression &&
            range->first() != nullptr && range->first()->integerValue() == 1 && range->second() != nullptr &&
            range->second()->kind() == AstNodeKind::FromEndExpression,
        "TP-PAR-481 slice index must retain range endpoints and from-end node");
}

// Verifies null-conditional member access retains member and source range. Complexity is O(n).
void testNullConditionalMember(TestContext& context)
{
    ParserFixture fixture("source?.value");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::NullConditionalMemberExpression &&
            node->text() == "value" && node->first() != nullptr && node->range().begin().offset() == 0 &&
            node->range().end().offset() == 13,
        "TP-PAR-482 null-conditional member must retain source, name, and complete range");
}

// Verifies null-conditional index retains the receiver and one index child. Complexity is O(n).
void testNullConditionalIndex(TestContext& context)
{
    ParserFixture fixture("source?[0]");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::NullConditionalIndexExpression &&
            node->first() != nullptr && node->first()->text() == "source" &&
            node->children().size() == 1,
        "TP-PAR-537 null-conditional index must retain receiver and index");
}

// Verifies ordinary dot access produces MemberExpression. Complexity is O(n).
void testDotMember(TestContext& context)
{
    ParserFixture fixture("source.value");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::MemberExpression &&
            node->text() == "value" && node->first() != nullptr && node->first()->text() == "source",
        "TP-PAR-483 dot member must preserve receiver and member name");
}

// Verifies arrow access lowers syntactically to marked dereference plus member access. Complexity is O(n).
void testArrowMember(TestContext& context)
{
    ParserFixture fixture("pointer->field");
    AstNode* member = parse(fixture);
    AstNode* dereference = member != nullptr ? member->first() : nullptr;
    context.expect(member != nullptr && member->kind() == AstNodeKind::MemberExpression &&
            member->text() == "field" && dereference != nullptr &&
            dereference->kind() == AstNodeKind::UnaryExpression &&
            dereference->tokenKind() == TokenKind::Star && dereference->auxiliaryText() == "arrow",
        "TP-PAR-484 arrow must synthesize an exactly marked dereference receiver");
}

// Verifies parser preserves arrow syntax without deciding receiver pointer type. Complexity is O(n).
void testArrowWithoutSyntacticPointer(TestContext& context)
{
    ParserFixture fixture("value->field");
    AstNode* member = parse(fixture);
    context.expect(member != nullptr && member->kind() == AstNodeKind::MemberExpression &&
            member->first() != nullptr && member->first()->auxiliaryText() == "arrow" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-485 parser must preserve arrow form for later semantic pointer validation");
}

// Verifies both postfix update operators retain their exact tokens. Complexity is O(n).
void testPostfixUpdates(TestContext& context)
{
    ParserFixture increment("value++");
    ParserFixture decrement("value--");
    AstNode* inc = parse(increment);
    AstNode* dec = parse(decrement);
    context.expect(inc != nullptr && inc->kind() == AstNodeKind::PostfixExpression &&
            inc->tokenKind() == TokenKind::PlusPlus && dec != nullptr &&
            dec->kind() == AstNodeKind::PostfixExpression && dec->tokenKind() == TokenKind::MinusMinus,
        "TP-PAR-486 postfix increment and decrement must retain exact tokens");
}

// Verifies postfix result propagation retains carrier and question token. Complexity is O(n).
void testResultPropagation(TestContext& context)
{
    ParserFixture fixture("result?");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ResultPropagationExpression &&
            node->tokenKind() == TokenKind::Question && node->first() != nullptr &&
            node->first()->text() == "result",
        "TP-PAR-487 result propagation must retain carrier and postfix token");
}

// Verifies with expression retains source and one named replacement. Complexity is O(n).
void testWithExpression(TestContext& context)
{
    ParserFixture fixture("source with { Value = 42 }");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::WithExpression &&
            node->first() != nullptr && node->first()->text() == "source" && node->children().size() == 1 &&
            node->children()[0]->argumentName() == "Value" && node->children()[0]->integerValue() == 42,
        "TP-PAR-488 with expression must retain source and named replacement");
}

// Verifies multiple with replacements and trailing comma preserve order. Complexity is O(n).
void testWithMultiple(TestContext& context)
{
    ParserFixture fixture("source with { Left = 1, Right = 2, }");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->children().size() == 2 &&
            node->children()[0]->argumentName() == "Left" &&
            node->children()[1]->argumentName() == "Right",
        "TP-PAR-489 with replacements must retain source order and accept trailing comma");
}

// Verifies a mixed postfix chain associates left-to-right. Complexity is O(n).
void testMixedChain(TestContext& context)
{
    ParserFixture fixture("source.items[0](arg).value");
    AstNode* outer = parse(fixture);
    AstNode* call = outer != nullptr ? outer->first() : nullptr;
    AstNode* index = call != nullptr ? call->first() : nullptr;
    AstNode* innerMember = index != nullptr ? index->first() : nullptr;
    context.expect(outer != nullptr && outer->kind() == AstNodeKind::MemberExpression &&
            call != nullptr && call->kind() == AstNodeKind::CallExpression && index != nullptr &&
            index->kind() == AstNodeKind::IndexExpression && innerMember != nullptr &&
            innerMember->kind() == AstNodeKind::MemberExpression,
        "TP-PAR-490 mixed postfix chain must associate left-to-right");
}

// Verifies incomplete member, index, and argument delimiters diagnose with finite output. Complexity is O(n).
void testMalformedPostfix(TestContext& context)
{
    ParserFixture memberFixture("source.");
    ParserFixture indexFixture("source[1");
    ParserFixture callFixture("run(value");
    parse(memberFixture);
    parse(indexFixture);
    parse(callFixture);
    context.expect(diagnosticCount(memberFixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1 &&
            diagnosticCount(indexFixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1 &&
            diagnosticCount(callFixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-491 incomplete postfix delimiters must diagnose and terminate");
}

// Verifies conditional question and nested postfix question remain disambiguated. Complexity is O(n).
void testQuestionDisambiguation(TestContext& context)
{
    ParserFixture fixture("condition ? left : result?");
    AstNode* conditional = parse(fixture);
    context.expect(conditional != nullptr && conditional->kind() == AstNodeKind::ConditionalExpression &&
            conditional->third() != nullptr &&
            conditional->third()->kind() == AstNodeKind::ResultPropagationExpression,
        "TP-PAR-492 conditional and result-propagation question marks must retain distinct nesting");
}

// Verifies a closed generic static receiver survives into the member/call chain. Complexity is O(n).
void testStaticGenericReceiver(TestContext& context)
{
    ParserFixture fixture("Box<int>.Make(value)");
    AstNode* call = parse(fixture);
    AstNode* member = call != nullptr ? call->first() : nullptr;
    AstNode* receiver = member != nullptr ? member->first() : nullptr;
    context.expect(call != nullptr && call->kind() == AstNodeKind::CallExpression && member != nullptr &&
            member->kind() == AstNodeKind::MemberExpression && receiver != nullptr &&
            receiver->typeSyntax() != nullptr && receiver->typeSyntax()->typeArguments().size() == 1,
        "TP-PAR-493 closed generic static receiver must retain canonical TypeSyntax");
}

// Verifies a deep postfix chain retains complete source range using iterative test construction. Complexity is O(n).
void testDeepChain(TestContext& context)
{
    std::string source = "root";
    for (std::size_t index = 0; index < 64; ++index) source += ".m";
    ParserFixture fixture(source);
    AstNode* node = parse(fixture);
    AstNode* outer = node;
    std::size_t depth = 0;
    while (node != nullptr && node->kind() == AstNodeKind::MemberExpression)
    {
        ++depth;
        node = node->first();
    }
    context.expect(depth == 64 && node != nullptr && node->text() == "root",
        "TP-PAR-494 deep member chain must retain all 64 postfix nodes");
    context.expect(outer != nullptr && outer->range().begin().offset() == 0 &&
            outer->range().end().offset() == source.size() && fixture.parser().at(TokenKind::EndOfFile),
        "TP-PAR-494 deep member chain range must span base through final postfix");
}
}

// Registers TP-PAR-477..494 for calls, indices, members, and postfix forms.
void registerParserPostfixExpressionModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-477", "CP-PAR-039", testCalls);
    addCase(registry, "TP-PAR-478", "CP-PAR-039", testArgumentMetadata);
    addCase(registry, "TP-PAR-479", "CP-PAR-039", testGenericCall);
    addCase(registry, "TP-PAR-480", "CP-PAR-039", testIndices);
    addCase(registry, "TP-PAR-481", "CP-PAR-039", testSliceIndex);
    addCase(registry, "TP-PAR-482", "CP-PAR-039", testNullConditionalMember);
    addCase(registry, "TP-PAR-483", "CP-PAR-039", testDotMember);
    addCase(registry, "TP-PAR-484", "CP-PAR-039", testArrowMember);
    addCase(registry, "TP-PAR-485", "CP-PAR-039", testArrowWithoutSyntacticPointer);
    addCase(registry, "TP-PAR-486", "CP-PAR-039", testPostfixUpdates);
    addCase(registry, "TP-PAR-487", "CP-PAR-039", testResultPropagation);
    addCase(registry, "TP-PAR-488", "CP-PAR-039", testWithExpression);
    addCase(registry, "TP-PAR-489", "CP-PAR-039", testWithMultiple);
    addCase(registry, "TP-PAR-490", "CP-PAR-039", testMixedChain);
    addCase(registry, "TP-PAR-491", "CP-PAR-039", testMalformedPostfix);
    addCase(registry, "TP-PAR-492", "CP-PAR-039", testQuestionDisambiguation);
    addCase(registry, "TP-PAR-493", "CP-PAR-039", testStaticGenericReceiver);
    addCase(registry, "TP-PAR-494", "CP-PAR-039", testDeepChain, "NIGHTLY");
    addCase(registry, "TP-PAR-537", "CP-PAR-039", testNullConditionalIndex);
}
}
