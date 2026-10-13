#include "ParserFoundationTestSupport.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::diagnostics::DiagnosticId;

void addPatternCase(
    TestRegistry& registry,
    std::string id,
    TestFunction function,
    TestDisposition disposition = TestDisposition::Required)
{
    registry.add(TestCase{std::move(id), "CP-PAR-031", "Host.Parser", "component", "PR",
        {"parser", "pattern"}, std::move(function), disposition});
}

std::vector<const AstNode*> nodesOfKind(const AstNode* root, AstNodeKind kind)
{
    std::vector<const AstNode*> matches;
    std::vector<const AstNode*> pending;
    if (root != nullptr) pending.push_back(root);
    while (!pending.empty())
    {
        const AstNode* const node = pending.back();
        pending.pop_back();
        if (node->kind() == kind) matches.push_back(node);
        if (node->first() != nullptr) pending.push_back(node->first());
        if (node->second() != nullptr) pending.push_back(node->second());
        if (node->third() != nullptr) pending.push_back(node->third());
        for (const AstNode* child : node->children())
        {
            if (child != nullptr) pending.push_back(child);
        }
    }
    return matches;
}

void testSignedIntegerBoundary(TestContext& context)
{
    ParserFixture fixture("-9223372036854775808");
    const Parser::Pattern pattern = fixture.parser().parsePattern(false);
    context.expect(pattern.kind == Parser::PatternKind::Literal &&
            pattern.firstInteger.has_value() &&
            *pattern.firstInteger == std::numeric_limits<std::int64_t>::min() &&
            pattern.firstValue != nullptr &&
            pattern.firstValue->kind() == AstNodeKind::UnaryExpression &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-351 signed pattern must preserve INT64_MIN without overflow");
}

void testValueBindings(TestContext& context)
{
    ParserFixture implicit("item");
    ParserFixture explicitBinding("var value");
    const Parser::Pattern first = implicit.parser().parsePattern(true);
    const Parser::Pattern second = explicitBinding.parser().parsePattern(false);
    context.expect(first.kind == Parser::PatternKind::Binding && first.name == "item" &&
            first.bindingMode == Parser::PatternBindingMode::Value &&
            second.kind == Parser::PatternKind::Binding && second.name == "value" &&
            second.bindingMode == Parser::PatternBindingMode::Value,
        "TP-PAR-352 implicit and explicit value bindings must preserve name and mode");
}

void testReferenceBinding(TestContext& context)
{
    ParserFixture fixture("ref value");
    const Parser::Pattern pattern = fixture.parser().parsePattern(false);
    context.expect(pattern.kind == Parser::PatternKind::Binding && pattern.name == "value" &&
            pattern.bindingMode == Parser::PatternBindingMode::Reference &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-353 ref binding must preserve reference mode");
}

void testMoveBinding(TestContext& context)
{
    ParserFixture fixture("move value");
    const Parser::Pattern pattern = fixture.parser().parsePattern(false);
    context.expect(pattern.kind == Parser::PatternKind::Binding && pattern.name == "value" &&
            pattern.bindingMode == Parser::PatternBindingMode::Move &&
            fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-354 move binding must preserve move mode");
}

void testEmptyAndNestedTuple(TestContext& context)
{
    ParserFixture empty("()");
    ParserFixture nested("(var a, (var b, var c))");
    const Parser::Pattern emptyPattern = empty.parser().parsePattern(false);
    const Parser::Pattern nestedPattern = nested.parser().parsePattern(false);
    context.expect(emptyPattern.kind == Parser::PatternKind::Tuple &&
            emptyPattern.children.empty() && empty.diagnostics().diagnostics().empty(),
        "TP-PAR-355 empty tuple pattern must produce a tuple with zero children");
    context.expect(nestedPattern.kind == Parser::PatternKind::Tuple &&
            nestedPattern.children.size() == 2 &&
            nestedPattern.children[1].kind == Parser::PatternKind::Tuple &&
            nestedPattern.children[1].children.size() == 2,
        "TP-PAR-355 nested tuple pattern must preserve its child hierarchy");
}

void testStructShorthandAndExplicitFields(TestContext& context)
{
    ParserFixture explicitFields("Point { x: var x, y: var y }");
    ParserFixture shorthand("Point { x, y: var y }");
    const Parser::Pattern first = explicitFields.parser().parsePattern(false);
    const Parser::Pattern second = shorthand.parser().parsePattern(false);
    context.expect(first.kind == Parser::PatternKind::Struct &&
            first.fieldNames == std::vector<std::string>({"x", "y"}) &&
            first.children.size() == 2 && explicitFields.diagnostics().diagnostics().empty(),
        "TP-PAR-356 explicit struct fields must align names and child patterns");
    context.expect(second.kind == Parser::PatternKind::Struct &&
            second.fieldNames == std::vector<std::string>({"x", "y"}) &&
            second.children.size() == 2 && shorthand.diagnostics().diagnostics().empty(),
        "TP-PAR-356 shorthand struct field must lower to an implicit value binding");
}

void testVariantPayloads(TestContext& context)
{
    ParserFixture empty("None");
    ParserFixture payload("Some(var value)");
    ParserFixture positional("Point(var value)", {"Point"});
    const Parser::Pattern first = empty.parser().parsePattern(false);
    const Parser::Pattern second = payload.parser().parsePattern(false);
    const Parser::Pattern third = positional.parser().parsePattern(false);
    context.expect(first.kind == Parser::PatternKind::Variant && first.name == "None" &&
            first.children.empty() && second.kind == Parser::PatternKind::Variant &&
            second.name == "Some" && second.children.size() == 1 &&
            second.children[0].kind == Parser::PatternKind::Binding &&
            second.children[0].name == "value" &&
            third.kind == Parser::PatternKind::Positional && third.name == "Point" &&
            third.children.size() == 1 &&
            third.children[0].kind == Parser::PatternKind::Binding,
        "TP-PAR-357 variant and unary positional patterns must remain unambiguous");
}

void testExclusiveAndInclusiveRanges(TestContext& context)
{
    ParserFixture exclusive("1..4");
    ParserFixture inclusive("1..=4");
    const Parser::Pattern first = exclusive.parser().parsePattern(false);
    const Parser::Pattern second = inclusive.parser().parsePattern(false);
    context.expect(first.kind == Parser::PatternKind::Range && first.firstInteger == 1 &&
            first.secondInteger == 4 && first.name == ".." && exclusive.diagnostics().diagnostics().empty(),
        "TP-PAR-358 exclusive range pattern must retain both integer endpoints");
    context.expect(second.kind == Parser::PatternKind::Range && second.firstInteger == 1 &&
            second.secondInteger == 4 && second.name == "..=" &&
            inclusive.parser().current().kind() == TokenKind::EndOfFile &&
            inclusive.diagnostics().diagnostics().empty(),
        "TP-PAR-358 inclusive range pattern must consume its delimiter and endpoints");
}

void testSimpleAndNestedOrPatterns(TestContext& context)
{
    ParserFixture simple("A | B | C");
    ParserFixture nested("(A | B, C | D)");
    const Parser::Pattern first = simple.parser().parsePattern(false);
    const Parser::Pattern second = nested.parser().parsePattern(false);
    context.expect(first.kind == Parser::PatternKind::Or && first.children.size() == 3 &&
            second.kind == Parser::PatternKind::Tuple && second.children.size() == 2 &&
            second.children[0].kind == Parser::PatternKind::Or &&
            second.children[1].kind == Parser::PatternKind::Or,
        "TP-PAR-359 simple and nested OR patterns must preserve every alternative");
}

void testCompatibleOrBindings(TestContext& context)
{
    ParserFixture fixture("Some(var value) | Other(var value)");
    const Parser::Pattern pattern = fixture.parser().parsePattern(false);
    const std::vector<Parser::Pattern> alternatives =
        fixture.parser().expandPatternAlternatives(pattern);
    context.expect(pattern.kind == Parser::PatternKind::Or && alternatives.size() == 2 &&
            alternatives[0].children.size() == 1 &&
            alternatives[1].children.size() == 1 &&
            alternatives[0].children[0].name == "value" &&
            alternatives[1].children[0].name == "value",
        "TP-PAR-360 compatible OR bindings must expand without losing identical names");
}

void testIncompatibleOrBindings(TestContext& context)
{
    ParserFixture fixture("match (source) { Some(var left) | Other(var right) => {} }");
    AstNode* const node = fixture.parser().parseStatement();
    context.expect(node != nullptr &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidOperand) == 1,
        "TP-PAR-361 incompatible OR bindings must produce the current contract diagnostic");
}

void testPatternBindingClassification(TestContext& context)
{
    Parser::Pattern wildcard;
    Parser::Pattern binding;
    binding.kind = Parser::PatternKind::Binding;
    Parser::Pattern tuple;
    tuple.kind = Parser::PatternKind::Tuple;
    tuple.children.push_back(binding);
    Parser::Pattern variant;
    variant.kind = Parser::PatternKind::Variant;
    variant.children.push_back(binding);
    ParserFixture fixture("");
    context.expect(!fixture.parser().patternHasBindings(wildcard) &&
            fixture.parser().patternHasBindings(binding) &&
            fixture.parser().patternHasBindings(tuple) &&
            fixture.parser().patternHasBindings(variant),
        "TP-PAR-362 binding classification must inspect every pattern kind and descendant");
}

void testCartesianAlternativeExpansion(TestContext& context)
{
    Parser::Pattern tuple;
    tuple.kind = Parser::PatternKind::Tuple;
    for (const std::array<std::string, 2>& names :
        {std::array<std::string, 2>{"A", "B"}, std::array<std::string, 2>{"C", "D"}})
    {
        Parser::Pattern alternatives;
        alternatives.kind = Parser::PatternKind::Or;
        for (const std::string& name : names)
        {
            Parser::Pattern child;
            child.kind = Parser::PatternKind::Variant;
            child.name = name;
            alternatives.children.push_back(std::move(child));
        }
        tuple.children.push_back(std::move(alternatives));
    }
    ParserFixture fixture("");
    const std::vector<Parser::Pattern> expanded =
        fixture.parser().expandPatternAlternatives(tuple);
    context.expect(expanded.size() == 4 && expanded[0].children.size() == 2 &&
            expanded[0].children[0].name == "A" && expanded[0].children[1].name == "C" &&
            expanded[3].children[0].name == "B" && expanded[3].children[1].name == "D",
        "TP-PAR-363 alternative expansion must produce the deterministic Cartesian product");
}

void testConditionOnlyClassification(TestContext& context)
{
    Parser::Pattern wildcard;
    Parser::Pattern binding;
    binding.kind = Parser::PatternKind::Binding;
    Parser::Pattern tuple;
    tuple.kind = Parser::PatternKind::Tuple;
    Parser::Pattern variant;
    variant.kind = Parser::PatternKind::Variant;
    variant.children.push_back(wildcard);
    ParserFixture fixture("");
    context.expect(fixture.parser().patternIsConditionOnly(wildcard) &&
            !fixture.parser().patternIsConditionOnly(binding) &&
            !fixture.parser().patternIsConditionOnly(tuple) &&
            fixture.parser().patternIsConditionOnly(variant),
        "TP-PAR-364 condition-only classification must reject binding and tuple patterns");
}

void testGeneratedPatternPredicates(TestContext& context)
{
    ParserFixture fixture("1..4");
    const Parser::Pattern range = fixture.parser().parsePattern(false);
    const Parser::PatternExpressionFactory source = [&fixture, &range]
    {
        return fixture.parser().makeIdentifierExpression(range.range, "source");
    };
    AstNode* const predicate = fixture.parser().makePatternCondition(range, source);
    Parser::Pattern wildcard;
    AstNode* const always = fixture.parser().makePatternCondition(wildcard, source);
    context.expect(predicate != nullptr && predicate->kind() == AstNodeKind::BinaryExpression &&
            predicate->tokenKind() == TokenKind::AmpAmp &&
            predicate->first() != nullptr && predicate->first()->tokenKind() == TokenKind::GreaterEqual &&
            predicate->second() != nullptr && predicate->second()->tokenKind() == TokenKind::Less &&
            always != nullptr && always->kind() == AstNodeKind::BooleanLiteralExpression &&
            always->integerValue() == 1,
        "TP-PAR-365 predicate generation must preserve range boundaries and wildcard truth");
}

void testBindingEmissionSwitch(TestContext& context)
{
    ParserFixture fixture("");
    Parser::Pattern binding;
    binding.kind = Parser::PatternKind::Binding;
    binding.name = "value";
    const Parser::PatternExpressionFactory source = [&fixture]
    {
        return fixture.parser().makeIdentifierExpression({}, "source");
    };
    AstNode* const continuation = fixture.parser().makeExpressionStatement(source());
    AstNode* const suppressed = fixture.parser().lowerPattern(
        binding, source, continuation, "temporary", false);
    AstNode* const emitted = fixture.parser().lowerPattern(
        binding, source, continuation, "temporary", true);
    context.expect(suppressed == continuation && emitted != nullptr &&
            emitted->kind() == AstNodeKind::BlockStatement && emitted->children().size() == 2 &&
            emitted->children()[0]->kind() == AstNodeKind::VariableDeclarationStatement &&
            emitted->children()[0]->text() == "value" && emitted->children()[1] == continuation,
        "TP-PAR-366 lowerPattern must honor emitBindings without changing continuation");
}

void testTemporaryPrefixMonotonicity(TestContext& context)
{
    ParserFixture fixture("(var value, _)");
    const Parser::Pattern pattern = fixture.parser().parsePattern(false);
    const Parser::PatternExpressionFactory source = [&fixture, &pattern]
    {
        return fixture.parser().makeIdentifierExpression(pattern.range, "source");
    };
    AstNode* const first = fixture.parser().lowerPattern(
        pattern, source, fixture.parser().makeExpressionStatement(source()), "probe", true);
    AstNode* const second = fixture.parser().lowerPattern(
        pattern, source, fixture.parser().makeExpressionStatement(source()), "probe", true);
    const auto firstDecls = nodesOfKind(first, AstNodeKind::DeconstructionDeclarationStatement);
    const auto secondDecls = nodesOfKind(second, AstNodeKind::DeconstructionDeclarationStatement);
    context.expect(firstDecls.size() == 1 && secondDecls.size() == 1 &&
            firstDecls[0]->children().size() == 2 && secondDecls[0]->children().size() == 2 &&
            firstDecls[0]->children()[0]->text() == "probe_tuple_0_0" &&
            secondDecls[0]->children()[0]->text() == "probe_tuple_2_0",
        "TP-PAR-367 repeated prefixes must still generate deterministic collision-free names");
}

void testMalformedPatternProgress(TestContext& context)
{
    ParserFixture fixture("]");
    const std::size_t before = fixture.parser().index_;
    const Parser::Pattern pattern = fixture.parser().parsePattern(false);
    context.expect(pattern.kind == Parser::PatternKind::Wildcard &&
            fixture.parser().index_ > before &&
            fixture.parser().current().kind() == TokenKind::EndOfFile &&
            diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedToken) == 1,
        "TP-PAR-368 malformed pattern must diagnose and advance to a stable cursor");
}
}

