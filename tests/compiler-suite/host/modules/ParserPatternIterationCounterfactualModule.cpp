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
using Pattern = nativesharp::parser::Parser::Pattern;
using PatternKind = nativesharp::parser::Parser::PatternKind;

constexpr std::size_t kPatternDepth = 32768;
constexpr std::string_view kProbe = "TP-PAR-542-depth-32768";

enum class DeepPatternShape
{
    Not,
    List,
    Tuple,
    Struct,
    Variant,
    Positional,
    Grouped
};

std::string wrapPattern(std::string_view open, std::string_view close)
{
    std::string source;
    source.reserve(kPatternDepth * (open.size() + close.size()) + 5U);
    for (std::size_t index = 0; index < kPatternDepth; ++index) source += open;
    source += "value";
    for (std::size_t index = 0; index < kPatternDepth; ++index) source += close;
    return source;
}

bool isBindingLeaf(const Pattern* pattern)
{
    return pattern != nullptr && pattern->kind == PatternKind::Binding &&
        pattern->name == "value" && pattern->children.empty();
}

bool measureSingleChildChain(
    const Pattern* pattern,
    PatternKind kind,
    std::string_view name,
    bool structField)
{
    std::size_t depth = 0;
    while (pattern != nullptr && pattern->kind == kind && pattern->children.size() == 1U)
    {
        if (!name.empty() && pattern->name != name) return false;
        if (structField &&
            (pattern->fieldNames.size() != 1U || pattern->fieldNames.front() != "f"))
            return false;
        ++depth;
        pattern = &pattern->children.front();
    }
    return depth == kPatternDepth && isBindingLeaf(pattern);
}

bool measurePattern(const Pattern* pattern, DeepPatternShape shape)
{
    switch (shape)
    {
        case DeepPatternShape::Not:
            return measureSingleChildChain(pattern, PatternKind::Not, {}, false);
        case DeepPatternShape::List:
            return measureSingleChildChain(pattern, PatternKind::List, {}, false);
        case DeepPatternShape::Tuple:
            return measureSingleChildChain(pattern, PatternKind::Tuple, {}, false);
        case DeepPatternShape::Struct:
            return measureSingleChildChain(pattern, PatternKind::Struct, {}, true);
        case DeepPatternShape::Variant:
            return measureSingleChildChain(pattern, PatternKind::Variant, "V", false);
        case DeepPatternShape::Positional:
            return measureSingleChildChain(pattern, PatternKind::Positional, "P", false);
        case DeepPatternShape::Grouped:
            return isBindingLeaf(pattern);
    }
    return false;
}

bool parseDeepPattern(
    std::string source,
    std::vector<std::string> knownTypes,
    DeepPatternShape shape)
{
    const std::size_t sourceLength = source.size();
    ParserFixture fixture(std::move(source), std::move(knownTypes));
    Pattern* pattern = new Pattern(fixture.parser().parsePattern(false));
    const std::size_t eof = fixture.tokens().size() - 1U;
    const bool exact = fixture.parser().index_ == eof &&
        fixture.diagnostics().diagnostics().empty() && pattern->range.isValid() &&
        pattern->range.begin().offset() == 0U && pattern->range.end().offset() == sourceLength &&
        measurePattern(pattern, shape);
    // The isolated child owns this deliberately leaked recursive value until process exit. Deleting
    // a 32,768-level std::vector<Pattern> chain would test destructor recursion, not parser recursion.
    return exact;
}

bool runDeepPatternMatrix()
{
    const bool notPattern = parseDeepPattern(
        std::string(kPatternDepth, '!') + "value", {}, DeepPatternShape::Not);
    const bool list = parseDeepPattern(
        wrapPattern("[", "]"), {}, DeepPatternShape::List);
    const bool tuple = parseDeepPattern(
        wrapPattern("(", ",)"), {}, DeepPatternShape::Tuple);
    const bool structure = parseDeepPattern(
        wrapPattern("{f:", "}"), {}, DeepPatternShape::Struct);
    const bool variant = parseDeepPattern(
        wrapPattern("V(", ")"), {}, DeepPatternShape::Variant);
    const bool positional = parseDeepPattern(
        wrapPattern("P(", ")"), {"P"}, DeepPatternShape::Positional);
    const bool grouped = parseDeepPattern(
        wrapPattern("(", ")"), {}, DeepPatternShape::Grouped);
    return notPattern && list && tuple && structure && variant && positional && grouped;
}

void testIterativePatternDepth(TestContext& context)
{
    if (isolated_process_support::probeMode() != kProbe)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-542", kProbe);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-542 depth 32768 must complete in the isolated parser process; started=" +
                std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
        return;
    }
    context.expect(runDeepPatternMatrix(),
        "TP-PAR-542 not, list, tuple, struct, variant, positional and grouped patterns at depth "
        "32768 must preserve cursor, AST shape, ranges and diagnostics without native-stack "
        "recursion");
}
}

void registerParserPatternIterationCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-542",
        "CP-PAR-042",
        testIterativePatternDepth,
        "NIGHTLY");
}
}
