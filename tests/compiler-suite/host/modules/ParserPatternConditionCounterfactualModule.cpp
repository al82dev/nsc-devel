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
using PatternKind = Parser::PatternKind;

constexpr std::size_t kPatternDepth = 32768;
constexpr std::string_view kProbe = "TP-PAR-544-depth-32768";

Pattern* makeDeepNotPattern()
{
    Pattern* const root = new Pattern;
    Pattern* cursor = root;
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        cursor->kind = PatternKind::Not;
        cursor->children.emplace_back();
        cursor = &cursor->children.back();
    }
    cursor->kind = PatternKind::Wildcard;
    return root;
}

Pattern* makeDeepStructPattern(AstNode* literal)
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
    cursor->kind = PatternKind::Literal;
    cursor->firstValue = literal;
    return root;
}

bool measureDeepNotCondition(const AstNode* condition)
{
    std::size_t depth = 0;
    while (condition != nullptr && condition->kind() == AstNodeKind::UnaryExpression &&
           condition->tokenKind() == TokenKind::Bang)
    {
        ++depth;
        condition = condition->first();
    }
    return depth == kPatternDepth && condition != nullptr &&
        condition->kind() == AstNodeKind::BooleanLiteralExpression &&
        condition->integerValue() == 1U;
}

bool measureMemberSource(const AstNode* source)
{
    std::size_t depth = 0;
    while (source != nullptr && source->kind() == AstNodeKind::MemberExpression)
    {
        if (source->text() != "field") return false;
        ++depth;
        source = source->first();
    }
    return depth == kPatternDepth && source != nullptr &&
        source->kind() == AstNodeKind::IdentifierExpression && source->text() == "source";
}

bool measureDeepStructCondition(const AstNode* condition, const AstNode* literal)
{
    std::size_t depth = 0;
    while (condition != nullptr && condition->kind() == AstNodeKind::BinaryExpression &&
           condition->tokenKind() == TokenKind::AmpAmp)
    {
        const AstNode* const initial = condition->first();
        if (initial == nullptr || initial->kind() != AstNodeKind::BooleanLiteralExpression ||
            initial->integerValue() != 1U)
            return false;
        ++depth;
        condition = condition->second();
    }
    return depth == kPatternDepth && condition != nullptr &&
        condition->kind() == AstNodeKind::BinaryExpression &&
        condition->tokenKind() == TokenKind::EqualEqual && condition->second() == literal &&
        measureMemberSource(condition->first());
}

bool testDeepNotAndSourcePlan(Parser& parser)
{
    Pattern* const negated = makeDeepNotPattern();
    AstNode* const notCondition = parser.makePatternCondition(*negated, [&parser]
    {
        return parser.makeIdentifierExpression({}, "unused");
    });
    AstNode* const literal = parser.makeIntegerLiteralExpression({}, 7U);
    Pattern* const structure = makeDeepStructPattern(literal);
    std::size_t sourceCalls = 0;
    AstNode* const structCondition = parser.makePatternCondition(*structure, [&]
    {
        ++sourceCalls;
        return parser.makeIdentifierExpression({}, "source");
    });
    // The isolated child owns both recursive Pattern values until process exit.
    return sourceCalls == 1U && measureDeepNotCondition(notCondition) &&
        measureDeepStructCondition(structCondition, literal);
}

bool testWideLogicalCondition(Parser& parser)
{
    Pattern pattern;
    pattern.kind = PatternKind::And;
    pattern.children.resize(kPatternDepth);
    AstNode* condition = parser.makePatternCondition(pattern, [&parser]
    {
        return parser.makeIdentifierExpression({}, "unused");
    });
    std::size_t joins = 0;
    while (condition != nullptr && condition->kind() == AstNodeKind::BinaryExpression &&
           condition->tokenKind() == TokenKind::AmpAmp)
    {
        const AstNode* const right = condition->second();
        if (right == nullptr || right->kind() != AstNodeKind::BooleanLiteralExpression ||
            right->integerValue() != 1U)
            return false;
        ++joins;
        condition = condition->first();
    }
    return joins + 1U == kPatternDepth && condition != nullptr &&
        condition->kind() == AstNodeKind::BooleanLiteralExpression &&
        condition->integerValue() == 1U;
}

void testVariantListSourceOrder(Parser& parser, TestContext& context)
{
    ParserFixture patternFixture("Some([1, .., 2])");
    const Pattern pattern = patternFixture.parser().parsePattern(false);
    std::size_t sourceCalls = 0;
    AstNode* const condition = parser.makePatternCondition(pattern, [&]
    {
        ++sourceCalls;
        return parser.makeIdentifierExpression({}, "source");
    });
    const std::size_t extracts = countAuxiliary(condition, "variant-pattern-extract");
    const std::size_t lengths = countAuxiliary(condition, "list-pattern-length");
    // IndexExpression exposes its index both as second() and as a child.  The flattened
    // product graph therefore observes the trailing index subtree twice, exactly as before.
    context.expect(patternFixture.diagnostics().diagnostics().empty() &&
            pattern.kind == PatternKind::Variant && sourceCalls == 5U && extracts == 5U &&
            lengths == 3U,
        "TP-PAR-544 variant/list condition must preserve exact source evaluation order; calls=" +
            std::to_string(sourceCalls) + ", extracts=" + std::to_string(extracts) +
            ", lengths=" + std::to_string(lengths));
}

void runConditionMatrix(TestContext& context)
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    context.expect(testDeepNotAndSourcePlan(parser),
        "TP-PAR-544 deep condition and source plan must preserve exact AST order");
    context.expect(testWideLogicalCondition(parser),
        "TP-PAR-544 wide logical condition must preserve exact left association");
    testVariantListSourceOrder(parser, context);
}

void testIterativePatternConditionBuilder(TestContext& context)
{
    if (isolated_process_support::probeMode() != kProbe)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-544", kProbe);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-544 iterative pattern condition builder must complete in isolation; started=" +
                std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
        return;
    }
    runConditionMatrix(context);
}
}

void registerParserPatternConditionCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-544",
        "CP-PAR-042",
        testIterativePatternConditionBuilder,
        "NIGHTLY");
}
}
