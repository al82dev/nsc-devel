#include "ParserFinalTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-555-lambda-block-return-depth",
    "TP-PAR-555-lambda-block-expression-depth",
    "TP-PAR-555-simple-statement-contracts",
    "TP-PAR-555-recovery-and-defer-boundary"};

// Builds an adversarial linear token stream without recursive fixture construction.
std::string makeDeepLambdaBlockSource(bool returnStatement)
{
    std::string source;
    source.reserve(kDepth * 30U);
    for (std::size_t index = 0; index < kDepth; ++index)
    {
        source += "handler" + std::to_string(index) + " => { ";
        if (returnStatement) source += "return ";
    }
    source += "terminal";
    for (std::size_t index = 0; index < kDepth; ++index) source += "; }";
    return source;
}

// Walks the produced AST iteratively and checks every lambda, block, statement, and terminal.
bool validateDeepLambdaBlock(
    const AstNode* node,
    AstNodeKind statementKind,
    std::string& detail)
{
    for (std::size_t index = 0; index < kDepth; ++index)
    {
        if (node == nullptr || node->kind() != AstNodeKind::LambdaExpression || !node->flag() ||
            !node->range().isValid())
        {
            detail = "invalid lambda at depth " + std::to_string(index);
            return false;
        }
        const AstNode* const block = node->first();
        const AstNode* statement = block != nullptr && block->children().size() == 1U
            ? block->children()[0] : nullptr;
        if (block == nullptr || block->kind() != AstNodeKind::BlockStatement ||
            !block->range().isValid() || statement == nullptr || statement->kind() != statementKind ||
            !statement->range().isValid())
        {
            detail = "invalid block/statement at depth " + std::to_string(index);
            return false;
        }
        node = statement->first();
    }
    if (node != nullptr && node->kind() == AstNodeKind::IdentifierExpression &&
        node->text() == "terminal") return true;
    detail = "deep lambda terminal mismatch";
    return false;
}

// Parses one 32,768-layer route and requires exact structure with no diagnostics.
bool testDeepLambdaBlock(bool returnStatement, std::string& detail)
{
    ParserFixture fixture(makeDeepLambdaBlockSource(returnStatement));
    const AstNode* const root = fixture.parser().parseExpression();
    const AstNodeKind statementKind = returnStatement
        ? AstNodeKind::ReturnStatement : AstNodeKind::ExpressionStatement;
    if (!validateDeepLambdaBlock(root, statementKind, detail)) return false;
    if (fixture.diagnostics().diagnostics().empty()) return true;
    detail = "valid deep lambda-block emitted diagnostics";
    return false;
}

// Verifies the ordered AST contract for all statements owned by the B leaf.
bool validSimpleStatementShape(const AstNode* block)
{
    if (block == nullptr || block->kind() != AstNodeKind::BlockStatement ||
        block->children().size() != 9U) return false;
    const auto& nodes = block->children();
    for (const AstNode* node : nodes)
        if (node == nullptr || !node->range().isValid()) return false;
    return nodes[0]->kind() == AstNodeKind::BlockStatement &&
        nodes[1]->kind() == AstNodeKind::ReturnStatement && nodes[1]->isReference() &&
        nodes[2]->kind() == AstNodeKind::ThrowStatement &&
        nodes[3]->kind() == AstNodeKind::BreakStatement && nodes[3]->text() == "label" &&
        nodes[4]->kind() == AstNodeKind::ContinueStatement && nodes[4]->text() == "label" &&
        nodes[5]->kind() == AstNodeKind::YieldReturnStatement &&
        nodes[6]->kind() == AstNodeKind::YieldBreakStatement &&
        nodes[7]->kind() == AstNodeKind::SuspendStatement &&
        nodes[8]->kind() == AstNodeKind::StaticAssertStatement &&
        nodes[8]->text() == "message";
}

// Covers flags, labels, payloads, message text, nested blocks, and valid ranges.
bool testSimpleStatementContracts(std::string& detail)
{
    ParserFixture fixture(
        "handler => { { value; } return ref source; throw error; "
        "break label: value; continue label; yield return value; yield break; yield; "
        "static_assert(condition, \"message\"); }");
    const AstNode* const lambda = fixture.parser().parseExpression();
    const AstNode* const block = lambda != nullptr ? lambda->first() : nullptr;
    if (lambda != nullptr && lambda->kind() == AstNodeKind::LambdaExpression && lambda->flag() &&
        validSimpleStatementShape(block) && fixture.diagnostics().diagnostics().empty()) return true;
    detail = "simple statement AST, flags, labels, message, or diagnostics changed";
    return false;
}

// Requires finite recovery and confirms that defer stays delegated to its canonical lowering.
bool testRecoveryAndDeferBoundary(std::string& detail)
{
    ParserFixture missingSemicolon("handler => { value }");
    const AstNode* first = missingSemicolon.parser().parseExpression();
    ParserFixture missingBrace("handler => { return value;");
    const AstNode* second = missingBrace.parser().parseExpression();
    ParserFixture defer("handler => { defer [] { cleanup(); } value; }");
    const AstNode* third = defer.parser().parseExpression();
    const bool recovered = first != nullptr && first->range().isValid() &&
        second != nullptr && second->range().isValid() &&
        !missingSemicolon.diagnostics().diagnostics().empty() &&
        !missingBrace.diagnostics().diagnostics().empty();
    const bool preservedDefer = third != nullptr && third->range().isValid() &&
        firstKind(third, AstNodeKind::TryStatement) != nullptr &&
        countAuxiliary(third, "defer-lowered") == 1U &&
        defer.diagnostics().diagnostics().empty();
    if (recovered && preservedDefer) return true;
    detail = "recovery termination, diagnostic, range, or shallow defer lowering changed";
    return false;
}

// Maps an isolated probe name to exactly one bounded counterfactual.
bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepLambdaBlock(true, detail);
    if (probe == kProbes[1]) return testDeepLambdaBlock(false, detail);
    if (probe == kProbes[2]) return testSimpleStatementContracts(detail);
    if (probe == kProbes[3]) return testRecoveryAndDeferBoundary(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

// Runs every deep probe out of process so a stack regression cannot terminate the host suite.
void testLambdaBlockContinuation(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail),
            "TP-PAR-555 lambda-block continuation must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-555", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-555 lambda-block probe must pass " + std::string(mode) +
                "; started=" + std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

// Registers TP-PAR-555 in the canonical Host.Parser catalog.
void registerParserLambdaBlockContinuationCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-555",
        "CP-PAR-036",
        testLambdaBlockContinuation,
        "NIGHTLY");
}
}
