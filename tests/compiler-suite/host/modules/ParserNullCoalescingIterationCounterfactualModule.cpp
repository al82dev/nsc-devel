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
constexpr std::array<std::string_view, 3> kProbes{
    "TP-PAR-553-null-coalescing-depth",
    "TP-PAR-553-null-coalescing-contracts",
    "TP-PAR-553-range-contracts"};

bool testNullCoalescingDepth(std::string& detail)
{
    std::string source;
    source.reserve(kDepth * 14U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "value" + std::to_string(index) + " ?? ";
    source += "terminal";
    ParserFixture fixture(std::move(source));
    const AstNode* node = fixture.parser().parseExpression();
    for (std::size_t index = 0; index < kDepth && node != nullptr; ++index)
    {
        if (node->kind() != AstNodeKind::NullCoalescingExpression ||
            node->tokenKind() != TokenKind::QuestionQuestion || node->first() == nullptr ||
            node->first()->text() != "value" + std::to_string(index))
        {
            detail = "invalid null-coalescing node at depth " + std::to_string(index);
            return false;
        }
        node = node->second();
    }
    if (node != nullptr && node->text() == "terminal" &&
        fixture.diagnostics().diagnostics().empty()) return true;
    detail = "terminal or diagnostics mismatch";
    return false;
}

bool testNullCoalescingContracts(std::string& detail)
{
    ParserFixture fixture("left ?? middle ?? right");
    const AstNode* outer = fixture.parser().parseExpression();
    const AstNode* inner = outer != nullptr ? outer->second() : nullptr;
    if (outer != nullptr && outer->kind() == AstNodeKind::NullCoalescingExpression &&
        outer->first() != nullptr && outer->first()->text() == "left" && inner != nullptr &&
        inner->kind() == AstNodeKind::NullCoalescingExpression && inner->first() != nullptr &&
        inner->first()->text() == "middle" && inner->second() != nullptr &&
        inner->second()->text() == "right" &&
        outer->range().begin() == outer->first()->range().begin() &&
        outer->range().end() == inner->range().end() &&
        inner->range().begin() == inner->first()->range().begin() &&
        inner->range().end() == inner->second()->range().end() &&
        fixture.diagnostics().diagnostics().empty())
        return true;
    detail = "right associativity or AST contract changed";
    return false;
}

bool testRangeContracts(std::string& detail)
{
    ParserFixture valid("1..=9 step 2 ?? fallback ?? tail");
    const AstNode* root = valid.parser().parseExpression();
    const AstNode* range = root != nullptr ? root->first() : nullptr;
    const AstNode* nested = root != nullptr ? root->second() : nullptr;
    ParserFixture open("..10");
    const AstNode* openRange = open.parser().parseExpression();
    ParserFixture inclusive("1..=");
    inclusive.parser().parseExpression();
    ParserFixture missingStep("1..10 step ;");
    missingStep.parser().parseExpression();
    if (root != nullptr && root->kind() == AstNodeKind::NullCoalescingExpression &&
        range != nullptr && range->kind() == AstNodeKind::RangeExpression && range->flag() &&
        range->tokenKind() == TokenKind::DotDotEqual && range->first() != nullptr &&
        range->first()->text() == "1" && range->second() != nullptr &&
        range->second()->text() == "9" && range->third() != nullptr &&
        range->third()->text() == "2" && nested != nullptr &&
        nested->kind() == AstNodeKind::NullCoalescingExpression && openRange != nullptr &&
        openRange->kind() == AstNodeKind::RangeExpression && openRange->first() == nullptr &&
        openRange->second() != nullptr && openRange->second()->text() == "10" &&
        diagnosticCount(inclusive.diagnostics(), DiagnosticId::InvalidIndex) == 1U &&
        diagnosticCount(missingStep.diagnostics(), DiagnosticId::ExpectedExpression) == 1U)
        return true;
    detail = "range precedence, shape, or recovery changed";
    return false;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testNullCoalescingDepth(detail);
    if (probe == kProbes[1]) return testNullCoalescingContracts(detail);
    if (probe == kProbes[2]) return testRangeContracts(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testNullCoalescingIteration(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail),
            "TP-PAR-553 iterative null-coalescing must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-553", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-553 null-coalescing probe must pass " + std::string(mode) +
                "; started=" + std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

void registerParserNullCoalescingIterationCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-553",
        "CP-PAR-036",
        testNullCoalescingIteration,
        "NIGHTLY");
}
}
