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
using Parser = nativesharp::parser::Parser;
using Pattern = Parser::Pattern;
using PatternBindingMode = Parser::PatternBindingMode;
using PatternKind = Parser::PatternKind;

constexpr std::size_t kPatternDepth = 32768;
constexpr std::array<std::string_view, 4> kProbes{
    "TP-PAR-546-struct-source-depth-32768",
    "TP-PAR-546-tuple-binding-depth-32768",
    "TP-PAR-546-variant-binding-depth-32768",
    "TP-PAR-546-list-or-parity"};

Pattern* makeDeepUnaryPattern(PatternKind kind, std::string_view name = {})
{
    Pattern* const root = new Pattern;
    Pattern* cursor = root;
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        cursor->kind = kind;
        cursor->name = name;
        if (kind == PatternKind::Struct) cursor->fieldNames.push_back("field");
        cursor->children.emplace_back();
        cursor = &cursor->children.back();
    }
    cursor->kind = PatternKind::Binding;
    cursor->name = "value";
    cursor->bindingMode = PatternBindingMode::Reference;
    return root;
}

AstNode* emitBindings(
    Parser& parser,
    const Pattern& pattern,
    std::size_t& sourceCalls,
    std::string_view prefix = "bind")
{
    AstNode* const destination =
        parser.arena_.make<AstNode>(AstNodeKind::BlockStatement, nativesharp::SourceRange{});
    const Parser::PatternExpressionFactory source = [&parser, &sourceCalls]
    {
        ++sourceCalls;
        return parser.makeIdentifierExpression({}, "source");
    };
    parser.emitPatternBindings(pattern, source, destination, prefix);
    return destination;
}

bool isBinding(
    const AstNode* node,
    std::string_view name,
    PatternBindingMode mode = PatternBindingMode::Value)
{
    if (node == nullptr || node->kind() != AstNodeKind::VariableDeclarationStatement ||
        node->text() != name)
        return false;
    if (mode == PatternBindingMode::Reference)
        return node->auxiliaryText() == "local:pattern-ref";
    return true;
}

bool testDeepStructSourcePlan()
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    Pattern* const pattern = makeDeepUnaryPattern(PatternKind::Struct);
    std::size_t sourceCalls = 0;
    const AstNode* const destination = emitBindings(parser, *pattern, sourceCalls);
    if (sourceCalls != 1U || destination->children().size() != 1U ||
        !isBinding(destination->children()[0], "value", PatternBindingMode::Reference))
        return false;
    const AstNode* source = destination->children()[0]->first();
    std::size_t depth = 0;
    while (source != nullptr && source->kind() == AstNodeKind::MemberExpression)
    {
        if (source->text() != "field") return false;
        ++depth;
        source = source->first();
    }
    return depth == kPatternDepth && source != nullptr && source->text() == "source";
}

bool testDeepTupleEmission()
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    Pattern* const pattern = makeDeepUnaryPattern(PatternKind::Tuple);
    std::size_t sourceCalls = 0;
    const AstNode* const destination = emitBindings(parser, *pattern, sourceCalls);
    if (sourceCalls != 1U || destination->children().size() != kPatternDepth + 1U)
        return false;
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        const AstNode* const declaration = destination->children()[index];
        const std::string name = "bind_tuple_" + std::to_string(index) + "_0";
        if (declaration->kind() != AstNodeKind::DeconstructionDeclarationStatement ||
            declaration->children().size() != 1U || declaration->children()[0]->text() != name)
            return false;
        const std::string expectedSource = index == 0U
            ? "source" : "bind_tuple_" + std::to_string(index - 1U) + "_0";
        if (declaration->first() == nullptr || declaration->first()->text() != expectedSource)
            return false;
    }
    const AstNode* const binding = destination->children().back();
    return isBinding(binding, "value", PatternBindingMode::Reference) &&
        binding->first()->text() ==
            "bind_tuple_" + std::to_string(kPatternDepth - 1U) + "_0";
}

bool testDeepVariantEmission()
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    Pattern* const pattern = makeDeepUnaryPattern(PatternKind::Variant, "Some");
    std::size_t sourceCalls = 0;
    const AstNode* const destination = emitBindings(parser, *pattern, sourceCalls);
    if (sourceCalls != 1U || destination->children().size() != kPatternDepth + 1U)
        return false;
    for (std::size_t index = 0; index < kPatternDepth; ++index)
    {
        const AstNode* const declaration = destination->children()[index];
        if (!isBinding(declaration, "bind_payload_" + std::to_string(index)) ||
            declaration->first() == nullptr ||
            declaration->first()->auxiliaryText() != "variant-pattern-extract")
            return false;
    }
    const AstNode* const binding = destination->children().back();
    return isBinding(binding, "value", PatternBindingMode::Reference) &&
        binding->first()->text() ==
            "bind_payload_" + std::to_string(kPatternDepth - 1U);
}

