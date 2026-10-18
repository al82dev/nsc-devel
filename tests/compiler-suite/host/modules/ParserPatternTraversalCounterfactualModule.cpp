#include "ParserFinalTestSupport.h"
#include "IsolatedProcessTestSupport.h"

#include <cstddef>
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
using Parser = nativesharp::parser::Parser;
using Pattern = Parser::Pattern;
using PatternBindingMode = Parser::PatternBindingMode;
using PatternKind = Parser::PatternKind;

constexpr std::size_t kPatternDepth = 32768;
constexpr std::string_view kProbe = "TP-PAR-543-depth-32768";

Pattern* makeDeepPattern(PatternKind leafKind, std::string leafName = {})
{
    Pattern* const root = new Pattern;
    Pattern* cursor = root;
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        cursor->kind = PatternKind::Not;
        cursor->children.emplace_back();
        cursor = &cursor->children.back();
    }
    cursor->kind = leafKind;
    cursor->name = std::move(leafName);
    return root;
}

bool isDeepPattern(const Pattern& root, PatternKind leafKind, std::string_view leafName = {})
{
    const Pattern* cursor = &root;
    std::size_t depth = 0;
    while (cursor->kind == PatternKind::Not && cursor->children.size() == 1U)
    {
        ++depth;
        cursor = &cursor->children.front();
    }
    return depth == kPatternDepth && cursor->kind == leafKind &&
        cursor->name == leafName && cursor->children.empty();
}

bool testDeepClassifiers(Parser& parser)
{
    Pattern* const binding = makeDeepPattern(PatternKind::Binding, "deep");
    Pattern* const literal = makeDeepPattern(PatternKind::Literal);
    const bool exact = parser.patternHasBindings(*binding) &&
        !parser.patternIsConditionOnly(*binding) && !parser.patternHasBindings(*literal) &&
        parser.patternIsConditionOnly(*literal);
    // The isolated child deliberately owns these recursive values until process exit.
    return exact;
}

bool testDeepExpansion(Parser& parser)
{
    Pattern* const source = makeDeepPattern(PatternKind::Literal);
    auto* const expanded = new std::vector<Pattern>(parser.expandPatternAlternatives(*source));
    // Deleting either 32,768-level value would test recursive container destruction.
    return expanded->size() == 1U &&
        isDeepPattern(expanded->front(), PatternKind::Literal);
}

Pattern namedPattern(std::string name)
{
    Pattern pattern;
    pattern.kind = PatternKind::Variant;
    pattern.name = std::move(name);
    return pattern;
}

Pattern orPattern(std::string first, std::string second)
{
    Pattern pattern;
    pattern.kind = PatternKind::Or;
    pattern.children.push_back(namedPattern(std::move(first)));
    pattern.children.push_back(namedPattern(std::move(second)));
    return pattern;
}

bool testExpansionOrder(Parser& parser)
{
    Pattern product;
    product.kind = PatternKind::Tuple;
    product.name = "root";
    product.firstInteger = 11;
    product.secondInteger = 29;
    product.restIndex = 1U;
    product.fieldNames = {"left", "right"};
    product.children.push_back(orPattern("A", "B"));
    product.children.push_back(orPattern("X", "Y"));
    const std::vector<Pattern> expanded = parser.expandPatternAlternatives(product);
    const std::vector<std::pair<std::string, std::string>> expected{
        {"A", "X"}, {"A", "Y"}, {"B", "X"}, {"B", "Y"}};
    if (expanded.size() != expected.size()) return false;
    for (std::size_t index = 0; index < expanded.size(); ++index)
    {
        const Pattern& alternative = expanded[index];
        if (alternative.kind != PatternKind::Tuple || alternative.name != "root" ||
            alternative.firstInteger != 11 || alternative.secondInteger != 29 ||
            alternative.restIndex != 1U || alternative.fieldNames != product.fieldNames ||
            alternative.children.size() != 2U ||
            alternative.children[0].name != expected[index].first ||
            alternative.children[1].name != expected[index].second)
            return false;
    }
    return true;
}

bool testDeepCollector(Parser& parser)
{
    Pattern* const source = makeDeepPattern(PatternKind::Binding, "deep");
    std::vector<std::pair<std::string, PatternBindingMode>> bindings;
    parser.collectUniquePatternBindings(*source, bindings);
    return bindings.size() == 1U && bindings.front().first == "deep" &&
        bindings.front().second == PatternBindingMode::Value;
}

bool testWideCollector(Parser& parser)
{
    Pattern source;
    source.kind = PatternKind::And;
    source.children.reserve(kPatternDepth * 2U + 1U);
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        Pattern binding;
        binding.kind = PatternKind::Binding;
        binding.name = "binding_" + std::to_string(index);
        binding.bindingMode = index % 2U == 0U
            ? PatternBindingMode::Copy : PatternBindingMode::Move;
        source.children.push_back(std::move(binding));
    }
    for (std::size_t index = 0; index < kPatternDepth; ++index)
        source.children.push_back(source.children[index]);
    std::vector<std::pair<std::string, PatternBindingMode>> bindings{
        {"binding_0", PatternBindingMode::Reference}};
    parser.collectUniquePatternBindings(source, bindings);
    if (bindings.size() != kPatternDepth) return false;
    for (std::size_t index = 1; index < bindings.size(); ++index)
    {
        const PatternBindingMode expected = index % 2U == 0U
            ? PatternBindingMode::Copy : PatternBindingMode::Move;
        if (bindings[index].first != "binding_" + std::to_string(index) ||
            bindings[index].second != expected)
            return false;
    }
    return bindings.front().second == PatternBindingMode::Reference;
}

bool runTraversalMatrix()
{
    ParserFixture fixture("_");
    Parser& parser = fixture.parser();
    return testDeepClassifiers(parser) && testDeepExpansion(parser) &&
        testExpansionOrder(parser) && testDeepCollector(parser) && testWideCollector(parser);
}

void testIterativePatternTraversals(TestContext& context)
{
    if (isolated_process_support::probeMode() != kProbe)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-543", kProbe);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-543 iterative pattern traversals must complete in the isolated process; "
            "started=" + std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
        return;
    }
    context.expect(runTraversalMatrix(),
        "TP-PAR-543 classifiers, alternative expansion and binding collection must preserve "
        "order and scalars at depth/width 32768 without recursion or quadratic lookup");
}
}

void registerParserPatternTraversalCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-543",
        "CP-PAR-042",
        testIterativePatternTraversals,
        "NIGHTLY");
}
}
