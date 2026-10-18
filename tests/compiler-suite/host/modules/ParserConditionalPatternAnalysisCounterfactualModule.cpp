#include "ParserFinalTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;
using Parser = nativesharp::parser::Parser;
using Pattern = Parser::Pattern;
using PatternBindingMode = Parser::PatternBindingMode;
using PatternKind = Parser::PatternKind;

constexpr std::size_t kPatternDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-548-address-depth-32768",
    "TP-PAR-548-or-depth-32768",
    "TP-PAR-548-diagnostic-order",
    "TP-PAR-548-lowering-parity"};

nativesharp::SourceRange rangeAt(std::uint32_t offset)
{
    const nativesharp::FileId file{1U};
    return {{file, offset}, {file, offset + 1U}};
}

Pattern binding(std::string name, PatternBindingMode mode, std::uint32_t offset)
{
    Pattern result;
    result.kind = PatternKind::Binding;
    result.name = std::move(name);
    result.bindingMode = mode;
    result.range = rangeAt(offset);
    return result;
}

Pattern* wrapDeep(Pattern leaf, PatternKind kind)
{
    Pattern* const root = new Pattern;
    Pattern* cursor = root;
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        cursor->kind = kind;
        if (kind == PatternKind::Struct) cursor->fieldNames.push_back("field");
        cursor->children.emplace_back();
        cursor = &cursor->children.back();
    }
    *cursor = std::move(leaf);
    return root;
}

bool exactDiagnostic(
    const nativesharp::diagnostics::Diagnostic& diagnostic,
    std::uint32_t offset,
    std::string_view message)
{
    return diagnostic.id() == DiagnosticId::InvalidOperand &&
        diagnostic.range().has_value() &&
        diagnostic.range()->begin().offset() == offset && diagnostic.message() == message;
}

bool testDeepAddressValidation()
{
    ParserFixture fixture("");
    Pattern tuple;
    tuple.kind = PatternKind::Tuple;
    tuple.children.push_back(binding("alias", PatternBindingMode::Reference, 17U));
    Pattern* const pattern = wrapDeep(std::move(tuple), PatternKind::Struct);
    const Parser::ConditionalPatternAnalysis analysis =
        fixture.parser().analyzeConditionalPattern(*pattern);
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    return analysis.hasReferenceBinding && !analysis.hasMoveBinding &&
        diagnostics.size() == 1U && exactDiagnostic(diagnostics[0], 17U,
            "ref/move pattern binding cannot cross a tuple deconstruction or variant "
            "payload copy; bind the aggregate first");
}

Pattern* makeDeepMismatchingOr()
{
    Pattern* root = new Pattern(binding("value", PatternBindingMode::Value, 0U));
    for (std::size_t index = 1; index <= kPatternDepth; ++index)
    {
        Pattern* const parent = new Pattern;
        parent->kind = PatternKind::Or;
        parent->range = rangeAt(static_cast<std::uint32_t>(index));
        parent->children.push_back(std::move(*root));
        parent->children.emplace_back();
        parent->children.back().kind = PatternKind::Wildcard;
        root = parent;
    }
    return root;
}

bool testDeepOrValidation(std::string& detail)
{
    ParserFixture fixture("");
    Pattern* const pattern = makeDeepMismatchingOr();
    const Parser::MatchPatternAnalysis analysis =
        fixture.parser().analyzeMatchExpressionPattern(*pattern, false);
    if (analysis.bindings.size() != 1U || analysis.invalidOrRanges.size() != kPatternDepth)
    {
        detail = "bindings=" + std::to_string(analysis.bindings.size()) +
            ", invalid-or=" + std::to_string(analysis.invalidOrRanges.size());
        return false;
    }
    for (std::size_t index = 0; index < analysis.invalidOrRanges.size(); ++index)
    {
        const std::uint32_t offset = static_cast<std::uint32_t>(kPatternDepth - index);
        if (analysis.invalidOrRanges[index].begin().offset() != offset)
        {
            detail = "invalid-or=" + std::to_string(index) + ", expected-offset=" +
                std::to_string(offset) + ", actual-offset=" +
                std::to_string(analysis.invalidOrRanges[index].begin().offset());
            return false;
        }
    }
    return true;
}

Pattern wrappedBinding(
    PatternKind kind,
    std::string name,
    PatternBindingMode mode,
    std::uint32_t offset)
{
    Pattern result;
    result.kind = kind;
    result.children.push_back(binding(std::move(name), mode, offset));
    return result;
}

bool testDiagnosticOrder()
{
    ParserFixture fixture("");
    Pattern nested;
    nested.kind = PatternKind::Or;
    nested.range = rangeAt(2U);
    nested.children.push_back(wrappedBinding(
        PatternKind::Variant, "right", PatternBindingMode::Move, 11U));
    nested.children.push_back(binding("other", PatternBindingMode::Value, 12U));
    Pattern root;
    root.kind = PatternKind::Or;
    root.range = rangeAt(1U);
    root.children.push_back(wrappedBinding(
        PatternKind::Tuple, "left", PatternBindingMode::Reference, 10U));
    root.children.push_back(std::move(nested));
    const Parser::ConditionalPatternAnalysis analysis =
        fixture.parser().analyzeConditionalPattern(root);
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    constexpr std::array<std::uint32_t, 4> kOffsets{10U, 11U, 1U, 2U};
    if (!analysis.hasReferenceBinding || !analysis.hasMoveBinding || diagnostics.size() != 4U)
        return false;
    for (std::size_t index = 0; index < diagnostics.size(); ++index)
        if (!diagnostics[index].range().has_value() ||
            diagnostics[index].range()->begin().offset() != kOffsets[index])
            return false;
    return diagnostics[0].message().starts_with("ref/move pattern binding") &&
        diagnostics[1].message().starts_with("ref/move pattern binding") &&
        diagnostics[2].message().starts_with("OR-pattern alternatives") &&
        diagnostics[3].message().starts_with("OR-pattern alternatives");
}

