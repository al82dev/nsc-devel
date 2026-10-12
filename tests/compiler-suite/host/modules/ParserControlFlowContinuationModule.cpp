#include "ParserFoundationTestSupport.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addCase(TestRegistry& registry, std::string id, TestFunction function,
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CP-PAR-030", "Host.Parser", "component", "PR",
        {"parser", "statement", "control-flow"}, std::move(function), disposition});
}

AstNode* parseStatement(ParserFixture& fixture)
{
    return fixture.parser().parseStatement();
}

void testIfHierarchy(TestContext& context)
{
    ParserFixture simple("if (ready) { run(); }");
    ParserFixture withElse("if (ready) { run(); } else { wait(); }");
    ParserFixture elseIf("if (first) { a(); } else if (second) { b(); } else { c(); }");
    AstNode* const simpleNode = parseStatement(simple);
    AstNode* const elseNode = parseStatement(withElse);
    AstNode* const chain = parseStatement(elseIf);
    context.expect(simpleNode != nullptr && simpleNode->kind() == AstNodeKind::IfStatement &&
            simpleNode->first() != nullptr && simpleNode->second() != nullptr && simpleNode->third() == nullptr &&
            elseNode != nullptr && elseNode->third() != nullptr &&
            elseNode->third()->kind() == AstNodeKind::BlockStatement &&
            chain != nullptr && chain->third() != nullptr && chain->third()->kind() == AstNodeKind::IfStatement &&
            chain->third()->third() != nullptr && simple.diagnostics().diagnostics().empty() &&
            withElse.diagnostics().diagnostics().empty() && elseIf.diagnostics().diagnostics().empty(),
        "TP-PAR-333 if, else, and else-if must retain one deterministic hierarchy");
}

void testConditionDeclaration(TestContext& context)
{
    ParserFixture fixture("if (int value = source) { consume(value); }");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::BlockStatement &&
            node->children().size() == 2 &&
            node->children()[0]->kind() == AstNodeKind::VariableDeclarationStatement &&
            node->children()[1]->kind() == AstNodeKind::IfStatement &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-334 condition declaration must lower to a scoped local followed by its conditional");
}

void testConditionalPattern(TestContext& context)
{
    ParserFixture fixture("if (let var item = source) { consume(item); }");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::BlockStatement &&
            node->children().size() >= 3 &&
            node->children()[0]->kind() == AstNodeKind::VariableDeclarationStatement &&
            node->children()[1]->kind() == AstNodeKind::VariableDeclarationStatement &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-335 conditional pattern must lower its source, match state, test, bindings, and body");
}

void testWhileStatement(TestContext& context)
{
    ParserFixture fixture("while (ready) { tick(); }");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::WhileStatement &&
            node->first() != nullptr && node->first()->kind() == AstNodeKind::IdentifierExpression &&
            node->first()->text() == "ready" && node->second() != nullptr &&
            node->second()->kind() == AstNodeKind::BlockStatement &&
            node->second()->children().size() == 1 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-336 while statement must retain its condition and block body");
}

void testDoWhileStatement(TestContext& context)
{
    ParserFixture fixture("do { tick(); } while (ready);");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::DoWhileStatement &&
            node->first() != nullptr && node->first()->text() == "ready" &&
            node->second() != nullptr && node->second()->kind() == AstNodeKind::BlockStatement &&
            node->second()->children().size() == 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-337 do-while must normalize condition/body slots and consume its final semicolon");
}

void testEmptyForStatement(TestContext& context)
{
    ParserFixture fixture("for (;;) {}");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ForStatement &&
            node->first() == nullptr && node->second() == nullptr && node->third() == nullptr &&
            node->children().size() == 1 && node->children()[0] != nullptr &&
            node->children()[0]->kind() == AstNodeKind::BlockStatement &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-338 empty for clause must preserve three null components and one body");
}

void testFullForStatement(TestContext& context)
{
    ParserFixture fixture("for (int index = 0; index < limit; index = index + 1) { work(); }");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::ForStatement &&
            node->first() != nullptr && node->first()->kind() == AstNodeKind::VariableDeclarationStatement &&
            node->second() != nullptr && node->second()->kind() == AstNodeKind::BinaryExpression &&
            node->third() != nullptr && node->third()->kind() == AstNodeKind::AssignmentExpression &&
            node->children().size() == 1 && node->children()[0]->kind() == AstNodeKind::BlockStatement &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-339 full for statement must retain initializer, condition, increment, and body");
}

void testEmptySwitch(TestContext& context)
{
    ParserFixture fixture("switch (value) {}");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::SwitchStatement &&
            node->first() != nullptr && node->first()->text() == "value" &&
            node->children().empty() && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-340 empty switch must retain governing expression and zero sections");
}

