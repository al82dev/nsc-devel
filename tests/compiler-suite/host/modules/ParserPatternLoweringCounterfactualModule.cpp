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
using Parser = nativesharp::parser::Parser;
using Pattern = Parser::Pattern;
using PatternBindingMode = Parser::PatternBindingMode;
using PatternKind = Parser::PatternKind;

constexpr std::size_t kPatternDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-545-struct-depth-32768",
    "TP-PAR-545-deconstruction-depth-32768",
    "TP-PAR-545-variant-depth-32768",
    "TP-PAR-545-or-depth-32768"};
std::string probeFailure;

bool failProbe(std::string message)
{
    probeFailure = std::move(message);
    return false;
}

Pattern* makeDeepStructPattern()
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
    cursor->kind = PatternKind::Binding;
    cursor->name = "value";
    cursor->bindingMode = PatternBindingMode::Reference;
    return root;
}

Pattern* makeDeepDeconstructionPattern()
{
    Pattern* const root = new Pattern;
    Pattern* cursor = root;
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        cursor->kind = index % 2U == 0U ? PatternKind::Tuple : PatternKind::Positional;
        if (cursor->kind == PatternKind::Positional) cursor->name = "Position";
        cursor->children.emplace_back();
        cursor = &cursor->children.back();
    }
    cursor->kind = PatternKind::Wildcard;
    return root;
}

Pattern* makeDeepNamedPattern(PatternKind kind, std::string_view name)
{
    Pattern* const root = new Pattern;
    Pattern* cursor = root;
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        cursor->kind = kind;
        cursor->name = name;
        cursor->children.emplace_back();
        cursor = &cursor->children.back();
    }
    cursor->kind = PatternKind::Binding;
    cursor->name = "value";
    return root;
}

AstNode* makeContinuation(Parser& parser)
{
    return parser.makeExpressionStatement(parser.makeIdentifierExpression({}, "continuation"));
}

AstNode* lowerDeepPattern(
    Parser& parser,
    const Pattern& pattern,
    AstNode* continuation,
    std::size_t& sourceCalls)
{
    const Parser::PatternExpressionFactory source = [&parser, &sourceCalls]
    {
        ++sourceCalls;
        return parser.makeIdentifierExpression({}, "source");
    };
    return parser.lowerPattern(pattern, source, continuation, "deep", true);
}

bool measureMemberChain(const AstNode* source)
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

bool testDeepStructLowering()
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    Pattern* const pattern = makeDeepStructPattern();
    AstNode* const continuation = makeContinuation(parser);
    std::size_t sourceCalls = 0;
    AstNode* const result = lowerDeepPattern(parser, *pattern, continuation, sourceCalls);
    if (result == nullptr || result->kind() != AstNodeKind::BlockStatement ||
        result->children().size() != 2U)
        return false;
    const AstNode* const binding = result->children()[0];
    return sourceCalls == 1U && binding != nullptr &&
        binding->kind() == AstNodeKind::VariableDeclarationStatement &&
        binding->text() == "value" && binding->auxiliaryText() == "local:pattern-ref" &&
        measureMemberChain(binding->first()) && result->children()[1] == continuation;
}

bool measureDeconstruction(
    const AstNode* declaration,
    std::size_t index,
    std::string_view expectedSource,
    std::string& elementName)
{
    if (declaration == nullptr ||
        declaration->kind() != AstNodeKind::DeconstructionDeclarationStatement ||
        declaration->first() == nullptr || declaration->first()->text() != expectedSource ||
        declaration->children().size() != 1U)
        return false;
    const bool tuple = index % 2U == 0U;
    elementName = "deep_" + std::string(tuple ? "tuple_" : "pos_") +
        std::to_string(index) + "_0";
    if (declaration->children()[0]->text() != elementName) return false;
    return tuple ? declaration->typeSyntax() == nullptr
                 : declaration->typeSyntax() != nullptr &&
                       declaration->typeSyntax()->baseName() == "Position";
}

bool testDeepDeconstructionLowering()
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    Pattern* const pattern = makeDeepDeconstructionPattern();
    AstNode* const continuation = makeContinuation(parser);
    std::size_t sourceCalls = 0;
    const AstNode* result = lowerDeepPattern(parser, *pattern, continuation, sourceCalls);
    std::string expectedSource = "source";
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        if (result == nullptr || result->kind() != AstNodeKind::BlockStatement ||
            result->children().size() != 2U)
            return false;
        std::string elementName;
        if (!measureDeconstruction(
                result->children()[0], index, expectedSource, elementName))
            return false;
        expectedSource = std::move(elementName);
        result = result->children()[1];
    }
    return sourceCalls == 1U && result == continuation;
}

