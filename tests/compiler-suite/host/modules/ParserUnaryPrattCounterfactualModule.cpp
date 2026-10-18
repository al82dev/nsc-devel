#include "ParserExpressionTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

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
    "TP-PAR-566-casts", "TP-PAR-566-delimited", "TP-PAR-566-contracts",
    "TP-PAR-566-recovery"};

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

bool testDeepCasts(std::string& detail)
{
    std::string source;
    source.reserve(kDepth * 5U + 5U);
    for (std::size_t depth = 0; depth < kDepth; ++depth) source += "(int)";
    source += "value";
    ParserFixture fixture(source);
    bool closed = true;
    AstNode* node = parseClosed(fixture, closed);
    for (std::size_t depth = 0; depth < kDepth && node != nullptr; ++depth)
    {
        if (node->kind() != AstNodeKind::CastExpression || node->typeSyntax() == nullptr ||
            node->typeSyntax()->baseName() != "int")
        {
            node = nullptr;
            break;
        }
        node = node->first();
    }
    const bool passed = node != nullptr && node->kind() == AstNodeKind::IdentifierExpression &&
        node->text() == "value" && fixture.parser().at(TokenKind::EndOfFile) &&
        fixture.diagnostics().diagnostics().empty() && closed;
    if (!passed) detail = "32768 parenthesized casts changed AST, cursor, or diagnostics";
    return passed;
}

bool testDeepDelimitedOperands(std::string& detail)
{
    std::string source;
    source.reserve(kDepth * 6U + 1U);
    for (std::size_t depth = 0; depth < kDepth; ++depth) source += "move(";
    source += '1';
    source.append(kDepth, ')');
    ParserFixture fixture(source);
    bool closed = true;
    AstNode* node = parseClosed(fixture, closed);
    for (std::size_t depth = 0; depth < kDepth && node != nullptr; ++depth)
    {
        if (node->kind() != AstNodeKind::MoveExpression)
        {
            node = nullptr;
            break;
        }
        node = node->first();
    }
    const bool passed = node != nullptr && node->kind() == AstNodeKind::IntegerLiteralExpression &&
        node->integerValue() == 1U && fixture.parser().at(TokenKind::EndOfFile) &&
        fixture.diagnostics().diagnostics().empty() && closed;
    if (!passed) detail = "32768 delimited operands reopened a recursive expression drain";
    return passed;
}

bool testUnaryContracts(std::string& detail)
{
    bool closed = true;
    ParserFixture chain("delete [] destroy (int)^!value");
    AstNode* deletion = parseClosed(chain, closed);
    ParserFixture named("static_cast<int>(value + 1)");
    AstNode* namedCast = parseClosed(named, closed);
    ParserFixture size("sizeof(value + 1)");
    AstNode* sizeNode = parseClosed(size, closed);
    ParserFixture alignment("alignof(int)");
    AstNode* alignmentNode = parseClosed(alignment, closed);
    ParserFixture stack("stackalloc int[value + 1]");
    AstNode* stackNode = parseClosed(stack, closed);
    ParserFixture placement("new (storage, trailing: count, failure: result) Item(1)", {"Item"});
    AstNode* placementNode = parseClosed(placement, closed);
    ParserFixture argument("new Item(ref value)", {"Item"});
    AstNode* argumentNode = parseClosed(argument, closed);
    const bool chainOk = deletion != nullptr && deletion->kind() == AstNodeKind::DeleteExpression &&
        deletion->flag() && deletion->first() != nullptr &&
        deletion->first()->kind() == AstNodeKind::DestroyExpression &&
        deletion->first()->first() != nullptr &&
        deletion->first()->first()->kind() == AstNodeKind::CastExpression;
    const bool formsOk = namedCast != nullptr && namedCast->kind() == AstNodeKind::CastExpression &&
        namedCast->auxiliaryText() == "cast:static_cast" && sizeNode != nullptr &&
        sizeNode->kind() == AstNodeKind::SizeofExpression && alignmentNode != nullptr &&
        alignmentNode->kind() == AstNodeKind::AlignofTypeExpression && stackNode != nullptr &&
        stackNode->kind() == AstNodeKind::StackallocExpression;
    const bool placementOk = placementNode != nullptr &&
        placementNode->kind() == AstNodeKind::PlacementConstructionExpression &&
        placementNode->auxiliaryText() == "trailing" &&
        placementNode->allocatorFailureProfile() == "result" &&
        placementNode->children().size() == 2U;
    const bool argumentOk = argumentNode != nullptr &&
        argumentNode->kind() == AstNodeKind::ObjectCreationExpression &&
        argumentNode->range().begin().offset() == 0U && argumentNode->children().size() == 1U &&
        argumentNode->children()[0]->auxiliaryText() == "pass:ref";
    const bool passed = chainOk && formsOk && placementOk && argumentOk && closed &&
        chain.diagnostics().diagnostics().empty() && named.diagnostics().diagnostics().empty() &&
        size.diagnostics().diagnostics().empty() && alignment.diagnostics().diagnostics().empty() &&
        stack.diagnostics().diagnostics().empty() && placement.diagnostics().diagnostics().empty() &&
        argument.diagnostics().diagnostics().empty();
    if (!passed) detail = "unary kinds, flags, types, metadata, children, or diagnostics changed";
    return passed;
}

bool testUnaryRecovery(std::string& detail)
{
    bool closed = true;
    ParserFixture move("move(value");
    ParserFixture cast("static_cast<int>(value");
    ParserFixture stack("stackalloc int[value");
    ParserFixture placement("new (storage, trailing: count) Item", {"Item"});
    static_cast<void>(parseClosed(move, closed));
    static_cast<void>(parseClosed(cast, closed));
    static_cast<void>(parseClosed(stack, closed));
    AstNode* placementNode = parseClosed(placement, closed);
    const bool passed = diagnosticCount(move.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        diagnosticCount(cast.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        diagnosticCount(stack.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        diagnosticCount(placement.diagnostics(), DiagnosticId::ExpectedToken) != 0U &&
        placementNode != nullptr && placementNode->children().size() == 1U && closed;
    if (!passed) detail = "unary recovery changed diagnostics or placement trailing child";
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepCasts(detail);
    if (probe == kProbes[1]) return testDeepDelimitedOperands(detail);
    if (probe == kProbes[2]) return testUnaryContracts(detail);
    if (probe == kProbes[3]) return testUnaryRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testUnaryPrattCounterfactual(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail), "TP-PAR-566 unary Pratt contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-566", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-566 probe must pass " + std::string(mode) + "; exit=" +
                std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserUnaryPrattCounterfactualModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-566", "CP-PAR-038", testUnaryPrattCounterfactual, "NIGHTLY");
}
}
