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

void addLoopCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CP-PAR-033", "Host.Parser", "component", "PR",
        {"parser", "loop", "comprehension"}, std::move(function)});
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

std::size_t countAuxiliary(const AstNode* root, std::string_view text)
{
    std::size_t count = 0;
    for (const AstNode* node : descendants(root))
        count += node->auxiliaryText() == text ? 1U : 0U;
    return count;
}

void testLoopBlock(TestContext& context)
{
    ParserFixture fixture("loop { consume(); }");
    AstNode* const node = fixture.parser().parseLoopExpression();
    context.expect(node != nullptr && node->kind() == AstNodeKind::LoopExpression &&
            node->first() != nullptr && node->first()->kind() == AstNodeKind::BlockStatement &&
            countText(node, "consume") == 1 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-385 loop expression must retain its block body");
}

void testMalformedLoop(TestContext& context)
{
    ParserFixture fixture("loop value;");
    const std::size_t before = fixture.parser().index_;
    AstNode* const node = fixture.parser().parseLoopExpression();
    context.expect(node != nullptr,
        "TP-PAR-386 malformed loop expression must still return a recovery node");
    context.expect(node != nullptr && node->kind() == AstNodeKind::LoopExpression,
        "TP-PAR-386 recovery node must retain LoopExpression identity");
    context.expect(fixture.parser().index_ > before,
        "TP-PAR-386 malformed loop expression must advance the cursor");
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-386 malformed loop expression must report ExpectedToken");
}

void testMinimalComprehension(TestContext& context)
{
    ParserFixture fixture("comprehend(builder) {}");
    AstNode* const node = fixture.parser().parseExpression();
    context.expect(node != nullptr && node->kind() == AstNodeKind::LoopExpression &&
            node->text() == "$__comprehension_loop_0" &&
            node->auxiliaryText() == "comprehension:explicit-builder" &&
            node->first() != nullptr && node->first()->children().size() == 3 &&
            node->first()->children()[0]->text() == "$__comprehension_builder_0" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-387 comprehension must create exact builder context, body, and result exit");
}

void testYieldInsideComprehension(TestContext& context)
{
    ParserFixture fixture("comprehend(builder) { yield return value; }");
    AstNode* const node = fixture.parser().parseExpression();
    context.expect(node != nullptr && countAuxiliary(node, "comprehension-builder-add") == 1 &&
            countText(node, "$__comprehension_builder_0") >= 2 &&
            countText(node, "value") == 1 && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-388 yield inside comprehension must emit one builder Add call");
}

void testYieldOutsideComprehension(TestContext& context)
{
    ParserFixture fixture("yield return value;");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr && node->kind() == AstNodeKind::YieldReturnStatement &&
            node->first() != nullptr && node->first()->text() == "value" &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-389 yield outside a comprehension must parse as a YieldReturnStatement");
}

void testNestedComprehensionContexts(TestContext& context)
{
    ParserFixture fixture(
        "comprehend(outer) { var nested = comprehend(inner) { yield return 1; }; "
        "yield return nested; }");
    AstNode* const node = fixture.parser().parseExpression();
    context.expect(node != nullptr && countKind(node, AstNodeKind::LoopExpression) == 2 &&
            countText(node, "$__comprehension_builder_0") >= 2 &&
            countText(node, "$__comprehension_builder_1") >= 2 &&
            countAuxiliary(node, "comprehension-builder-add") == 2 &&
            fixture.parser().comprehensionContexts_.empty(),
        "TP-PAR-390 nested comprehensions must bind yields to LIFO contexts");
}

void testContextRestoredAfterEarlyError(TestContext& context)
{
    ParserFixture fixture("comprehend(inner)");
    fixture.parser().comprehensionContexts_.push_back(
        Parser::ComprehensionContext{"outer-builder", "outer-loop"});
    AstNode* const node = fixture.parser().parseComprehensionExpression();
    context.expect(node != nullptr,
        "TP-PAR-391 malformed nested comprehension must still return a recovery node");
    context.expect(fixture.parser().comprehensionContexts_.size() == 1 &&
            fixture.parser().comprehensionContexts_.front().builderName == "outer-builder" &&
            fixture.parser().comprehensionContexts_.front().loopLabel == "outer-loop",
        "TP-PAR-391 malformed nested comprehension must restore its enclosing context");
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-391 malformed nested comprehension must report ExpectedToken");
    fixture.parser().comprehensionContexts_.clear();
}

void testMonotonicComprehensionNames(TestContext& context)
{
    ParserFixture fixture("comprehend(first) {} comprehend(second) {}");
    AstNode* const first = fixture.parser().parseExpression();
    AstNode* const second = fixture.parser().parseExpression();
    context.expect(first != nullptr && second != nullptr &&
            first->text() == "$__comprehension_loop_0" &&
            second->text() == "$__comprehension_loop_1" &&
            countText(first, "$__comprehension_builder_0") >= 2 &&
            countText(second, "$__comprehension_builder_1") >= 2,
        "TP-PAR-392 consecutive comprehensions must use monotonic synthetic names");
}

void testComprehensionControlTree(TestContext& context)
{
    ParserFixture fixture(
        "comprehend(builder) { for (var i = 0; i < 4; i = i + 1) { "
        "if (ready) { yield return i; } } }");
    AstNode* const node = fixture.parser().parseExpression();
    context.expect(node != nullptr && countKind(node, AstNodeKind::ForStatement) == 1 &&
            countKind(node, AstNodeKind::IfStatement) == 1 &&
            countAuxiliary(node, "comprehension-builder-add") == 1 &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-393 comprehension must preserve nested for/if control flow");
}

void testBuilderSideEffectOnce(TestContext& context)
{
    ParserFixture fixture("comprehend(createBuilder()) { yield return 1; }");
    AstNode* const node = fixture.parser().parseExpression();
    context.expect(node != nullptr && countText(node, "createBuilder") == 1 &&
            countAuxiliary(node, "comprehension-builder-add") == 1,
        "TP-PAR-394 builder expression with side effects must appear exactly once");
}
}

void registerParserLoopComprehensionModule(TestRegistry& registry)
{
    addLoopCase(registry, "TP-PAR-385", testLoopBlock);
    addLoopCase(registry, "TP-PAR-386", testMalformedLoop);
    addLoopCase(registry, "TP-PAR-387", testMinimalComprehension);
    addLoopCase(registry, "TP-PAR-388", testYieldInsideComprehension);
    addLoopCase(registry, "TP-PAR-389", testYieldOutsideComprehension);
    addLoopCase(registry, "TP-PAR-390", testNestedComprehensionContexts);
    addLoopCase(registry, "TP-PAR-391", testContextRestoredAfterEarlyError);
    addLoopCase(registry, "TP-PAR-392", testMonotonicComprehensionNames);
    addLoopCase(registry, "TP-PAR-393", testComprehensionControlTree);
    addLoopCase(registry, "TP-PAR-394", testBuilderSideEffectOnce);
}
}
