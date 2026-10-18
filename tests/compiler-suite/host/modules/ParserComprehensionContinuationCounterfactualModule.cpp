#include "ParserExpressionTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace expression_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::TokenKind;

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-571-golden", "TP-PAR-571-lifo", "TP-PAR-571-depth", "TP-PAR-571-recovery"};

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
            if (child != nullptr) pending.push_back(child);
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

bool exactComprehensionShape(const AstNode* loop, std::string_view suffix)
{
    if (loop == nullptr || loop->kind() != AstNodeKind::LoopExpression ||
        loop->text() != "$__comprehension_loop_" + std::string(suffix) ||
        loop->auxiliaryText() != "comprehension:explicit-builder" || loop->first() == nullptr)
        return false;
    const AstNode* const loopBody = loop->first();
    if (loopBody->kind() != AstNodeKind::BlockStatement || loopBody->children().size() != 3U)
        return false;
    const AstNode* const declaration = loopBody->children()[0];
    const AstNode* const body = loopBody->children()[1];
    const AstNode* const finish = loopBody->children()[2];
    const AstNode* const result = finish != nullptr ? finish->first() : nullptr;
    return declaration != nullptr &&
        declaration->text() == "$__comprehension_builder_" + std::string(suffix) &&
        declaration->auxiliaryText() == "local:comprehension-builder" &&
        body != nullptr && body->kind() == AstNodeKind::BlockStatement &&
        finish != nullptr && finish->kind() == AstNodeKind::BreakStatement &&
        finish->text() == loop->text() && result != nullptr &&
        result->kind() == AstNodeKind::MoveExpression &&
        result->auxiliaryText() == "comprehension-result" && result->first() != nullptr &&
        result->first()->text() == declaration->text();
}

bool testGolden(std::string& detail)
{
    ParserFixture minimal("comprehend(createBuilder()) { yield return value; yield break; }");
    const AstNode* const loop = minimal.parser().parseExpression();
    ParserFixture outside("yield return value;");
    const AstNode* const ordinaryYield = outside.parser().parseStatement();
    ParserFixture control(
        "comprehend(builder) { for (var i = 0; i < 4; i = i + 1) { "
        "if (ready) { yield i; } } }");
    const AstNode* const controlled = control.parser().parseExpression();
    const bool passed = exactComprehensionShape(loop, "0") &&
        countText(loop, "createBuilder") == 1U &&
        countAuxiliary(loop, "comprehension-builder-add") == 1U &&
        countKind(loop, AstNodeKind::BreakStatement) == 2U &&
        ordinaryYield != nullptr && ordinaryYield->kind() == AstNodeKind::YieldReturnStatement &&
        controlled != nullptr && countKind(controlled, AstNodeKind::ForStatement) == 1U &&
        countKind(controlled, AstNodeKind::IfStatement) == 1U &&
        countAuxiliary(controlled, "comprehension-builder-add") == 1U &&
        minimal.parser().at(TokenKind::EndOfFile) && outside.parser().at(TokenKind::EndOfFile) &&
        control.parser().at(TokenKind::EndOfFile) && minimal.diagnostics().diagnostics().empty() &&
        outside.diagnostics().diagnostics().empty() && control.diagnostics().diagnostics().empty();
    if (!passed) detail = "golden AST, yield routing, control tree, builder count, or cursor changed";
    return passed;
}

bool testLifoAndOrdinals(std::string& detail)
{
    ParserFixture fixture(
        "comprehend(comprehend(inner) { yield return 1; }) { "
        "var nested = comprehend(second) { yield return 2; }; yield return nested; }");
    const AstNode* const root = fixture.parser().parseExpression();
    const bool passed = root != nullptr && exactComprehensionShape(root, "1") &&
        countKind(root, AstNodeKind::LoopExpression) == 3U &&
        countText(root, "$__comprehension_builder_0") >= 2U &&
        countText(root, "$__comprehension_builder_1") >= 2U &&
        countText(root, "$__comprehension_builder_2") >= 2U &&
        countAuxiliary(root, "comprehension-builder-add") == 3U &&
        fixture.parser().comprehensionContexts_.empty() &&
        fixture.parser().at(TokenKind::EndOfFile) && fixture.diagnostics().diagnostics().empty();
    if (!passed) detail = "builder-first ordinal order, nested LIFO context, or names changed";
    return passed;
}

std::string makeDeepComprehensionSource()
{
    std::string source;
    source.reserve(kDepth * 27U + 32U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "comprehend(builder) { ";
    source += "yield return terminal;";
    for (std::size_t index = 1; index < kDepth; ++index) source += " };";
    source += " }";
    return source;
}

bool testDeepComprehension(std::string& detail)
{
    ParserFixture fixture(makeDeepComprehensionSource());
    const AstNode* loop = fixture.parser().parseExpression();
    for (std::size_t index = 0; index < kDepth; ++index)
    {
        if (!exactComprehensionShape(loop, std::to_string(index)))
        {
            detail = "invalid comprehension shape at depth " + std::to_string(index);
            return false;
        }
        const AstNode* const body = loop->first()->children()[1];
        const AstNode* const statement = body->children().empty() ? nullptr : body->children()[0];
        loop = statement != nullptr ? statement->first() : nullptr;
    }
    const bool passed = loop != nullptr && loop->kind() == AstNodeKind::CallExpression &&
        loop->auxiliaryText() == "comprehension-builder-add" &&
        fixture.parser().comprehensionContexts_.empty() &&
        fixture.parser().at(TokenKind::EndOfFile) && fixture.diagnostics().diagnostics().empty();
    if (!passed) detail = "deep terminal, context cleanup, cursor, or diagnostics changed";
    return passed;
}

bool testRecovery(std::string& detail)
{
    ParserFixture fixture("comprehend(inner) comprehend(next) {}");
    fixture.parser().comprehensionContexts_.push_back(
        Parser::ComprehensionContext{"outer-builder", "outer-loop"});
    const AstNode* const malformed = fixture.parser().parseComprehensionExpression();
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    const bool restored = malformed != nullptr && exactComprehensionShape(malformed, "0") &&
        fixture.parser().comprehensionContexts_.size() == 1U &&
        fixture.parser().comprehensionContexts_.back().builderName == "outer-builder" &&
        fixture.parser().comprehensionContexts_.back().loopLabel == "outer-loop" &&
        !diagnostics.empty() && diagnostics.front().id() == DiagnosticId::ExpectedToken &&
        diagnostics.front().message() == "expected '{' after comprehension builder";
    const AstNode* const next = fixture.parser().parseExpression();
    const bool passed = restored && exactComprehensionShape(next, "1") &&
        fixture.parser().comprehensionContexts_.size() == 1U &&
        fixture.parser().at(TokenKind::EndOfFile);
    fixture.parser().comprehensionContexts_.clear();
    if (!passed) detail = "recovery diagnostic, outer context, monotonic name, or next parse changed";
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testGolden(detail);
    if (probe == kProbes[1]) return testLifoAndOrdinals(detail);
    if (probe == kProbes[2]) return testDeepComprehension(detail);
    if (probe == kProbes[3]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testComprehensionContinuation(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed, "TP-PAR-571 comprehension continuation contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-571", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-571 probe must pass " + std::string(mode) + "; exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserComprehensionContinuationCounterfactualModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-571", "CP-PAR-033", testComprehensionContinuation, "NIGHTLY");
}
}
