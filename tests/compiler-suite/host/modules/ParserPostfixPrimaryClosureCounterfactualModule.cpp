#include "ParserExpressionTestSupport.h"
#include "ParserFinalTestSupport.h"
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
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::lexer::TokenKind;
namespace scheduler_metrics = nativesharp::parser::scheduler_test_hooks;

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-569-cross-family", "TP-PAR-569-contracts", "TP-PAR-569-recovery",
    "TP-PAR-569-entry"};

std::string makeCrossFamilySource()
{
    std::string source;
    std::vector<std::string_view> closes;
    closes.reserve(kDepth);
    for (std::size_t depth = 0; depth < kDepth; ++depth)
    {
        switch (depth % 6U)
        {
        case 0U: source += "call("; closes.push_back(")"); break;
        case 1U: source += '['; closes.push_back("]"); break;
        case 2U: source += "checked("; closes.push_back(")"); break;
        case 3U: source += "nameof("; closes.push_back(")"); break;
        case 4U: source += "box["; closes.push_back("]"); break;
        default: source += "seed with { Value = "; closes.push_back(" }"); break;
        }
    }
    source += "leaf";
    for (auto close = closes.rbegin(); close != closes.rend(); ++close) source += *close;
    return source;
}

AstNode* crossFamilyChild(AstNode* node, std::size_t kind)
{
    if (node == nullptr) return nullptr;
    if (kind == 0U && node->kind() == AstNodeKind::CallExpression &&
        node->children().size() == 1U) return node->children()[0];
    if (kind == 1U && node->kind() == AstNodeKind::CollectionExpression &&
        node->children().size() == 1U) return node->children()[0];
    if (kind == 2U && node->kind() == AstNodeKind::OverflowPolicyExpression)
        return node->first();
    if (kind == 3U && node->kind() == AstNodeKind::NameOfExpression) return node->first();
    if (kind == 4U && node->kind() == AstNodeKind::IndexExpression) return node->second();
    if (kind == 5U && node->kind() == AstNodeKind::WithExpression &&
        node->children().size() == 1U) return node->children()[0];
    return nullptr;
}

bool schedulerClosed() noexcept
{
    return scheduler_metrics::maximumPostfixPrimaryDepth() == 1U &&
        scheduler_metrics::unfinishedPostfixPrimaryResults() == 0U;
}

bool testCrossFamilyDepth(std::string& detail)
{
    scheduler_metrics::resetPostfixPrimaryMetrics();
    ParserFixture fixture(makeCrossFamilySource());
    AstNode* node = parse(fixture);
    for (std::size_t depth = 0; depth < kDepth && node != nullptr; ++depth)
        node = crossFamilyChild(node, depth % 6U);
    const bool passed = node != nullptr && node->kind() == AstNodeKind::IdentifierExpression &&
        node->text() == "leaf" && fixture.parser().at(TokenKind::EndOfFile) &&
        fixture.diagnostics().diagnostics().empty() && schedulerClosed();
    if (!passed) detail = "32768 primary/postfix crossings changed shape or nested the scheduler";
    return passed;
}

bool testIntegratedContracts(std::string& detail)
{
    scheduler_metrics::resetPostfixPrimaryMetrics();
    ParserFixture generic("Box<int>.Make<string>(named: value, ref target)");
    AstNode* genericRoot = parse(generic);
    ParserFixture questions("condition ? call([value]) : source?.member?[0](arg)?.tail++?");
    AstNode* questionRoot = parse(questions);
    const AstNode* call = firstKind(genericRoot, AstNodeKind::CallExpression);
    const bool genericOk = call != nullptr && call->typeSyntaxes().size() == 1U &&
        call->children().size() == 2U && call->children()[0]->argumentName() == "named" &&
        call->children()[1]->auxiliaryText() == "pass:ref";
    const bool questionOk = questionRoot != nullptr &&
        questionRoot->kind() == AstNodeKind::ConditionalExpression &&
        firstKind(questionRoot, AstNodeKind::ResultPropagationExpression) != nullptr &&
        firstKind(questionRoot, AstNodeKind::NullConditionalIndexExpression) != nullptr;
    const bool passed = genericOk && questionOk && generic.diagnostics().diagnostics().empty() &&
        questions.diagnostics().diagnostics().empty() && schedulerClosed();
    if (!passed) detail = "generic, argument, question-role, suffix, or scheduler contract changed";
    return passed;
}

bool malformedPreservesSentinel(std::string source)
{
    ParserFixture fixture(std::move(source));
    static_cast<void>(parse(fixture));
    return fixture.parser().at(TokenKind::Semicolon) &&
        diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) != 0U;
}

bool testIntegratedRecovery(std::string& detail)
{
    scheduler_metrics::resetPostfixPrimaryMetrics();
    const bool delimiters = malformedPreservesSentinel("call(value; sentinel") &&
        malformedPreservesSentinel("items[value; sentinel") &&
        malformedPreservesSentinel("[value; sentinel") &&
        malformedPreservesSentinel("seed with { Value = value; sentinel");
    ParserFixture member("source?.; sentinel");
    static_cast<void>(parse(member));
    ParserFixture unexpected(";");
    AstNode* error = parse(unexpected);
    const bool primary = member.parser().at(TokenKind::Semicolon) &&
        diagnosticCount(member.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        error != nullptr && unexpected.parser().at(TokenKind::EndOfFile) &&
        diagnosticCount(unexpected.diagnostics(), DiagnosticId::ExpectedExpression) == 1U;
    const bool passed = delimiters && primary && schedulerClosed();
    if (!passed) detail = "integrated recovery consumed a sentinel or left scheduler results open";
    return passed;
}

bool testDirectEntries(std::string& detail)
{
    scheduler_metrics::resetPostfixPrimaryMetrics();
    ParserFixture postfix("call([value]).member");
    AstNode* postfixRoot = postfix.parser().parsePostfixExpression();
    const bool postfixClosed = schedulerClosed();
    scheduler_metrics::resetPostfixPrimaryMetrics();
    ParserFixture primary("[value]");
    AstNode* primaryRoot = primary.parser().parsePrimaryExpression();
    const bool passed = postfixRoot != nullptr && primaryRoot != nullptr && postfixClosed &&
        schedulerClosed() && postfix.parser().at(TokenKind::EndOfFile) &&
        primary.parser().at(TokenKind::EndOfFile) && postfix.diagnostics().diagnostics().empty() &&
        primary.diagnostics().diagnostics().empty();
    if (!passed) detail = "direct postfix/primary entry opened nested schedulers or left slots pending";
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testCrossFamilyDepth(detail);
    if (probe == kProbes[1]) return testIntegratedContracts(detail);
    if (probe == kProbes[2]) return testIntegratedRecovery(detail);
    if (probe == kProbes[3]) return testDirectEntries(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testPostfixPrimaryClosure(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail), "TP-PAR-569 closure contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-569", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-569 probe must pass " + std::string(mode) + "; exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserPostfixPrimaryClosureCounterfactualModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-569", "CP-PAR-039", testPostfixPrimaryClosure, "NIGHTLY");
}
}