Pattern binding(std::string name, PatternBindingMode mode = PatternBindingMode::Value)
{
    Pattern result;
    result.kind = PatternKind::Binding;
    result.name = std::move(name);
    result.bindingMode = mode;
    return result;
}

bool verifyListBindings(const AstNode* destination, std::size_t sourceCalls)
{
    if (sourceCalls != 5U || destination->children().size() != 3U) return false;
    const AstNode* const rest = destination->children()[1]->first();
    const AstNode* const tail = destination->children()[2]->first();
    if (!isBinding(destination->children()[0], "head") ||
        !isBinding(destination->children()[1], "rest") ||
        !isBinding(destination->children()[2], "tail") || rest == nullptr || tail == nullptr)
        return false;
    if (rest->kind() != AstNodeKind::CallExpression ||
        rest->auxiliaryText() != "list-pattern-rest" || rest->children().size() != 2U ||
        rest->first() == nullptr || rest->first()->text() != "Slice")
        return false;
    return tail->kind() == AstNodeKind::IndexExpression && tail->second() != nullptr &&
        tail->second()->kind() == AstNodeKind::BinaryExpression &&
        tail->second()->first()->auxiliaryText() == "list-pattern-length";
}

bool verifyOrBinding(Parser& parser)
{
    Pattern pattern;
    pattern.kind = PatternKind::Or;
    pattern.children.push_back(binding("selected", PatternBindingMode::Reference));
    pattern.children.push_back(binding("selected", PatternBindingMode::Reference));
    std::size_t sourceCalls = 0;
    const AstNode* const destination = emitBindings(parser, pattern, sourceCalls);
    if (sourceCalls != 1U || destination->children().size() != 1U ||
        !isBinding(destination->children()[0], "selected", PatternBindingMode::Reference))
        return false;
    const AstNode* const selector = destination->children()[0]->first();
    if (selector == nullptr || selector->kind() != AstNodeKind::MatchExpression ||
        selector->children().size() != 2U)
        return false;
    for (const AstNode* arm : selector->children())
        if (arm == nullptr || arm->kind() != AstNodeKind::MatchArmExpression ||
            arm->children().size() != 1U || !isBinding(arm->children()[0], "selected") ||
            arm->third() == nullptr || arm->third()->text() != "selected" || !arm->flag())
            return false;
    return true;
}

bool testListAndOrParity()
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    Pattern list;
    list.kind = PatternKind::List;
    list.restIndex = 1U;
    list.children.push_back(binding("head"));
    list.children.push_back(binding("rest"));
    list.children.push_back(binding("tail"));
    std::size_t sourceCalls = 0;
    const AstNode* const destination = emitBindings(parser, list, sourceCalls);
    return verifyListBindings(destination, sourceCalls) && verifyOrBinding(parser);
}

bool runProbe(std::string_view probe)
{
    if (probe == kProbes[0]) return testDeepStructSourcePlan();
    if (probe == kProbes[1]) return testDeepTupleEmission();
    if (probe == kProbes[2]) return testDeepVariantEmission();
    if (probe == kProbes[3]) return testListAndOrParity();
    return false;
}

void testIterativePatternBindingEmission(TestContext& context)
{
    const std::string probe = isolated_process_support::probeMode();
    if (!probe.empty())
    {
        context.expect(runProbe(probe),
            "TP-PAR-546 binding-emission probe must preserve its exact AST contract");
        return;
    }
    for (const std::string_view mode : kProbes)
    {
        const auto result = isolated_process_support::runCurrentExecutable("TP-PAR-546", mode);
        context.expect(result.started && result.error.empty() && result.exitCode == 0 &&
                result.output.find("\"status\":\"PASS\"") != std::string::npos,
            "TP-PAR-546 iterative binding emission must pass probe " + std::string(mode) +
                "; started=" + std::string(result.started ? "true" : "false") +
                ", exit=" + std::to_string(result.exitCode) + ", error=" + result.error);
    }
}
}

void registerParserPatternBindingEmissionCounterfactualModule(TestRegistry& registry)
{
    addFinalParserCase(
        registry,
        "TP-PAR-546",
        "CP-PAR-042",
        testIterativePatternBindingEmission,
        "NIGHTLY");
}
}
