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
    "TP-PAR-554-conditional-false-depth",
    "TP-PAR-554-conditional-true-depth",
    "TP-PAR-554-assignment-conditional-depth",
    "TP-PAR-554-contracts-recovery"};

bool testConditionalFalseDepth(std::string& detail)
{
    std::string source;
    source.reserve(kDepth * 28U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "condition" + std::to_string(index) + " ? value" +
            std::to_string(index) + " : ";
    source += "terminal";
    ParserFixture fixture(std::move(source));
    const AstNode* node = fixture.parser().parseExpression();
    for (std::size_t index = 0; index < kDepth && node != nullptr; ++index)
    {
        if (node->kind() != AstNodeKind::ConditionalExpression || node->first() == nullptr ||
            node->first()->text() != "condition" + std::to_string(index) ||
            node->second() == nullptr || node->second()->text() != "value" +
                std::to_string(index))
        {
            detail = "invalid false-branch conditional at depth " + std::to_string(index);
            return false;
        }
        node = node->third();
    }
    if (node != nullptr && node->text() == "terminal" &&
        fixture.diagnostics().diagnostics().empty()) return true;
    detail = "false-branch terminal or diagnostics mismatch";
    return false;
}

bool testConditionalTrueDepth(std::string& detail)
{
    std::string source;
    source.reserve(kDepth * 25U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "condition" + std::to_string(index) + " ? ";
    source += "terminal";
    for (std::size_t index = kDepth; index != 0; --index)
        source += " : fallback" + std::to_string(index - 1U);
    ParserFixture fixture(std::move(source));
    const AstNode* node = fixture.parser().parseExpression();
    for (std::size_t index = 0; index < kDepth && node != nullptr; ++index)
    {
        if (node->kind() != AstNodeKind::ConditionalExpression || node->first() == nullptr ||
            node->first()->text() != "condition" + std::to_string(index) ||
            node->third() == nullptr || node->third()->text() != "fallback" +
                std::to_string(index))
        {
            detail = "invalid true-branch conditional at depth " + std::to_string(index);
            return false;
        }
        node = node->second();
    }
    if (node != nullptr && node->text() == "terminal" &&
        fixture.diagnostics().diagnostics().empty()) return true;
    detail = "true-branch terminal or diagnostics mismatch";
    return false;
}

bool testAssignmentConditionalDepth(std::string& detail)
{
    std::string source;
    source.reserve(kDepth * 38U);
    for (std::size_t index = 0; index < kDepth; ++index)
        source += "target" + std::to_string(index) + " = condition" +
            std::to_string(index) + " ? value" + std::to_string(index) + " : ";
    source += "terminal";
    ParserFixture fixture(std::move(source));
    const AstNode* node = fixture.parser().parseExpression();
    for (std::size_t index = 0; index < kDepth && node != nullptr; ++index)
    {
        const AstNode* conditional = node->second();
        if (node->kind() != AstNodeKind::AssignmentExpression || node->first() == nullptr ||
            node->first()->text() != "target" + std::to_string(index) ||
            conditional == nullptr || conditional->kind() != AstNodeKind::ConditionalExpression ||
            conditional->first() == nullptr || conditional->first()->text() !=
                "condition" + std::to_string(index))
        {
            detail = "invalid assignment/conditional node at depth " + std::to_string(index);
            return false;
        }
        node = conditional->third();
    }
    if (node != nullptr && node->text() == "terminal" &&
        fixture.diagnostics().diagnostics().empty()) return true;
    detail = "assignment/conditional terminal or diagnostics mismatch";
    return false;
}

bool testContractsAndRecovery(std::string& detail)
{
    ParserFixture valid("target = ref source ? handler => value : fallback");
    const AstNode* assignment = valid.parser().parseExpression();
    const AstNode* conditional = assignment != nullptr ? assignment->second() : nullptr;
    const AstNode* lambda = conditional != nullptr ? conditional->second() : nullptr;
    ParserFixture block("handler => { return value; }");
    const AstNode* blockLambda = block.parser().parseExpression();
    ParserFixture missingTrue("condition ? : fallback");
    missingTrue.parser().parseExpression();
    ParserFixture missingFalse("condition ? value :");
    missingFalse.parser().parseExpression();
    if (assignment == nullptr || assignment->kind() != AstNodeKind::AssignmentExpression ||
        !assignment->isReference())
    {
        detail = "reference assignment shape changed";
        return false;
    }
    if (conditional == nullptr || conditional->kind() != AstNodeKind::ConditionalExpression ||
        lambda == nullptr || lambda->kind() != AstNodeKind::LambdaExpression || lambda->flag())
    {
        detail = "conditional or expression-lambda shape changed";
        return false;
    }
    if (blockLambda == nullptr || blockLambda->kind() != AstNodeKind::LambdaExpression ||
        !blockLambda->flag() || blockLambda->first() == nullptr ||
        blockLambda->first()->kind() != AstNodeKind::BlockStatement)
    {
        detail = "lambda-block boundary changed";
        return false;
    }
    if (!valid.diagnostics().diagnostics().empty() || !block.diagnostics().diagnostics().empty())
    {
        detail = "valid contract emitted diagnostics";
        return false;
    }
    if (missingTrue.diagnostics().diagnostics().empty())
    {
        detail = "missing true branch did not diagnose";
        return false;
    }
    if (missingFalse.diagnostics().diagnostics().empty())
    {
        detail = "missing false branch did not diagnose";
        return false;
    }
    return true;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testConditionalFalseDepth(detail);
    if (probe == kProbes[1]) return testConditionalTrueDepth(detail);
    if (probe == kProbes[2]) return testAssignmentConditionalDepth(detail);
    if (probe == kProbes[3]) return testContractsAndRecovery(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testConditionalTrampoline(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed,
            "TP-PAR-554 conditional trampoline must preserve its contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-554", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-554 conditional trampoline probe must pass " + std::string(mode) +
                "; started=" + std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

void registerParserConditionalTrampolineCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-554",
        "CP-PAR-036",
        testConditionalTrampoline,
        "NIGHTLY");
}
}
