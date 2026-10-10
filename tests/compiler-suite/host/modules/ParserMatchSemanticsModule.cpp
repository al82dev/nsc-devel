#include "ParserFoundationTestSupport.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addMatchCase(
    TestRegistry& registry,
    std::string id,
    TestFunction function,
    std::string profile = "PR")
{
    registry.add(TestCase{std::move(id), "CP-PAR-032", "Host.Parser", "component",
        std::move(profile), {"parser", "match"}, std::move(function)});
}

std::vector<const AstNode*> descendants(const AstNode* root)
{
    std::vector<const AstNode*> result;
    std::vector<const AstNode*> pending;
    if (root != nullptr) pending.push_back(root);
    while (!pending.empty())
    {
        const AstNode* const node = pending.back();
        pending.pop_back();
        result.push_back(node);
        if (node->first() != nullptr) pending.push_back(node->first());
        if (node->second() != nullptr) pending.push_back(node->second());
        if (node->third() != nullptr) pending.push_back(node->third());
        for (const AstNode* child : node->children())
        {
            if (child != nullptr) pending.push_back(child);
        }
    }
    return result;
}

std::size_t countKind(const AstNode* root, AstNodeKind kind)
{
    std::size_t count = 0;
    for (const AstNode* node : descendants(root)) count += node->kind() == kind ? 1U : 0U;
    return count;
}

std::size_t countText(const AstNode* root, std::string_view text)
{
    std::size_t count = 0;
    for (const AstNode* node : descendants(root)) count += node->text() == text ? 1U : 0U;
    return count;
}

