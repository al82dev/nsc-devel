#include "ParserExpressionTestSupport.h"

#include <array>

namespace nsc::suite
{
namespace
{
using namespace expression_support;
using namespace parser_support;

// Verifies every supported prefix token produces one unary node. Complexity is O(n).
void testPrefixOperators(TestContext& context)
{
    const std::array<std::pair<std::string, TokenKind>, 6> cases{{
        {"+x", TokenKind::Plus}, {"-x", TokenKind::Minus}, {"!x", TokenKind::Bang},
        {"~x", TokenKind::Tilde}, {"++x", TokenKind::PlusPlus}, {"--x", TokenKind::MinusMinus}}};
    for (const auto& [source, token] : cases)
    {
        ParserFixture fixture(source);
        AstNode* node = parse(fixture);
        context.expect(node != nullptr && node->kind() == AstNodeKind::UnaryExpression &&
                node->tokenKind() == token && node->first() != nullptr,
            "TP-PAR-457 prefix operator must retain exact token and operand");
    }
}

// Verifies dereference and address-of are distinct unary token forms. Complexity is O(n).
void testAddressForms(TestContext& context)
{
    ParserFixture dereference("*pointer");
    ParserFixture address("&value");
    AstNode* star = parse(dereference);
    AstNode* amp = parse(address);
    context.expect(star != nullptr && star->tokenKind() == TokenKind::Star && amp != nullptr &&
            amp->tokenKind() == TokenKind::Amp,
        "TP-PAR-458 dereference and address-of must retain distinct operator tokens");
}

// Verifies move owns its operand through a dedicated AST kind. Complexity is O(n).
void testMove(TestContext& context)
{
    ParserFixture fixture("move(value)");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::MoveExpression &&
            node->first() != nullptr && node->first()->text() == "value",
        "TP-PAR-459 move must preserve its operand in MoveExpression");
}

// Verifies caret produces the dedicated from-end node. Complexity is O(n).
void testFromEnd(TestContext& context)
{
    ParserFixture fixture("^2");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::FromEndExpression &&
            node->tokenKind() == TokenKind::Caret && node->first()->integerValue() == 2,
        "TP-PAR-460 from-end expression must retain caret and operand");
}

// Verifies a parenthesized cast retains target type and value. Complexity is O(n).
void testParenthesizedCast(TestContext& context)
{
    ParserFixture fixture("(int)value");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::CastExpression &&
            node->typeSyntax() != nullptr && node->typeSyntax()->baseName() == "int" &&
            node->first() != nullptr && node->first()->text() == "value",
        "TP-PAR-461 parenthesized cast must retain type and operand");
}

// Verifies static_cast has its exact contextual marker. Complexity is O(n).
void testStaticCast(TestContext& context)
{
    ParserFixture fixture("static_cast<int>(value)");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::CastExpression &&
            node->auxiliaryText() == "cast:static_cast",
        "TP-PAR-462 static_cast must retain its exact marker");
}

// Verifies reinterpret_cast has its exact contextual marker. Complexity is O(n).
void testReinterpretCast(TestContext& context)
{
    ParserFixture fixture("reinterpret_cast<int*>(value)");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::CastExpression &&
            node->auxiliaryText() == "cast:reinterpret_cast",
        "TP-PAR-463 reinterpret_cast must retain its exact marker");
}

// Verifies const_cast has its exact contextual marker. Complexity is O(n).
void testConstCast(TestContext& context)
{
    ParserFixture fixture("const_cast<int*>(value)");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::CastExpression &&
            node->auxiliaryText() == "cast:const_cast",
        "TP-PAR-464 const_cast must retain its exact marker");
}

// Verifies safe-cast and type-test operators have independent AST kinds. Complexity is O(n).
void testSafeCasts(TestContext& context)
{
    ParserFixture safe("value as Target");
    ParserFixture test("value is Target");
    AstNode* asNode = parse(safe);
    AstNode* isNode = parse(test);
    context.expect(asNode != nullptr && asNode->kind() == AstNodeKind::SafeCastExpression &&
            isNode != nullptr && isNode->kind() == AstNodeKind::TypeTestExpression,
        "TP-PAR-465 as and is must remain distinct cast/test nodes");
}

// Verifies sizeof selects type and expression shapes by complete lookahead. Complexity is O(n).
void testSizeofForms(TestContext& context)
{
    ParserFixture type("sizeof(int)");
    ParserFixture expression("sizeof(value + 1)");
    AstNode* typeNode = parse(type);
    AstNode* expressionNode = parse(expression);
    context.expect(typeNode != nullptr && typeNode->kind() == AstNodeKind::SizeofTypeExpression &&
            typeNode->typeSyntax() != nullptr && expressionNode != nullptr &&
            expressionNode->kind() == AstNodeKind::SizeofExpression && expressionNode->first() != nullptr,
        "TP-PAR-466 sizeof must distinguish complete type and expression forms");
}

