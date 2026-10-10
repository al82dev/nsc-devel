#include "ParserFinalTestSupport.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace nsc::suite
{
namespace
{
using namespace parser_final_support;
using namespace parser_support;
using namespace nativesharp::ast;
using nativesharp::lexer::TokenKind;

AstNode* firstChild(AstNode* node)
{
    return node != nullptr && !node->children().empty() ? node->children().front() : nullptr;
}

void testMakeHelpers(TestContext& context)
{
    ParserFixture fixture("");
    Parser& parser = fixture.parser();
    const nativesharp::SourceRange range = parser.current().range();
    AstNode* id = parser.makeIdentifierExpression(range, "item");
    AstNode* integer = parser.makeIntegerLiteralExpression(range, 7);
    AstNode* member = parser.makeMemberExpression(id, "Field");
    AstNode* call = parser.makeZeroArgCallExpression(member);
    AstNode* index = parser.makeIndexExpression(id, integer);
    AstNode* binary = parser.makeBinaryExpression(TokenKind::Plus, integer, integer);
    AstNode* assignment = parser.makeAssignmentExpression(id, integer);
    AstNode* statement = parser.makeExpressionStatement(call);
    TypeSyntax* type = parser.arena_.make<TypeSyntax>(range, TokenKind::KwInt, "int");
    AstNode* variable = parser.makeVariableDeclaration(type, "value", integer, false, true);
    AstNode* error = parser.makeErrorExpression();
    context.expect(id->kind() == AstNodeKind::IdentifierExpression && id->text() == "item" &&
            integer->kind() == AstNodeKind::IntegerLiteralExpression && integer->integerValue() == 7 &&
            member->kind() == AstNodeKind::MemberExpression && member->first() == id && member->text() == "Field" &&
            call->kind() == AstNodeKind::CallExpression && call->first() == member &&
            index->kind() == AstNodeKind::IndexExpression && index->first() == id && index->second() == integer &&
            binary->kind() == AstNodeKind::BinaryExpression && binary->tokenKind() == TokenKind::Plus &&
            assignment->kind() == AstNodeKind::AssignmentExpression && statement->first() == call &&
            variable->kind() == AstNodeKind::VariableDeclarationStatement && variable->typeSyntax() == type &&
            variable->first() == integer && variable->text() == "value" && variable->flag() &&
            error->text() == "<error>" && id->range().begin() == range.begin(),
        "TP-PAR-513 Parser make helpers must preserve exact kind, topology, payload, operator, and range");
}

void testCloneTypeRepresentativeAndExposeSeamLimit(TestContext& context)
{
    ParserFixture fixture("extension ref Box<int> { int Read() { return this.Value; } }");
    AstNode* root = fixture.parse();
    AstNode* extension = firstChild(root);
    AstNode* function = firstChild(extension);
    AstNode* receiver = firstChild(function);
    const TypeSyntax* source = extension != nullptr ? extension->typeSyntax() : nullptr;
    const TypeSyntax* clone = receiver != nullptr ? receiver->typeSyntax() : nullptr;
    const bool representative = source != nullptr && clone != nullptr && source != clone &&
        source->baseName() == clone->baseName() && source->typeArguments().size() == 1 &&
        clone->typeArguments().size() == 1 && source->typeArguments()[0] != clone->typeArguments()[0] &&
        source->typeArguments()[0]->baseName() == clone->typeArguments()[0]->baseName();
    context.expect(representative,
        "TP-PAR-514 observable extension cloning must deep-copy the receiver and nested type arguments");
    context.expect(false,
        "TP-PAR-514 TEST-INFRASTRUCTURE: cloneParsedType has anonymous-namespace linkage; all fields cannot be injected without a product seam");
}

void testCloneAstTopology(TestContext& context)
{
    ParserFixture fixture(
        "match (source) { Left(var value) | Right(var value) when guard => flag ? F(a,b) : G(c,d), _ => zero }");
    AstNode* match = fixture.parser().parseExpression();
    std::vector<const AstNode*> arms;
    for (const AstNode* node : flatten(match))
    {
        if (node->kind() == AstNodeKind::MatchArmExpression && node->third() != nullptr &&
            node->third()->kind() == AstNodeKind::ConditionalExpression) arms.push_back(node);
    }
    const AstNode* first = arms.size() >= 2 ? arms[0] : nullptr;
    const AstNode* second = arms.size() >= 2 ? arms[1] : nullptr;
    context.expect(arms.size() >= 2, "TP-PAR-515 OR-pattern must expand to at least two arm nodes");
    context.expect(first != nullptr && second != nullptr && first != second,
        "TP-PAR-515 expanded alternatives must be distinct nodes");
    context.expect(first != nullptr && second != nullptr && first->second() != nullptr &&
            second->second() != nullptr && first->second() != second->second(),
        "TP-PAR-515 cloned guards must not alias");
    context.expect(first != nullptr && second != nullptr && first->third() != nullptr &&
            second->third() != nullptr && first->third() != second->third(),
        "TP-PAR-515 cloned values must not alias");
    context.expect(first != nullptr && second != nullptr && first->third() != nullptr &&
            second->third() != nullptr && first->third()->first() != second->third()->first(),
        "TP-PAR-515 first/second/third descendants must not alias across clones");
}

void testCloneGenericConstraintSeam(TestContext& context)
{
    context.expect(false,
        "TP-PAR-516 TEST-INFRASTRUCTURE: generic constraint/projection cloning is anonymous and no public parser path carries those records through cloneParsedNode");
}

void testCloneNullThroughOrPattern(TestContext& context)
{
    ParserFixture fixture("match (source) { Left(var value) | Right(var value) => value, _ => zero }");
    AstNode* match = fixture.parser().parseExpression();
    std::vector<const AstNode*> alternatives;
    for (const AstNode* node : flatten(match))
    {
        if (node->kind() == AstNodeKind::MatchArmExpression && node->third() != nullptr &&
            node->third()->text() == "value") alternatives.push_back(node);
    }
    context.expect(alternatives.size() == 2 && alternatives[0]->second() == nullptr &&
            alternatives[1]->second() == nullptr,
        "TP-PAR-517 cloning a null optional guard must preserve null on every expanded alternative");
}

void testFloatingPointPropagation(TestContext& context)
{
    ParserFixture fixture("double F() fp(strict) { return 1.0 + 2.0; }");
    AstNode* root = fixture.parse();
    const AstNode* function = firstKind(root, AstNodeKind::FunctionDeclaration);
    bool allStrict = function != nullptr && function->first() != nullptr;
    for (const AstNode* node : flatten(function != nullptr ? function->first() : nullptr))
    {
        allStrict = allStrict && node->floatingPointMode() == FloatingPointModeSyntaxKind::Strict;
    }
    context.expect(allStrict && fixture.diagnostics().diagnostics().empty(),
        "TP-PAR-518 callable FP mode must propagate to every Inherit descendant");
}

void testFloatingPointOverrideBoundary(TestContext& context)
{
    ParserFixture fixture(
        "double F() fp(strict) { fp(fast) { return 1.0 + 2.0; } return 3.0; }");
    AstNode* root = fixture.parse();
    const AstNode* function = firstKind(root, AstNodeKind::FunctionDeclaration);
    const AstNode* fastBlock = nullptr;
    for (const AstNode* node : flatten(function != nullptr ? function->first() : nullptr))
    {
        if (node->auxiliaryText() == "fp-scope:fast") fastBlock = node;
    }
    bool fastBranch = fastBlock != nullptr;
    for (const AstNode* node : flatten(fastBlock))
    {
        fastBranch = fastBranch && node->floatingPointMode() == FloatingPointModeSyntaxKind::Fast;
    }
    context.expect(fastBranch,
        "TP-PAR-519 an explicit child FP mode must form an override boundary and retain its branch mode");
}

void testSignedIntegerNonnegative(TestContext& context)
{
    ParserFixture zero("0");
    ParserFixture maximum("9223372036854775807");
    const Parser::Pattern first = zero.parser().parsePattern(false);
    const Parser::Pattern second = maximum.parser().parsePattern(false);
    context.expect(first.firstInteger == std::optional<std::int64_t>(0) &&
            second.firstInteger == std::optional<std::int64_t>(std::numeric_limits<std::int64_t>::max()),
        "TP-PAR-520 zero and INT64_MAX literals must retain exact signed values");
}

void testSignedIntegerNegative(TestContext& context)
{
    ParserFixture negative("-1");
    ParserFixture minimum("-9223372036854775808");
    const Parser::Pattern first = negative.parser().parsePattern(false);
    const Parser::Pattern second = minimum.parser().parsePattern(false);
    context.expect(first.firstInteger == std::optional<std::int64_t>(-1) &&
            second.firstInteger == std::optional<std::int64_t>(std::numeric_limits<std::int64_t>::min()),
        "TP-PAR-521 unary minus and INT64_MIN must retain exact signed values without overflow");
}

void testSignedIntegerOverflow(TestContext& context)
{
    ParserFixture positive("9223372036854775808");
    ParserFixture negative("-9223372036854775809");
    const Parser::Pattern first = positive.parser().parsePattern(false);
    const Parser::Pattern second = negative.parser().parsePattern(false);
    context.expect(!first.firstInteger.has_value() && !second.firstInteger.has_value(),
        "TP-PAR-522 magnitudes outside int64 must not publish a signed constant");
}

void testSignedIntegerNonLiteral(TestContext& context)
{
    ParserFixture fixture("value");
    const Parser::Pattern pattern = fixture.parser().parsePattern(false);
    context.expect(!pattern.firstInteger.has_value(),
        "TP-PAR-523 a non-literal pattern must not publish a signed integer constant");
}

void testOperatorMappingsAndExposeFallbackLimit(TestContext& context)
{
    ParserFixture fixture(
        "class C { public static C operator +(C a, C b) { return a; } "
        "public static C operator -(C a) { return a; } "
        "public static bool operator ==(C a, C b) { return true; } }");
    AstNode* root = fixture.parse();
    bool addition = false;
    bool negation = false;
    bool equality = false;
    for (const AstNode* node : flatten(root))
    {
        addition = addition || node->text() == "op_Addition";
        negation = negation || node->text() == "op_UnaryNegation";
        equality = equality || node->text() == "op_Equality";
    }
    context.expect(addition && negation && equality,
        "TP-PAR-524 supported operator kind/arity pairs must map to exact metadata names");
    context.expect(false,
        "TP-PAR-524 TEST-INFRASTRUCTURE: op_Invalid fallback is anonymous and rejected before the public declaration path; it needs a product test seam");
}
}

void registerParserConstructionCloneFinalModule(TestRegistry& registry)
{
    addFinalParserCase(registry, "TP-PAR-513", "CP-PAR-041", testMakeHelpers);
    addFinalParserCase(registry, "TP-PAR-514", "CP-PAR-041", testCloneTypeRepresentativeAndExposeSeamLimit,
        "PR", TestDisposition::KnownProductDefect, {"infrastructure-special"});
    addFinalParserCase(registry, "TP-PAR-515", "CP-PAR-041", testCloneAstTopology);
    addFinalParserCase(registry, "TP-PAR-516", "CP-PAR-041", testCloneGenericConstraintSeam,
        "PR", TestDisposition::KnownProductDefect, {"infrastructure-special"});
    addFinalParserCase(registry, "TP-PAR-517", "CP-PAR-041", testCloneNullThroughOrPattern);
    addFinalParserCase(registry, "TP-PAR-518", "CP-PAR-041", testFloatingPointPropagation);
    addFinalParserCase(registry, "TP-PAR-519", "CP-PAR-041", testFloatingPointOverrideBoundary);
    addFinalParserCase(registry, "TP-PAR-520", "CP-PAR-041", testSignedIntegerNonnegative);
    addFinalParserCase(registry, "TP-PAR-521", "CP-PAR-041", testSignedIntegerNegative);
    addFinalParserCase(registry, "TP-PAR-522", "CP-PAR-041", testSignedIntegerOverflow);
    addFinalParserCase(registry, "TP-PAR-523", "CP-PAR-041", testSignedIntegerNonLiteral);
    addFinalParserCase(registry, "TP-PAR-524", "CP-PAR-041", testOperatorMappingsAndExposeFallbackLimit,
        "PR", TestDisposition::KnownProductDefect, {"infrastructure-special"});
}
}
