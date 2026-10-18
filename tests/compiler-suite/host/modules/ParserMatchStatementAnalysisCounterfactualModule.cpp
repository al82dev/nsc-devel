#include "ParserFinalTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <array>
#include <cstddef>
#include <cstdint>
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
using Parser = nativesharp::parser::Parser;
using Pattern = Parser::Pattern;
using PatternBindingMode = Parser::PatternBindingMode;
using PatternKind = Parser::PatternKind;

constexpr std::size_t kPatternDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-549-address-modes-32768",
    "TP-PAR-549-or-facts-32768",
    "TP-PAR-549-diagnostic-order",
    "TP-PAR-549-coverage-ownership"};

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

Pattern* wrapDeep(Pattern leaf)
{
    Pattern* const root = new Pattern;
    Pattern* cursor = root;
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        cursor->kind = PatternKind::Struct;
        cursor->fieldNames.push_back("field");
        cursor->children.emplace_back();
        cursor = &cursor->children.back();
    }
    *cursor = std::move(leaf);
    return root;
}

bool testDeepAddressModes(std::string& detail)
{
    ParserFixture fixture("");
    Pattern tuple;
    tuple.kind = PatternKind::Tuple;
    tuple.children.push_back(binding("alias", PatternBindingMode::ReferenceMutable, 17U));
    const Parser::MatchPatternAnalysis analysis =
        fixture.parser().analyzeMatchExpressionPattern(*wrapDeep(std::move(tuple)), false);
    if (analysis.nonAddressableBindings.size() == 1U &&
        analysis.nonAddressableBindings[0].first.begin().offset() == 17U &&
        analysis.nonAddressableBindings[0].second == PatternBindingMode::ReferenceMutable)
        return true;
    detail = "non-addressable=" + std::to_string(analysis.nonAddressableBindings.size());
    return false;
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

bool testDeepOrFacts(std::string& detail)
{
    ParserFixture fixture("");
    const Parser::MatchPatternAnalysis analysis =
        fixture.parser().analyzeMatchExpressionPattern(*makeDeepMismatchingOr(), false);
    const bool passed = analysis.bindings.size() == 1U && analysis.wildcard &&
        analysis.invalidOrRanges.size() == kPatternDepth &&
        analysis.invalidOrRanges.front().begin().offset() == kPatternDepth &&
        analysis.invalidOrRanges.back().begin().offset() == 1U;
    if (!passed)
        detail = "bindings=" + std::to_string(analysis.bindings.size()) +
            ", invalid-or=" + std::to_string(analysis.invalidOrRanges.size());
    return passed;
}

bool diagnosticContains(
    const nativesharp::diagnostics::Diagnostic& diagnostic,
    std::string_view text)
{
    return diagnostic.id() == DiagnosticId::InvalidOperand &&
        diagnostic.message().find(text) != std::string::npos;
}

bool testDiagnosticOrder(std::string& detail)
{
    ParserFixture fixture(
        "match (value) { (ref alias, _) | (var other, _) when false => {}, "
        "_ => {}, 1 | 1 => {} }");
    static_cast<void>(fixture.parser().parseStatement());
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    const bool passed = diagnostics.size() == 5U &&
        diagnosticContains(diagnostics[0], "ref pattern binding cannot cross") &&
        diagnosticContains(diagnostics[1], "OR-pattern alternatives") &&
        diagnosticContains(diagnostics[2], "guard is the constant false") &&
        diagnosticContains(diagnostics[3], "unreachable after a catch-all") &&
        diagnosticContains(diagnostics[4], "overlaps an earlier alternative");
    if (!passed) detail = "diagnostics=" + std::to_string(diagnostics.size());
    return passed;
}

std::size_t countText(const AstNode* root, std::string_view text)
{
    std::size_t result = 0;
    for (const AstNode* node : flatten(root)) result += node->text() == text ? 1U : 0U;
    return result;
}

bool testCoverageOwnership(std::string& detail)
{
    ParserFixture fixture(
        "match (next()) { Some(var payload) => { consume(payload); }, "
        "[var head, .. var tail] when ready => {}, ref mut alias => {} }");
    AstNode* const node = fixture.parser().parseStatement();
    const bool passed = node != nullptr && node->children().size() == 5U &&
        node->children()[0]->auxiliaryText() == "local:ref" &&
        node->auxiliaryText() == "match-coverage:v1:Some;~#1+;*" &&
        countText(node, "next") == 1U && countText(node, "payload") >= 2U &&
        fixture.diagnostics().diagnostics().empty();
    if (!passed)
        detail = node == nullptr ? "null" : "children=" +
            std::to_string(node->children().size()) + ", coverage=" +
            std::string(node->auxiliaryText()) + ", diagnostics=" +
            std::to_string(fixture.diagnostics().diagnostics().size());
    return passed;
}

bool runProbe(std::string_view probe, std::string& detail)
{
    if (probe == kProbes[0]) return testDeepAddressModes(detail);
    if (probe == kProbes[1]) return testDeepOrFacts(detail);
    if (probe == kProbes[2]) return testDiagnosticOrder(detail);
    if (probe == kProbes[3]) return testCoverageOwnership(detail);
    detail = "unknown probe '" + std::string(probe) + "'";
    return false;
}

void testMatchStatementAnalysis(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        std::string detail;
        const bool passed = runProbe(probe, detail);
        context.expect(passed,
            "TP-PAR-549 statement-match analysis must preserve its exact contract: " + detail);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-549", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-549 iterative statement-match analysis must pass probe " +
                std::string(mode) + "; started=" +
                std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

void registerParserMatchStatementAnalysisCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-549",
        "CP-PAR-042",
        testMatchStatementAnalysis,
        "NIGHTLY");
}
}