// Verifies alignof selects type and expression shapes independently. Complexity is O(n).
void testAlignofForms(TestContext& context)
{
    ParserFixture type("alignof(long)");
    ParserFixture expression("alignof(value + 1)");
    AstNode* typeNode = parse(type);
    AstNode* expressionNode = parse(expression);
    context.expect(typeNode != nullptr && typeNode->kind() == AstNodeKind::AlignofTypeExpression &&
            expressionNode != nullptr && expressionNode->kind() == AstNodeKind::AlignofExpression,
        "TP-PAR-467 alignof must distinguish type and expression forms");
}

// Verifies offsetof retains target type and exact member spelling. Complexity is O(n).
void testOffsetof(TestContext& context)
{
    ParserFixture fixture("offsetof(Packet, field)");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::OffsetofExpression &&
            node->typeSyntax() != nullptr && node->typeSyntax()->baseName() == "Packet" &&
            node->text() == "field",
        "TP-PAR-468 offsetof must retain target type and member");
}

// Verifies typed default is supported and bare default remains explicitly invalid. Complexity is O(n).
void testDefaultForms(TestContext& context)
{
    ParserFixture typed("default(int)");
    ParserFixture bare("default");
    AstNode* typedNode = parse(typed);
    parse(bare);
    context.expect(typedNode != nullptr && typedNode->kind() == AstNodeKind::DefaultValueExpression &&
            typedNode->typeSyntax() != nullptr && typedNode->typeSyntax()->baseName() == "int",
        "TP-PAR-469 default(T) must retain its target type");
    context.expect(diagnosticCount(bare.diagnostics(), DiagnosticId::ExpectedToken) >= 1,
        "TP-PAR-469 bare default is outside the current typed-default grammar");
}

// Verifies object, array, and placement construction produce distinct node contracts. Complexity is O(n).
void testNewForms(TestContext& context)
{
    ParserFixture object("new Item(1)");
    ParserFixture array("new int[4]");
    ParserFixture initialized("new int[] { 1, 2 }");
    ParserFixture placement("new (storage) Item(1)");
    AstNode* objectNode = parse(object);
    AstNode* arrayNode = parse(array);
    AstNode* initializedNode = parse(initialized);
    AstNode* placementNode = parse(placement);
    context.expect(objectNode != nullptr && objectNode->kind() == AstNodeKind::ObjectCreationExpression &&
            objectNode->children().size() == 1 && arrayNode != nullptr &&
            arrayNode->kind() == AstNodeKind::NewExpression && arrayNode->first() != nullptr,
        "TP-PAR-470 object and array construction must retain distinct shapes");
    context.expect(initializedNode != nullptr && initializedNode->kind() == AstNodeKind::NewExpression &&
            initializedNode->auxiliaryText() == "inferred-array-initializer" &&
            initializedNode->children().size() == 2,
        "TP-PAR-470 inferred array initializer must retain ordered elements");
    context.expect(placementNode != nullptr && placementNode->kind() ==
            AstNodeKind::PlacementConstructionExpression && placementNode->first() != nullptr &&
            placementNode->children().size() == 1,
        "TP-PAR-470 placement construction must separate storage from arguments");
}

// Verifies delete and destroy retain distinct lifetime operations and operands. Complexity is O(n).
void testDeleteDestroy(TestContext& context)
{
    ParserFixture deletion("delete value");
    ParserFixture destruction("destroy value");
    AstNode* deleted = parse(deletion);
    AstNode* destroyed = parse(destruction);
    context.expect(deleted != nullptr && deleted->kind() == AstNodeKind::DeleteExpression &&
            destroyed != nullptr && destroyed->kind() == AstNodeKind::DestroyExpression &&
            deleted->first() != nullptr && destroyed->first() != nullptr,
        "TP-PAR-471 delete and destroy must remain separate operand-bearing nodes");
}

// Verifies stackalloc retains element type and count; initializer syntax is not current. Complexity is O(n).
void testStackalloc(TestContext& context)
{
    ParserFixture fixture("stackalloc int[8]");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::StackallocExpression &&
            node->typeSyntax() != nullptr && node->typeSyntax()->baseName() == "int" &&
            node->first() != nullptr && node->first()->integerValue() == 8,
        "TP-PAR-472 stackalloc must retain type and count under the current grammar");
    ParserFixture initializer("stackalloc int[2] { 1, 2 }");
    parse(initializer);
    context.expect(initializer.parser().at(TokenKind::LeftBrace),
        "TP-PAR-472 stackalloc initializer is not consumed by the current stackalloc grammar");
}

