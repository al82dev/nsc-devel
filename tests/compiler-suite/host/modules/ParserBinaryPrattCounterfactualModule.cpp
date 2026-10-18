#include "ParserExpressionTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::parser::scheduler_test_hooks
{
void resetPostfixPrimaryMetrics() noexcept;
std::size_t maximumPostfixPrimaryDepth() noexcept;
std::size_t unfinishedPostfixPrimaryResults() noexcept;
}

namespace nsc::suite
{
namespace
{
using namespace expression_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::TokenKind;
namespace scheduler_metrics = nativesharp::parser::scheduler_test_hooks;

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-565-golden", "TP-PAR-565-deep", "TP-PAR-565-boundary",
    "TP-PAR-565-recovery"};

bool schedulerClosed() noexcept
{
    return scheduler_metrics::maximumPostfixPrimaryDepth() == 1U &&
        scheduler_metrics::unfinishedPostfixPrimaryResults() == 0U;
}

AstNode* parseClosed(ParserFixture& fixture, bool& allSchedulersClosed)
{
    scheduler_metrics::resetPostfixPrimaryMetrics();
    AstNode* node = parse(fixture);
    allSchedulersClosed = allSchedulersClosed && schedulerClosed();
    return node;
}

bool isBinary(const AstNode* node, TokenKind operation) noexcept
{
    return node != nullptr && node->kind() == AstNodeKind::BinaryExpression &&
        node->tokenKind() == operation;
}

bool testGoldenPrecedence(std::string& detail)
{
    constexpr std::array<TokenKind, 10> operations{
        TokenKind::PipePipe, TokenKind::AmpAmp, TokenKind::Pipe, TokenKind::Caret,
        TokenKind::Amp, TokenKind::EqualEqual, TokenKind::Less, TokenKind::LessLess,
        TokenKind::Plus, TokenKind::Star};
    const std::string source = "a || b && c | d ^ e & f == g < h << i + j * k";
    ParserFixture fixture(source);
    bool closed = true;
    const AstNode* node = parseClosed(fixture, closed);
    bool treeMatches = true;
    for (const TokenKind operation : operations)
    {
        treeMatches = treeMatches && isBinary(node, operation) && node->first() != nullptr;
        if (!treeMatches) break;
        node = node->second();
    }
    treeMatches = treeMatches && node != nullptr &&
        node->kind() == AstNodeKind::IdentifierExpression && node->text() == "k";

    ParserFixture relations("value is Widget as object", {"Widget", "object"});
    const AstNode* safeCast = parseClosed(relations, closed);
    const AstNode* typeTest = safeCast != nullptr ? safeCast->first() : nullptr;
    const bool relationsMatch = safeCast != nullptr &&
        safeCast->kind() == AstNodeKind::SafeCastExpression &&
        safeCast->tokenKind() == TokenKind::KwAs && safeCast->typeSyntax() != nullptr &&
        safeCast->typeSyntax()->baseName() == "object" && typeTest != nullptr &&
        typeTest->kind() == AstNodeKind::TypeTestExpression &&
        typeTest->tokenKind() == TokenKind::KwIs && typeTest->typeSyntax() != nullptr &&
        typeTest->typeSyntax()->baseName() == "Widget";
    const bool passed = treeMatches && relationsMatch && closed &&
        fixture.parser().at(TokenKind::EndOfFile) &&
        relations.parser().at(TokenKind::EndOfFile) &&
        fixture.diagnostics().diagnostics().empty() &&
        relations.diagnostics().diagnostics().empty();
    if (!passed) detail = "binary precedence, is/as type RHS, cursor, or scheduler changed";
    return passed;
}

bool testDeepMixedPrecedence(std::string& detail)
{
    constexpr std::array<std::string_view, 10> operations{
        " || ", " && ", " | ", " ^ ", " & ", " == ", " < ", " << ", " + ", " * "};
    std::string source = "value";
    source.reserve(kDepth * 10U + 5U);
    for (std::size_t index = 0; index < kDepth; ++index)
    {
        source += operations[index % operations.size()];
        source += "value";
    }
    ParserFixture fixture(source);
    bool closed = true;
    const AstNode* root = parseClosed(fixture, closed);
    std::vector<const AstNode*> pending{root};
    std::size_t binaries = 0;
    std::size_t identifiers = 0;
    bool validKinds = true;
    while (!pending.empty() && validKinds)
    {
        const AstNode* node = pending.back();
        pending.pop_back();
        if (node != nullptr && node->kind() == AstNodeKind::BinaryExpression)
        {
            ++binaries;
            pending.push_back(node->first());
            pending.push_back(node->second());
        }
        else if (node != nullptr && node->kind() == AstNodeKind::IdentifierExpression)
            ++identifiers;
        else validKinds = false;
    }
    const bool passed = validKinds && binaries == kDepth && identifiers == kDepth + 1U &&
        root != nullptr && root->range().begin().offset() == 0U &&
        root->range().end().offset() == source.size() &&
        fixture.parser().at(TokenKind::EndOfFile) &&
        fixture.diagnostics().diagnostics().empty() && closed;
    if (!passed) detail = "32768 mixed operators changed AST cardinality, range, cursor, or scheduler";
    return passed;
}

bool testAssociativityAndBoundary(std::string& detail)
{
    bool closed = true;
    ParserFixture association("a - b - c");
    const AstNode* minus = parseClosed(association, closed);
    ParserFixture grouping("(a + b) * c");
    const AstNode* product = parseClosed(grouping, closed);
    ParserFixture boundary("a + b ?? c ? d : e = f");
    const AstNode* conditional = parseClosed(boundary, closed);
    const AstNode* coalescing = conditional != nullptr ? conditional->first() : nullptr;
    const AstNode* assignment = conditional != nullptr ? conditional->third() : nullptr;
    const bool passed = isBinary(minus, TokenKind::Minus) &&
        isBinary(minus->first(), TokenKind::Minus) && isBinary(product, TokenKind::Star) &&
        isBinary(product->first(), TokenKind::Plus) && conditional != nullptr &&
        conditional->kind() == AstNodeKind::ConditionalExpression && assignment != nullptr &&
        assignment->kind() == AstNodeKind::AssignmentExpression && coalescing != nullptr &&
        coalescing->kind() == AstNodeKind::NullCoalescingExpression &&
        isBinary(coalescing->first(), TokenKind::Plus) && closed &&
        association.diagnostics().diagnostics().empty() &&
        grouping.diagnostics().diagnostics().empty() &&
        boundary.diagnostics().diagnostics().empty();
    if (!passed) detail = "left association, grouping, or upper expression boundary changed";
    return passed;
}

bool testMissingRightHandSide(std::string& detail)
{
    constexpr std::array<std::string_view, 10> sources{
        "a ||", "a &&", "a |", "a ^", "a &", "a ==", "a <", "a <<", "a +", "a *"};
    bool closed = true;
    bool passed = true;
    for (const std::string_view source : sources)
    {
        ParserFixture fixture{std::string(source)};
        const AstNode* node = parseClosed(fixture, closed);
        passed = passed && node != nullptr && fixture.diagnostics().hasErrors() &&
            fixture.parser().at(TokenKind::EndOfFile) &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedExpression) != 0U;
    }
    passed = passed && closed;
    if (!passed) detail = "missing binary RHS changed diagnosis, cursor progress, or scheduler closure";
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testGoldenPrecedence(detail);
    if (probe == kProbes[1]) return testDeepMixedPrecedence(detail);
    if (probe == kProbes[2]) return testAssociativityAndBoundary(detail);
    if (probe == kProbes[3]) return testMissingRightHandSide(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testBinaryPrattCounterfactual(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed, "TP-PAR-565 binary Pratt contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-565", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-565 probe must pass " + std::string(mode) + "; exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserBinaryPrattCounterfactualModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-565", "CP-PAR-036", testBinaryPrattCounterfactual, "NIGHTLY");
}
}