bool measureVariantLayer(const AstNode* decision, std::size_t index, const AstNode*& nested)
{
    if (decision == nullptr || decision->kind() != AstNodeKind::IfStatement ||
        decision->second() == nullptr ||
        decision->second()->kind() != AstNodeKind::BlockStatement ||
        decision->second()->children().size() != 2U)
        return false;
    const AstNode* const payload = decision->second()->children()[0];
    const std::string expectedName = "deep_payload_" + std::to_string(index);
    if (payload == nullptr || payload->kind() != AstNodeKind::VariableDeclarationStatement ||
        payload->text() != expectedName || payload->first() == nullptr ||
        payload->first()->auxiliaryText() != "variant-pattern-extract")
        return false;
    nested = decision->second()->children()[1];
    return true;
}

bool measureBindingResult(
    const AstNode* result,
    const AstNode* continuation,
    std::string_view sourceName)
{
    if (result == nullptr || result->kind() != AstNodeKind::BlockStatement ||
        result->children().size() != 2U || result->children()[1] != continuation)
        return false;
    const AstNode* const binding = result->children()[0];
    return binding != nullptr &&
        binding->kind() == AstNodeKind::VariableDeclarationStatement &&
        binding->text() == "value" && binding->first() != nullptr &&
        binding->first()->kind() == AstNodeKind::IdentifierExpression &&
        binding->first()->text() == sourceName;
}

bool measureSelectedContinuationChain(
    const AstNode* selectedBody,
    const AstNode* continuation)
{
    std::size_t depth = 0;
    while (selectedBody != nullptr && selectedBody != continuation &&
           selectedBody->kind() == AstNodeKind::BlockStatement &&
           selectedBody->children().size() == 2U)
    {
        ++depth;
        selectedBody = selectedBody->children()[1];
    }
    return depth == kPatternDepth && selectedBody == continuation;
}

bool testDeepVariantLowering()
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    Pattern* const pattern = makeDeepNamedPattern(PatternKind::Variant, "Some");
    AstNode* const continuation = makeContinuation(parser);
    std::size_t sourceCalls = 0;
    const AstNode* result = lowerDeepPattern(parser, *pattern, continuation, sourceCalls);
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        const AstNode* nested = nullptr;
        if (!measureVariantLayer(result, index, nested)) return false;
        result = nested;
    }
    return sourceCalls == 2U && measureBindingResult(
        result, continuation, "deep_payload_" + std::to_string(kPatternDepth - 1U));
}

bool measureOrLayer(const AstNode* block, std::size_t index, const AstNode*& nested)
{
    if (block == nullptr || block->kind() != AstNodeKind::BlockStatement ||
        block->children().size() != 2U)
        return failProbe("or block shape at depth " + std::to_string(index));
    const AstNode* const selected = block->children()[0];
    const AstNode* const guarded = block->children()[1];
    if (selected == nullptr || selected->kind() != AstNodeKind::VariableDeclarationStatement ||
        selected->text() != "deep_or_selected_" + std::to_string(index) ||
        guarded == nullptr || guarded->kind() != AstNodeKind::IfStatement ||
        guarded->second() == nullptr || guarded->second()->children().size() != 1U)
        return failProbe("or selected/guard shape at depth " + std::to_string(index));
    nested = guarded->second()->children()[0];
    return true;
}

bool testDeepOrLowering()
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    Pattern* const pattern = makeDeepNamedPattern(PatternKind::Or, "");
    AstNode* const continuation = makeContinuation(parser);
    std::size_t sourceCalls = 0;
    const AstNode* result = lowerDeepPattern(parser, *pattern, continuation, sourceCalls);
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        const AstNode* nested = nullptr;
        if (!measureOrLayer(result, index, nested)) return false;
        result = nested;
    }
    if (result == nullptr || result->children().size() != 2U)
        return failProbe("binding block shape after deep or chain");
    const AstNode* const selectedBody = result->children()[1];
    if (sourceCalls != 1U)
        return failProbe("deep or source calls=" + std::to_string(sourceCalls));
    if (selectedBody == nullptr || selectedBody->kind() != AstNodeKind::BlockStatement)
        return failProbe("deep or selected body kind");
    if (selectedBody->children().size() != 2U)
        return failProbe("deep or selected body children=" +
            std::to_string(selectedBody->children().size()));
    if (!measureBindingResult(result, selectedBody, "source"))
        return failProbe("deep or binding source/result mismatch");
    return measureSelectedContinuationChain(selectedBody, continuation)
        ? true
        : failProbe("deep or selected continuation chain/order mismatch");
}

bool runProbe(std::string_view probe)
{
    if (probe == kProbes[0]) return testDeepStructLowering();
    if (probe == kProbes[1]) return testDeepDeconstructionLowering();
    if (probe == kProbes[2]) return testDeepVariantLowering();
    if (probe == kProbes[3]) return testDeepOrLowering();
    return false;
}

void testIterativePatternLowering(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        const bool passed = runProbe(probe);
        context.expect(passed,
            "TP-PAR-545 lowering probe must preserve the exact deep AST contract: " +
                probeFailure);
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-545", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-545 iterative pattern lowering must pass probe " + std::string(mode) +
                "; started=" + std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error +
                ", output=" + result.output);
    }
}
}

void registerParserPatternLoweringCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-545",
        "CP-PAR-042",
        testIterativePatternLowering,
        "NIGHTLY");
}
}