// Verifies prefix chains associate from the right and cover their complete range. Complexity is O(n).
void testUnaryChain(TestContext& context)
{
    ParserFixture fixture("!-~value");
    AstNode* outer = parse(fixture);
    context.expect(outer != nullptr && outer->tokenKind() == TokenKind::Bang && outer->first() != nullptr &&
            outer->first()->tokenKind() == TokenKind::Minus && outer->first()->first() != nullptr &&
            outer->first()->first()->tokenKind() == TokenKind::Tilde,
        "TP-PAR-473 unary chain must associate from the right");
    context.expect(outer != nullptr && outer->range().begin().offset() == 0 && outer->range().end().offset() == 8,
        "TP-PAR-473 unary chain range must cover every prefix and operand");
}

// Verifies a missing unary operand produces a diagnostic and finite error node. Complexity is O(n).
void testMissingUnaryOperand(TestContext& context)
{
    ParserFixture fixture("!");
    AstNode* node = parse(fixture);
    context.expect(node != nullptr && diagnosticCount(fixture.diagnostics(), DiagnosticId::ExpectedExpression) >= 1,
        "TP-PAR-474 missing unary operand must diagnose and terminate");
}

// Verifies a large prefix chain is characterized without recursive test construction. Complexity is O(n).
void testLargeUnaryDepth(TestContext& context)
{
    std::string source(64, '!');
    source += "value";
    ParserFixture fixture(source);
    AstNode* node = parse(fixture);
    std::size_t depth = 0;
    while (node != nullptr && node->kind() == AstNodeKind::UnaryExpression)
    {
        ++depth;
        node = node->first();
    }
    context.expect(depth == 64 && node != nullptr && node->text() == "value",
        "TP-PAR-475 64-prefix chain must parse completely with a characterized finite depth");
}

// Verifies allocation of void reports InvalidType instead of silently constructing. Complexity is O(n).
void testVoidAllocation(TestContext& context)
{
    ParserFixture fixture("new void()");
    parse(fixture);
    context.expect(diagnosticCount(fixture.diagnostics(), DiagnosticId::InvalidType) >= 1,
        "TP-PAR-476 void allocation must report InvalidType");
}
}

// Registers TP-PAR-457..476 for unary and allocation expressions.
void registerParserUnaryExpressionModule(TestRegistry& registry)
{
    addCase(registry, "TP-PAR-457", "CP-PAR-038", testPrefixOperators);
    addCase(registry, "TP-PAR-458", "CP-PAR-038", testAddressForms);
    addCase(registry, "TP-PAR-459", "CP-PAR-038", testMove);
    addCase(registry, "TP-PAR-460", "CP-PAR-038", testFromEnd);
    addCase(registry, "TP-PAR-461", "CP-PAR-038", testParenthesizedCast);
    addCase(registry, "TP-PAR-462", "CP-PAR-038", testStaticCast);
    addCase(registry, "TP-PAR-463", "CP-PAR-038", testReinterpretCast);
    addCase(registry, "TP-PAR-464", "CP-PAR-038", testConstCast);
    addCase(registry, "TP-PAR-465", "CP-PAR-038", testSafeCasts);
    addCase(registry, "TP-PAR-466", "CP-PAR-038", testSizeofForms);
    addCase(registry, "TP-PAR-467", "CP-PAR-038", testAlignofForms);
    addCase(registry, "TP-PAR-468", "CP-PAR-038", testOffsetof);
    addCase(registry, "TP-PAR-469", "CP-PAR-038", testDefaultForms);
    addCase(registry, "TP-PAR-470", "CP-PAR-038", testNewForms);
    addCase(registry, "TP-PAR-471", "CP-PAR-038", testDeleteDestroy);
    addCase(registry, "TP-PAR-472", "CP-PAR-038", testStackalloc);
    addCase(registry, "TP-PAR-473", "CP-PAR-038", testUnaryChain);
    addCase(registry, "TP-PAR-474", "CP-PAR-038", testMissingUnaryOperand);
    addCase(registry, "TP-PAR-475", "CP-PAR-038", testLargeUnaryDepth, "NIGHTLY");
    addCase(registry, "TP-PAR-476", "CP-PAR-038", testVoidAllocation, "PR");
}
}
