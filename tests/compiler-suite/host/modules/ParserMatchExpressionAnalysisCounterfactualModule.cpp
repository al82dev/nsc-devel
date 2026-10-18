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
using Parser = nativesharp::parser::Parser;
using Pattern = Parser::Pattern;
using PatternBindingMode = Parser::PatternBindingMode;
using PatternKind = Parser::PatternKind;

constexpr std::size_t kPatternDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-547-binding-depth-32768",
    "TP-PAR-547-or-depth-32768",
    "TP-PAR-547-coverage-depth-32768",
    "TP-PAR-547-diagnostic-coverage-parity"};

Pattern binding(std::string name, PatternBindingMode mode = PatternBindingMode::Value)
{
    Pattern result;
    result.kind = PatternKind::Binding;
    result.name = std::move(name);
    result.bindingMode = mode;
    return result;
}

Pattern* makeDeepUnaryPattern(PatternKind kind, Pattern leaf)
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

bool testDeepBindingInventory()
{
    ParserFixture fixture("");
    Pattern* const pattern = makeDeepUnaryPattern(
        PatternKind::Struct, binding("value", PatternBindingMode::Reference));
    const Parser::MatchPatternAnalysis analysis =
        fixture.parser().analyzeMatchExpressionPattern(*pattern, true);
    return fixture.diagnostics().diagnostics().empty() && analysis.bindings.size() == 1U &&
        analysis.bindings[0].first == "value" &&
        analysis.bindings[0].second == PatternBindingMode::Reference &&
        analysis.cases.empty() && analysis.listLengths.empty() && !analysis.wildcard;
}

bool testDeepOrCanonicalInventory()
{
    ParserFixture fixture("");
    Pattern* const pattern =
        makeDeepUnaryPattern(PatternKind::Or, binding("selected", PatternBindingMode::Move));
    const Parser::MatchPatternAnalysis analysis =
        fixture.parser().analyzeMatchExpressionPattern(*pattern, true);
    return fixture.diagnostics().diagnostics().empty() && analysis.bindings.size() == 1U &&
        analysis.bindings[0].first == "selected" &&
        analysis.bindings[0].second == PatternBindingMode::Move && analysis.wildcard;
}

std::string coverageFor(
    Parser& parser,
    const Pattern& pattern,
    AstNode* guard = nullptr)
{
    std::string coverage = "match-coverage:v1:";
    bool firstArm = true;
    parser.appendMatchCoverage(
        parser.analyzeMatchExpressionPattern(pattern, false), guard, coverage, firstArm);
    return coverage;
}

bool testDeepCatchAllCoverage()
{
    ParserFixture fixture("");
    Pattern wildcard;
    wildcard.kind = PatternKind::Wildcard;
    Pattern* const pattern = makeDeepUnaryPattern(PatternKind::Or, std::move(wildcard));
    return coverageFor(fixture.parser(), *pattern) == "match-coverage:v1:*";
}

nativesharp::SourceRange rangeAt(std::uint32_t offset)
{
    const nativesharp::FileId file{1U};
    return {{file, offset}, {file, offset + 1U}};
}

Pattern mismatchingOr(
    std::uint32_t offset,
    std::string left,
    std::string right,
    PatternBindingMode rightMode = PatternBindingMode::Value)
{
    Pattern pattern;
    pattern.kind = PatternKind::Or;
    pattern.range = rangeAt(offset);
    pattern.children.push_back(binding(std::move(left)));
    pattern.children.push_back(binding(std::move(right), rightMode));
    return pattern;
}

bool verifyDiagnosticOrder(ParserFixture& fixture, const Pattern& pattern)
{
    fixture.parser().analyzeMatchExpressionPattern(pattern, true);
    const auto& diagnostics = fixture.diagnostics().diagnostics();
    if (diagnostics.size() != 3U) return false;
    constexpr std::array<std::uint32_t, 3> kOffsets{1U, 2U, 3U};
    for (std::size_t index = 0; index < diagnostics.size(); ++index)
    {
        if (diagnostics[index].id() != DiagnosticId::InvalidOperand ||
            !diagnostics[index].range().has_value() ||
            diagnostics[index].range()->begin().offset() != kOffsets[index] ||
            diagnostics[index].message() !=
                "OR-pattern alternatives in a match expression must bind identical names "
                "with identical value/ref/move modes")
            return false;
    }
    return true;
}

bool verifyCoverageParity(Parser& parser)
{
    Pattern variants;
    variants.kind = PatternKind::Or;
    Pattern some;
    some.kind = PatternKind::Variant;
    some.name = "Some";
    some.children.emplace_back();
    some.children[0].kind = PatternKind::Literal;
    Pattern none;
    none.kind = PatternKind::Variant;
    none.name = "None";
    variants.children.push_back(std::move(some));
    variants.children.push_back(std::move(none));
    if (coverageFor(parser, variants) != "match-coverage:v1:~None|Some") return false;

    Pattern list;
    list.kind = PatternKind::List;
    list.children.emplace_back();
    list.children.back().kind = PatternKind::Wildcard;
    list.children.emplace_back();
    list.children.back().kind = PatternKind::Literal;
    if (coverageFor(parser, list) != "match-coverage:v1:~#2") return false;

    Pattern wildcard;
    wildcard.kind = PatternKind::Wildcard;
    AstNode* const guard = parser.makeBooleanLiteral({}, false);
    return coverageFor(parser, wildcard, guard) == "match-coverage:v1:~*";
}

bool testDiagnosticAndCoverageParity()
{
    ParserFixture fixture("");
    Pattern root;
    root.kind = PatternKind::Or;
    root.range = rangeAt(1U);
    root.children.push_back(mismatchingOr(2U, "left", "left", PatternBindingMode::Reference));
    root.children.push_back(mismatchingOr(3U, "right", "other"));
    return verifyDiagnosticOrder(fixture, root) && verifyCoverageParity(fixture.parser());
}

bool runProbe(std::string_view probe)
{
    if (probe == kProbes[0]) return testDeepBindingInventory();
    if (probe == kProbes[1]) return testDeepOrCanonicalInventory();
    if (probe == kProbes[2]) return testDeepCatchAllCoverage();
    if (probe == kProbes[3]) return testDiagnosticAndCoverageParity();
    return false;
}

void testIterativeMatchExpressionAnalysis(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        context.expect(runProbe(probe),
            "TP-PAR-547 match-expression analysis must preserve its exact contract");
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-547", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-547 iterative match analysis must pass probe " + std::string(mode) +
                "; started=" + std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

void registerParserMatchExpressionAnalysisCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-547",
        "CP-PAR-042",
        testIterativeMatchExpressionAnalysis,
        "NIGHTLY");
}
}
