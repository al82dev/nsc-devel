#include "ParserCallableTestSupport.h"

#include <array>
#include <string>

namespace nsc::suite
{
namespace
{
using namespace parser_callable_support;

void testOperatorBodylessContexts(TestContext& context)
{
    DeclarationFixture interfaceFixture(
        "trait I { public static I operator +(I left, I right); }");
    DeclarationFixture classFixture(
        "class C { public static C operator +(C left, C right); }");
    AstNode* const interfaceOperator = member(interfaceFixture);
    AstNode* const classOperator = member(classFixture);
    context.expect(interfaceOperator != nullptr && interfaceOperator->first() == nullptr &&
            interfaceOperator->hasModifier(TokenKind::KwAbstract) && interfaceFixture.diagnostics().empty(),
        "TP-PAR-251 trait operator must accept a bodyless declaration");
    context.expect(classOperator != nullptr && classOperator->first() == nullptr &&
            classFixture.diagnosticCount(DiagnosticId::ExpectedToken) == 1,
        "TP-PAR-251 concrete operator must reject a bodyless declaration");
}

void testImplicitConversion(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { public static implicit operator int(C value) { return 0; } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::MethodDeclaration &&
            node->text() == "op_Implicit" && node->auxiliaryText() == "conversion:implicit" &&
            node->typeSyntax() != nullptr && node->typeSyntax()->baseName() == "int" &&
            node->children().size() == 1 && node->children()[0]->text() == "value" &&
            node->first() != nullptr && node->hasModifier(TokenKind::KwPublic) &&
            node->hasModifier(TokenKind::KwStatic),
        "TP-PAR-252 implicit conversion must preserve name, flags, return type, parameter, and body");
}

void testExplicitConversion(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { public static explicit operator long(C value) { return 0; } }");
    AstNode* const node = member(fixture);
    context.expect(node != nullptr && node->kind() == AstNodeKind::MethodDeclaration &&
            node->text() == "op_Explicit" && node->auxiliaryText() == "conversion:explicit" &&
            node->typeSyntax() != nullptr && node->typeSyntax()->baseName() == "long" &&
            node->children().size() == 1 && node->children()[0]->text() == "value" &&
            node->first() != nullptr && node->hasModifier(TokenKind::KwPublic) &&
            node->hasModifier(TokenKind::KwStatic),
        "TP-PAR-253 explicit conversion must preserve name, flags, return type, parameter, and body");
}

void testInvalidConversion(TestContext& context)
{
    DeclarationFixture fixture("class C { implicit operator int() { return 0; } }");
    context.expect(fixture.diagnosticCount(DiagnosticId::InvalidArgumentCount) == 1 &&
            diagnosticContains(fixture, "must be declared public") &&
            diagnosticContains(fixture, "must be declared static"),
        "TP-PAR-254 invalid conversion arity and missing required modifiers must diagnose independently");
}

struct OperatorExpectation final
{
    const char* spelling;
    std::size_t arity;
    const char* name;
};

constexpr std::array<OperatorExpectation, 20> kOperators{{
    {"+", 1, "op_UnaryPlus"}, {"-", 1, "op_UnaryNegation"},
    {"!", 1, "op_LogicalNot"}, {"~", 1, "op_OnesComplement"},
    {"+", 2, "op_Addition"}, {"-", 2, "op_Subtraction"},
    {"*", 2, "op_Multiply"}, {"/", 2, "op_Division"},
    {"%", 2, "op_Modulus"}, {"&", 2, "op_BitwiseAnd"},
    {"|", 2, "op_BitwiseOr"}, {"^", 2, "op_ExclusiveOr"},
    {"<<", 2, "op_LeftShift"}, {">>", 2, "op_RightShift"},
    {"==", 2, "op_Equality"}, {"!=", 2, "op_Inequality"},
    {"<", 2, "op_LessThan"}, {"<=", 2, "op_LessThanOrEqual"},
    {">", 2, "op_GreaterThan"}, {">=", 2, "op_GreaterThanOrEqual"},
}};

void testOperatorMappings(TestContext& context)
{
    std::string source = "class C { ";
    for (const OperatorExpectation& operation : kOperators)
    {
        source += "public static C operator ";
        source += operation.spelling;
        source += operation.arity == 1 ? "(C left) { return left; } "
                                      : "(C left, C right) { return left; } ";
    }
    source += "}";
    DeclarationFixture fixture(std::move(source));
    AstNode* const type = declaration(fixture);
    context.expect(type != nullptr && type->children().size() == kOperators.size(),
        "TP-PAR-255 every contractual operator spelling must produce one method");
    if (type == nullptr || type->children().size() != kOperators.size()) return;
    for (std::size_t index = 0; index < kOperators.size(); ++index)
    {
        context.expect(type->children()[index]->text() == kOperators[index].name,
            "TP-PAR-255 operator spelling and arity must map to its exact canonical method name");
    }
}

void testUnsupportedOperator(TestContext& context)
{
    DeclarationFixture fixture(
        "class C { public static C operator &&(C left, C right) { return left; } int kept; }");
    AstNode* const type = declaration(fixture);
    bool foundInvalid = false;
    if (type != nullptr)
    {
        for (const AstNode* const child : type->children())
            foundInvalid = foundInvalid || (child != nullptr && child->text() == "op_Invalid");
    }
    context.expect(!foundInvalid && diagnosticContains(fixture, "short-circuit operator '&&' is intrinsic"),
        "TP-PAR-256 unsupported operator kind must diagnose and never create op_Invalid");
}
}

void registerParserOperatorConversionModule(TestRegistry& registry)
{
    addCallableCase(registry, "TP-PAR-251", "CP-PAR-024", testOperatorBodylessContexts);
    addCallableCase(registry, "TP-PAR-252", "CP-PAR-024", testImplicitConversion);
    addCallableCase(registry, "TP-PAR-253", "CP-PAR-024", testExplicitConversion);
    addCallableCase(registry, "TP-PAR-254", "CP-PAR-024", testInvalidConversion);
    addCallableCase(registry, "TP-PAR-255", "CP-PAR-024", testOperatorMappings);
    addCallableCase(registry, "TP-PAR-256", "CP-PAR-024", testUnsupportedOperator);
}
}