struct LoweringStats final
{
    std::size_t temporary = 0;
    std::size_t matched = 0;
    std::size_t source = 0;
    std::size_t whileLoop = 0;
    std::size_t breakStatement = 0;
    std::size_t inlineLetElse = 0;
    std::size_t referenceTemporary = 0;
};

LoweringStats summarize(AstNode* root)
{
    LoweringStats result;
    std::vector<AstNode*> work{root};
    while (!work.empty())
    {
        AstNode* const node = work.back();
        work.pop_back();
        if (node == nullptr) continue;
        const bool hidden = node->text().starts_with("$__nsc_conditional_pattern_");
        const bool declaration = node->kind() == AstNodeKind::VariableDeclarationStatement;
        result.matched += declaration && hidden && node->text().ends_with("_matched") ? 1U : 0U;
        result.temporary += declaration && hidden && !node->text().ends_with("_matched") ? 1U : 0U;
        result.source += node->kind() == AstNodeKind::IdentifierExpression &&
            node->text() == "source" ? 1U : 0U;
        result.whileLoop += node->kind() == AstNodeKind::WhileStatement ? 1U : 0U;
        result.breakStatement += node->kind() == AstNodeKind::BreakStatement ? 1U : 0U;
        result.inlineLetElse += node->auxiliaryText() == "conditional-pattern-inline" ? 1U : 0U;
        result.referenceTemporary += declaration && hidden &&
            node->auxiliaryText() == "local:ref" ? 1U : 0U;
        work.push_back(node->first());
        work.push_back(node->second());
        work.push_back(node->third());
        for (AstNode* child : node->children()) work.push_back(child);
    }
    return result;
}

bool testLoweringParity(std::string& detail)
{
    ParserFixture ifFixture("if (let ref alias = source) { consume(alias); } else { wait(); }");
    ParserFixture whileFixture("while (let var current = source) { consume(current); }");
    ParserFixture letFixture("let var value = source else { return; };");
    const LoweringStats ifStats = summarize(ifFixture.parser().parseStatement());
    const LoweringStats whileStats = summarize(whileFixture.parser().parseStatement());
    const LoweringStats letStats = summarize(letFixture.parser().parseStatement());
    const bool passed = ifFixture.diagnostics().diagnostics().empty() &&
        whileFixture.diagnostics().diagnostics().empty() &&
        letFixture.diagnostics().diagnostics().empty() &&
        ifStats.temporary == 1U && ifStats.matched == 1U && ifStats.source == 1U &&
        ifStats.referenceTemporary == 1U && whileStats.temporary == 1U &&
        whileStats.matched == 1U && whileStats.source == 1U && whileStats.whileLoop == 1U &&
        whileStats.breakStatement == 1U && letStats.temporary == 1U &&
        letStats.matched == 1U && letStats.source == 1U && letStats.inlineLetElse == 1U;
    if (!passed)
    {
        detail = "if=" + std::to_string(ifStats.temporary) + "/" +
            std::to_string(ifStats.matched) + "/" + std::to_string(ifStats.source) + "/" +
            std::to_string(ifStats.referenceTemporary) + ", while=" +
            std::to_string(whileStats.temporary) + "/" + std::to_string(whileStats.matched) +
            "/" + std::to_string(whileStats.source) + "/" +
            std::to_string(whileStats.whileLoop) + "/" +
            std::to_string(whileStats.breakStatement) + ", let=" +
            std::to_string(letStats.temporary) + "/" + std::to_string(letStats.matched) +
            "/" + std::to_string(letStats.source) + "/" +
            std::to_string(letStats.inlineLetElse) + ", diagnostics=" +
            std::to_string(ifFixture.diagnostics().diagnostics().size()) + "/" +
            std::to_string(whileFixture.diagnostics().diagnostics().size()) + "/" +
            std::to_string(letFixture.diagnostics().diagnostics().size());
    }
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepAddressValidation();
    if (probe == kProbes[1]) return testDeepOrValidation(detail);
    if (probe == kProbes[2]) return testDiagnosticOrder();
    if (probe == kProbes[3]) return testLoweringParity(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testIterativeConditionalPatternAnalysis(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed,
            "TP-PAR-548 conditional-pattern analysis must preserve its exact contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-548", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-548 iterative conditional-pattern analysis must pass probe " +
                std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

void registerParserConditionalPatternAnalysisCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-548",
        "CP-PAR-042",
        testIterativeConditionalPatternAnalysis,
        "NIGHTLY");
}
}
