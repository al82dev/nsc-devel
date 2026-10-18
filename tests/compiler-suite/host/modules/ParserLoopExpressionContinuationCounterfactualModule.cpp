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
constexpr std::array<std::string_view, 3> kProbes{
    "TP-PAR-570-positions", "TP-PAR-570-depth", "TP-PAR-570-recovery"};

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

bool testExpressionPositions(std::string& detail)
{
    ParserFixture fixture(
        "{ loop { consume(); }; var first = loop outer { consume(); }; "
        "first = loop { consume(); }; consume(loop { consume(); }); "
        "return ready ? (loop { consume(); }) : loop { consume(); }; }");
    const AstNode* const block = fixture.parser().parseBlock();
    std::size_t loops = 0;
    std::size_t labelled = 0;
    bool valid = block != nullptr && block->kind() == AstNodeKind::BlockStatement;
    for (const AstNode* node : descendants(block))
    {
        if (node->kind() != AstNodeKind::LoopExpression) continue;
        ++loops;
        valid = valid && node->first() != nullptr &&
            node->first()->kind() == AstNodeKind::BlockStatement && node->range().isValid();
        if (!node->text().empty())
        {
            ++labelled;
            valid = valid && node->text() == "outer";
        }
    }
    const bool passed = valid && loops == 6U && labelled == 1U &&
        fixture.parser().at(TokenKind::EndOfFile) &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed) detail = "loop positions, body, label, ranges, cursor, or diagnostics changed";
    return passed;
}

std::string makeDeepLoopSource()
{
    std::string source;
    source.reserve(kDepth * 10U + 16U);
    for (std::size_t index = 0; index < kDepth; ++index) source += "loop { ";
    source += "terminal();";
    for (std::size_t index = 0; index < kDepth; ++index) source += " };";
    return source;
}

bool testDeepLoop(std::string& detail)
{
    ParserFixture fixture(makeDeepLoopSource());
    const AstNode* statement = fixture.parser().parseStatement();
    for (std::size_t index = 0; index < kDepth; ++index)
    {
        const AstNode* loop = statement != nullptr &&
            statement->kind() == AstNodeKind::ExpressionStatement ? statement->first() : nullptr;
        const AstNode* body = loop != nullptr ? loop->first() : nullptr;
        statement = body != nullptr && body->kind() == AstNodeKind::BlockStatement &&
            body->children().size() == 1U ? body->children()[0] : nullptr;
        if (loop == nullptr || loop->kind() != AstNodeKind::LoopExpression ||
            !loop->range().isValid() || statement == nullptr)
        {
            detail = "invalid loop/block/statement edge at depth " + std::to_string(index);
            return false;
        }
    }
    const AstNode* terminal = statement != nullptr ? statement->first() : nullptr;
    const bool passed = statement != nullptr &&
        statement->kind() == AstNodeKind::ExpressionStatement && terminal != nullptr &&
        terminal->kind() == AstNodeKind::CallExpression &&
        fixture.parser().at(TokenKind::EndOfFile) &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed) detail = "deep loop terminal, cursor, or diagnostics changed";
    return passed;
}

bool testRecovery(std::string& detail)
{
    ParserFixture fixture("loop value;");
    const std::size_t before = fixture.parser().index_;
    const AstNode* const loop = fixture.parser().parseLoopExpression();
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    const bool passed = loop != nullptr && loop->kind() == AstNodeKind::LoopExpression &&
        loop->first() != nullptr && loop->first()->kind() == AstNodeKind::BlockStatement &&
        loop->text().empty() && loop->range().isValid() && fixture.parser().index_ > before &&
        fixture.parser().at(TokenKind::EndOfFile) && !diagnostics.empty() &&
        diagnostics.front().id() == DiagnosticId::ExpectedToken &&
        diagnostics.front().message() == "expected '{' after loop expression";
    if (!passed) detail = "direct loop recovery, first diagnostic, range, or cursor changed";
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testExpressionPositions(detail);
    if (probe == kProbes[1]) return testDeepLoop(detail);
    if (probe == kProbes[2]) return testRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testLoopExpressionContinuation(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed, "TP-PAR-570 loop continuation contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-570", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-570 probe must pass " + std::string(mode) + "; exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserLoopExpressionContinuationCounterfactualModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-570", "CP-PAR-033", testLoopExpressionContinuation, "NIGHTLY");
}
}
