#include "ParserFinalTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;

constexpr std::size_t kDepth = 32768;
constexpr std::array<std::string_view, 3> kProbes{
    "TP-PAR-551-assignment-depth",
    "TP-PAR-551-lambda-depth",
    "TP-PAR-551-contracts"};

bool testAssignmentDepth(std::string& detail)
{
    std::string source;
    source.reserve(kDepth * 12U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "value" + std::to_string(index) + " = ";
    source += "terminal";
    ParserFixture fixture(source);
    const AstNode* node = fixture.parser().parseExpression();
    for (std::size_t index = 0; index < kDepth && node != nullptr; ++index)
    {
        if (node->kind() != AstNodeKind::AssignmentExpression ||
            node->tokenKind() != TokenKind::Equal || node->first() == nullptr ||
            node->first()->text() != "value" + std::to_string(index))
        {
            detail = "invalid assignment at depth " + std::to_string(index);
            return false;
        }
        node = node->second();
    }
    if (node != nullptr && node->text() == "terminal" &&
        fixture.diagnostics().diagnostics().empty()) return true;
    detail = "terminal or diagnostics mismatch";
    return false;
}

bool testLambdaDepth(std::string& detail)
{
    std::string source;
    source.reserve(kDepth * 12U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "value" + std::to_string(index) + " => ";
    source += "terminal";
    ParserFixture fixture(source);
    const AstNode* node = fixture.parser().parseExpression();
    for (std::size_t index = 0; index < kDepth && node != nullptr; ++index)
    {
        if (node->kind() != AstNodeKind::LambdaExpression || node->flag() ||
            node->children().size() != 1U ||
            node->children().front()->text() != "value" + std::to_string(index))
        {
            detail = "invalid lambda at depth " + std::to_string(index);
            return false;
        }
        node = node->first();
    }
    if (node != nullptr && node->text() == "terminal" &&
        fixture.diagnostics().diagnostics().empty()) return true;
    detail = "lambda terminal or diagnostics mismatch";
    return false;
}

bool testAssignmentContracts(std::string& detail)
{
    constexpr std::array<TokenKind, 12> operators{
        TokenKind::Equal, TokenKind::PlusEqual, TokenKind::MinusEqual,
        TokenKind::StarEqual, TokenKind::SlashEqual, TokenKind::PercentEqual,
        TokenKind::AmpEqual, TokenKind::PipeEqual, TokenKind::CaretEqual,
        TokenKind::LessLessEqual, TokenKind::GreaterGreaterEqual,
        TokenKind::QuestionQuestionEqual};
    ParserFixture mixed(
        "a = b += c -= d *= e /= f %= g &= h |= i ^= j <<= k >>= l ?" "?= terminal");
    const AstNode* node = mixed.parser().parseExpression();
    for (std::size_t index = 0; index < operators.size() && node != nullptr; ++index)
    {
        if (node->kind() != AstNodeKind::AssignmentExpression ||
            node->tokenKind() != operators[index])
        {
            detail = "mixed chain lost assignment " + std::to_string(index);
            return false;
        }
        node = node->second();
    }
    ParserFixture reference("a = ref b = ref c");
    const AstNode* outer = reference.parser().parseExpression();
    const AstNode* inner = outer != nullptr ? outer->second() : nullptr;
    ParserFixture malformed("a =");
    const AstNode* recovery = malformed.parser().parseExpression();
    if (node != nullptr && node->text() == "terminal" &&
        mixed.diagnostics().diagnostics().empty() && outer != nullptr &&
        outer->isReference() && inner != nullptr && inner->isReference() &&
        reference.diagnostics().diagnostics().empty() && recovery != nullptr &&
        malformed.diagnostics().hasErrors()) return true;
    detail = "operator, ref, or recovery contract mismatch";
    return false;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testAssignmentDepth(detail);
    if (probe == kProbes[1]) return testLambdaDepth(detail);
    if (probe == kProbes[2]) return testAssignmentContracts(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testAssignmentIteration(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        context.expect(runProbe(probe, detail),
            "TP-PAR-551 iterative assignment must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-551", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-551 assignment probe must pass " + std::string(mode) +
                "; started=" + std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

void registerParserAssignmentIterationCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-551",
        "CP-PAR-036",
        testAssignmentIteration,
        "NIGHTLY");
}
}