void testSingleStatementArm(TestContext& context)
{
    ParserFixture fixture("match (value) { Some => { consume(); } }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && node->kind() == AstNodeKind::BlockStatement &&
            node->text() == "$__nsc_match_0" && node->children().size() == 3 &&
            node->children()[0]->kind() == AstNodeKind::VariableDeclarationStatement &&
            node->children()[0]->first() != nullptr &&
            node->children()[0]->first()->text() == "value" &&
            node->children()[2]->kind() == AstNodeKind::IfStatement &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-369 one statement arm must lower source, selected flag, test, and body");
}

void testSingleExpressionArm(TestContext& context)
{
    ParserFixture fixture("match(value) { Some => 42 }");
    AstNode* const node = fixture.parser().parseExpression();
    context.expect(node != nullptr && node->kind() == AstNodeKind::MatchExpression &&
            node->text() == "$__nsc_match_expr_0" && node->first() != nullptr &&
            node->first()->text() == "value" && node->children().size() == 1 &&
            node->children()[0]->kind() == AstNodeKind::MatchArmExpression &&
            node->children()[0]->third() != nullptr &&
            node->children()[0]->third()->integerValue() == 42,
        "TP-PAR-370 one expression arm must preserve governing value and result expression");
}

void testStatementArmOrder(TestContext& context)
{
    ParserFixture fixture("match (value) { A => { first(); }, B => { second(); } }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && node->children().size() == 4 &&
            node->children()[2]->kind() == AstNodeKind::IfStatement &&
            node->children()[3]->kind() == AstNodeKind::IfStatement &&
            node->auxiliaryText() == "match-coverage:v1:A;B" &&
            countText(node->children()[2], "first") == 1 &&
            countText(node->children()[3], "second") == 1,
        "TP-PAR-371 match statement arms must retain source decision order");
}

void testGuardAfterBindings(TestContext& context)
{
    ParserFixture fixture(
        "match (value) { Some(var payload) when ready => { consume(payload); } }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && countText(node, "payload") >= 2 &&
            countText(node, "ready") == 1 && countKind(node, AstNodeKind::IfStatement) >= 2 &&
            node->auxiliaryText() == "match-coverage:v1:~Some" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-372 guarded arm must bind payload before evaluating its guard");
}

void testFinalCatchAll(TestContext& context)
{
    ParserFixture fixture("match (value) { A => {}, _ => {} }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && node->auxiliaryText() == "match-coverage:v1:A;*" &&
            node->children().size() == 4 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-373 final wildcard must lower to the exact terminal catch-all branch");
}

void testArmAfterCatchAll(TestContext& context)
{
    ParserFixture fixture("match (value) { _ => {}, A => {} }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidOperand) == 1,
        "TP-PAR-374 arm after catch-all must be diagnosed as unreachable");
}

void testDuplicateCatchAll(TestContext& context)
{
    ParserFixture fixture("match (value) { _ => {}, var rest => {} }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidOperand) == 1,
        "TP-PAR-375 second catch-all must be diagnosed as unreachable");
}

void testRangeOverlap(TestContext& context)
{
    ParserFixture disjoint("match (value) { 1..4 => {}, 4..7 => {} }");
    ParserFixture overlap("match (value) { 1..4 => {}, 3..7 => {} }");
    AstNode* const first = disjoint.parser().parseStatement();
    AstNode* const second = overlap.parser().parseStatement();
    context.expect(first != nullptr && disjoint.diagnostics().diagnostics().empty() &&
            second != nullptr &&
            diagnosticCount(overlap.diagnostics(), DiagnosticId::InvalidOperand) == 1,
        "TP-PAR-376 disjoint ranges must pass and overlapping ranges must diagnose");
}

void testOrAlternatives(TestContext& context)
{
    ParserFixture fixture(
        "match (next()) { Some(var value) | Other(var value) => { consume(value); } }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && countText(node, "next") == 1 &&
            countText(node, "$__nsc_match_0") >= 3 &&
            node->auxiliaryText() == "match-coverage:v1:Other|Some" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-377 OR alternatives must share one governing evaluation");
}

void testVariantPayloadExtraction(TestContext& context)
{
    ParserFixture fixture("match (value) { Some(var payload) => { consume(payload); } }");
    AstNode* const node = fixture.parser().parseStatement();
    bool foundExtract = false;
    for (const AstNode* current : descendants(node))
        foundExtract = foundExtract || current->auxiliaryText() == "variant-pattern-extract";
    context.expect(node != nullptr && foundExtract && countText(node, "payload") >= 2 &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-378 variant payload arm must emit one extractor and its binding");
}

void testGoverningSideEffectOnce(TestContext& context)
{
    ParserFixture fixture("match (advance()) { A => {}, B => {} }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && countText(node, "advance") == 1,
        "TP-PAR-379 match statement must evaluate a side-effecting scrutinee exactly once");
}

void testExpressionCoverageMarker(TestContext& context)
{
    ParserFixture fixture("match(value) { Some => 1 }");
    AstNode* const node = fixture.parser().parseExpression();
    context.expect(node != nullptr && node->kind() == AstNodeKind::MatchExpression &&
            node->auxiliaryText() == "match-coverage:v1:Some",
        "TP-PAR-380 non-fallback match expression must retain semantic coverage metadata");
}

void testMissingArmArrowOrBody(TestContext& context)
{
    ParserFixture missingArrow("match (value) { A {} }");
    ParserFixture missingBody("match (value) { A => value; }");
    AstNode* const first = missingArrow.parser().parseStatement();
    AstNode* const second = missingBody.parser().parseStatement();
    context.expect(first != nullptr && second != nullptr &&
            diagnosticCount(missingArrow.diagnostics(), DiagnosticId::ExpectedToken) >= 1 &&
            diagnosticCount(missingBody.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-381 malformed arms must diagnose and leave a stable lowered statement");
}

void testEmptyMatch(TestContext& context)
{
    ParserFixture fixture("match (value) {}");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && node->children().size() == 2 &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidOperand) == 1,
        "TP-PAR-382 empty match must retain lowering shell and exact semantic error");
}

void testConsecutiveMatchTemporaries(TestContext& context)
{
    ParserFixture fixture("match (first) { _ => {} } match (second) { _ => {} }");
    AstNode* const first = fixture.parser().parseStatement();
    AstNode* const second = fixture.parser().parseStatement();
    context.expect(first != nullptr && second != nullptr &&
            first->text() == "$__nsc_match_0" && second->text() == "$__nsc_match_1" &&
            countText(first, "first") == 1 && countText(second, "second") == 1,
        "TP-PAR-383 consecutive matches must use independent monotonic temporaries");
}

void testDeepMatchNesting(TestContext& context)
{
    std::string source;
    constexpr std::size_t depth = 16;
    for (std::size_t index = 0; index < depth; ++index)
        source += "match (value) { _ => { ";
    source += "consume();";
    for (std::size_t index = 0; index < depth; ++index) source += " } }";
    ParserFixture fixture(std::move(source));
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && countKind(node, AstNodeKind::IfStatement) == depth &&
            countText(node, "consume") == 1 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-384 deeply nested matches must preserve every branch and source range");
}
}

void registerParserMatchSemanticsModule(TestRegistry& registry)
{
    addMatchCase(registry, "TP-PAR-369", testSingleStatementArm);
    addMatchCase(registry, "TP-PAR-370", testSingleExpressionArm);
    addMatchCase(registry, "TP-PAR-371", testStatementArmOrder);
    addMatchCase(registry, "TP-PAR-372", testGuardAfterBindings);
    addMatchCase(registry, "TP-PAR-373", testFinalCatchAll);
    addMatchCase(registry, "TP-PAR-374", testArmAfterCatchAll);
    addMatchCase(registry, "TP-PAR-375", testDuplicateCatchAll);
    addMatchCase(registry, "TP-PAR-376", testRangeOverlap);
    addMatchCase(registry, "TP-PAR-377", testOrAlternatives);
    addMatchCase(registry, "TP-PAR-378", testVariantPayloadExtraction);
    addMatchCase(registry, "TP-PAR-379", testGoverningSideEffectOnce);
    addMatchCase(registry, "TP-PAR-380", testExpressionCoverageMarker);
    addMatchCase(registry, "TP-PAR-381", testMissingArmArrowOrBody);
    addMatchCase(registry, "TP-PAR-382", testEmptyMatch);
    addMatchCase(registry, "TP-PAR-383", testConsecutiveMatchTemporaries);
    addMatchCase(registry, "TP-PAR-384", testDeepMatchNesting, "NIGHTLY");
}
}