void registerParserPatternSemanticsModule(TestRegistry& registry)
{
    addPatternCase(registry, "TP-PAR-351", testSignedIntegerBoundary);
    addPatternCase(registry, "TP-PAR-352", testValueBindings);
    addPatternCase(registry, "TP-PAR-353", testReferenceBinding);
    addPatternCase(registry, "TP-PAR-354", testMoveBinding);
    addPatternCase(registry, "TP-PAR-355", testEmptyAndNestedTuple);
    addPatternCase(registry, "TP-PAR-356", testStructShorthandAndExplicitFields);
    addPatternCase(registry, "TP-PAR-357", testVariantPayloads);
    addPatternCase(registry, "TP-PAR-358", testExclusiveAndInclusiveRanges);
    addPatternCase(registry, "TP-PAR-359", testSimpleAndNestedOrPatterns);
    addPatternCase(registry, "TP-PAR-360", testCompatibleOrBindings);
    addPatternCase(registry, "TP-PAR-361", testIncompatibleOrBindings);
    addPatternCase(registry, "TP-PAR-362", testPatternBindingClassification);
    addPatternCase(registry, "TP-PAR-363", testCartesianAlternativeExpansion);
    addPatternCase(registry, "TP-PAR-364", testConditionOnlyClassification);
    addPatternCase(registry, "TP-PAR-365", testGeneratedPatternPredicates);
    addPatternCase(registry, "TP-PAR-366", testBindingEmissionSwitch);
    addPatternCase(registry, "TP-PAR-367", testTemporaryPrefixMonotonicity);
    addPatternCase(registry, "TP-PAR-368", testMalformedPatternProgress);
}
}