void testGroupedCaseLabels(TestContext& context)
{
    ParserFixture fixture("switch (value) { case 1: case 2: act(); break; }");
    AstNode* const node = parseStatement(fixture);
    const AstNode* const section = node != nullptr && node->children().size() == 1
        ? node->children()[0] : nullptr;
    context.expect(section != nullptr && section->kind() == AstNodeKind::SwitchSection &&
            section->integerValue() == 2 && section->children().size() == 4 &&
            section->children()[0]->kind() == AstNodeKind::SwitchLabel &&
            section->children()[1]->kind() == AstNodeKind::SwitchLabel &&
            section->children()[2]->kind() == AstNodeKind::ExpressionStatement &&
            section->children()[3]->kind() == AstNodeKind::BreakStatement &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-341 adjacent case labels must be grouped into one section before statements");
}

void testDefaultSwitchLabel(TestContext& context)
{
    ParserFixture fixture("switch (value) { default: break; }");
    AstNode* const node = parseStatement(fixture);
    const AstNode* const section = node != nullptr && node->children().size() == 1
        ? node->children()[0] : nullptr;
    const AstNode* const label = section != nullptr && !section->children().empty()
        ? section->children()[0] : nullptr;
    context.expect(label != nullptr && label->kind() == AstNodeKind::SwitchLabel &&
            label->flag() && label->first() == nullptr && section->integerValue() == 1 &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-342 default label must carry its explicit flag and no value expression");
}

void testDefaultExpressionInsideSection(TestContext& context)
{
    ParserFixture fixture("switch (value) { case 1: default(int); break; }");
    AstNode* const node = parseStatement(fixture);
    const AstNode* const section = node != nullptr && node->children().size() == 1
        ? node->children()[0] : nullptr;
    context.expect(section != nullptr && section->integerValue() == 1 && section->children().size() == 3 &&
            section->children()[1]->kind() == AstNodeKind::ExpressionStatement &&
            section->children()[1]->first() != nullptr &&
            section->children()[1]->first()->kind() == AstNodeKind::DefaultValueExpression &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-343 default(T) at section start must remain an expression rather than a label");
}

void testImplicitSwitchFallthroughShape(TestContext& context)
{
    ParserFixture fixture("switch (value) { case 1: act(); case 2: stop(); }");
    AstNode* const node = parseStatement(fixture);
    context.expect(node != nullptr && node->children().size() == 2 &&
            node->children()[0]->kind() == AstNodeKind::SwitchSection &&
            node->children()[0]->children().size() == 2 &&
            node->children()[1]->kind() == AstNodeKind::SwitchSection &&
            node->children()[1]->children().size() == 2 &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-344 parser must preserve two independent sections for implicit source fallthrough");
}

void testBreakAndContinue(TestContext& context)
{
    ParserFixture breakFixture("break;");
    ParserFixture continueFixture("continue;");
    AstNode* const breakNode = parseStatement(breakFixture);
    AstNode* const continueNode = parseStatement(continueFixture);
    context.expect(breakNode != nullptr && breakNode->kind() == AstNodeKind::BreakStatement &&
            breakNode->range().begin().offset() == 0 && breakNode->range().end().offset() == 6 &&
            continueNode != nullptr && continueNode->kind() == AstNodeKind::ContinueStatement &&
            continueNode->range().begin().offset() == 0 && continueNode->range().end().offset() == 9 &&
            breakFixture.diagnostics().diagnostics().empty() &&
            continueFixture.diagnostics().diagnostics().empty(),
        "TP-PAR-345 break and continue must retain distinct kinds and complete source ranges");
}

void testSimpleDivergence(TestContext& context)
{
    ParserFixture returns("return 1;");
    ParserFixture throws("throw error;");
    AstNode* const returnNode = parseStatement(returns);
    AstNode* const throwNode = parseStatement(throws);
    context.expect(returns.parser().statementAlwaysDiverges(returnNode) &&
            throws.parser().statementAlwaysDiverges(throwNode),
        "TP-PAR-346 return and throw statements must both be classified as divergent");
}

void testConditionalDivergence(TestContext& context)
{
    ParserFixture both("if (ready) { return 1; } else { throw error; }");
    ParserFixture one("if (ready) { return 1; } else { work(); }");
    AstNode* const bothNode = parseStatement(both);
    AstNode* const oneNode = parseStatement(one);
    context.expect(both.parser().statementAlwaysDiverges(bothNode) &&
            !one.parser().statementAlwaysDiverges(oneNode),
        "TP-PAR-347 if diverges only when both explicit arms diverge");
}

void testMalformedControlRecovery(TestContext& context)
{
    ParserFixture fixture("{ if (ready { first(); } second(); }");
    AstNode* const block = parseStatement(fixture);
    context.expect(block != nullptr && block->kind() == AstNodeKind::BlockStatement &&
            block->children().size() == 2 && block->children()[0]->kind() == AstNodeKind::IfStatement &&
            block->children()[1]->kind() == AstNodeKind::ExpressionStatement &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1 &&
            fixture.parser().current().kind() == TokenKind::EndOfFile,
        "TP-PAR-348 malformed control syntax must recover without erasing the following statement");
}
}

void registerParserControlFlowContinuationModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-333", testIfHierarchy);
    addCase(registry, "TP-PAR-334", testConditionDeclaration);
    addCase(registry, "TP-PAR-335", testConditionalPattern);
    addCase(registry, "TP-PAR-336", testWhileStatement);
    addCase(registry, "TP-PAR-337", testDoWhileStatement);
    addCase(registry, "TP-PAR-338", testEmptyForStatement);
    addCase(registry, "TP-PAR-339", testFullForStatement);
    addCase(registry, "TP-PAR-340", testEmptySwitch);
    addCase(registry, "TP-PAR-341", testGroupedCaseLabels);
    addCase(registry, "TP-PAR-342", testDefaultSwitchLabel);
    addCase(registry, "TP-PAR-343", testDefaultExpressionInsideSection);
    addCase(registry, "TP-PAR-344", testImplicitSwitchFallthroughShape);
    addCase(registry, "TP-PAR-345", testBreakAndContinue);
    addCase(registry, "TP-PAR-346", testSimpleDivergence);
    addCase(registry, "TP-PAR-347", testConditionalDivergence);
    addCase(registry, "TP-PAR-348", testMalformedControlRecovery);
}
}
