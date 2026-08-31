#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <array>
#include <limits>

namespace nativesharp::frontend
{
namespace
{
using lexer::TokenKind;

// Profundidad maxima de expansion. Un campo `const` puede definirse en terminos de otro, y nada
// impide sintacticamente un ciclo (`A = B; B = A;`). El limite corta el ciclo sin necesitar que el
// modelo de miembros demuestre que no existe.
constexpr std::size_t kMaximumDepth = 32;

struct IntegerTypeInfo final
{
    std::string_view key;
    unsigned width;
    bool isSigned;
};

// CC1-100: `builtin:char` mide OCHO bits. Es la misma decision que `constantFits`, escrita una
// segunda vez aqui a proposito porque este evaluador tiene que poder responder sin depender del
// clasificador de conversiones -- pero las dos tablas dicen lo mismo y el test
// nsc.cc1.integer-evaluator lo fija.
constexpr std::array<IntegerTypeInfo, 10> kIntegerTypes = {{
    {"builtin:byte", 8, false},
    {"builtin:char", 8, false},
    {"builtin:short", 16, true},
    {"builtin:ushort", 16, false},
    {"builtin:int", 32, true},
    {"builtin:uint", 32, false},
    {"builtin:long", 64, true},
    {"builtin:ulong", 64, false},
    {"builtin:nint", 64, true},
    {"builtin:nuint", 64, false}}};

const IntegerTypeInfo* findIntegerType(std::string_view typeKey) noexcept
{
    for (const IntegerTypeInfo& info : kIntegerTypes)
    {
        if (info.key == typeKey) return &info;
    }
    return nullptr;
}

// El tipo de un literal entero, con las mismas reglas de sufijo que usa el modelo semantico.
// Duplicarlo aqui seria justo el error que este archivo existe para eliminar, asi que la regla se
// mantiene deliberadamente identica y el test la fija contra casos concretos.
std::string integerLiteralTypeOf(const ast::AstNode* node)
{
    bool unsignedSuffix = false;
    bool longSuffix = false;
    for (const char value : node->text())
    {
        if (value == 'u' || value == 'U') unsignedSuffix = true;
        if (value == 'l' || value == 'L') longSuffix = true;
    }

    if (unsignedSuffix && longSuffix) return "builtin:ulong";
    if (unsignedSuffix)
    {
        return node->integerValue() <= std::numeric_limits<std::uint32_t>::max()
            ? "builtin:uint"
            : "builtin:ulong";
    }
    if (longSuffix) return "builtin:long";
    return node->integerValue() <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())
        ? "builtin:int"
        : "builtin:long";
}
}

unsigned ProjectIntegerEvaluator::integerWidth(std::string_view typeKey) noexcept
{
    const IntegerTypeInfo* info = findIntegerType(typeKey);
    return info != nullptr ? info->width : 0U;
}

bool ProjectIntegerEvaluator::isSignedInteger(std::string_view typeKey) noexcept
{
    const IntegerTypeInfo* info = findIntegerType(typeKey);
    return info != nullptr && info->isSigned;
}

bool ProjectIntegerEvaluator::isIntegerType(std::string_view typeKey) noexcept
{
    return findIntegerType(typeKey) != nullptr;
}

// LA OPERACION CENTRAL. Trunca al ancho del tipo y extiende el signo si el tipo lo tiene.
//
// Es lo que convierte "el frontend cree que vale 321" en "el backend va a materializar 65", y por
// tanto lo que permite que la comparacion de dos etiquetas de `switch` signifique lo mismo antes y
// despues del lowering. Sin esto, C-001: el frontend ve dos valores distintos, LLVM recibe dos
// casos identicos, y eso es comportamiento indefinido en la instruccion `switch`.
std::uint64_t ProjectIntegerEvaluator::normalize(std::uint64_t bits, std::string_view typeKey) noexcept
{
    const IntegerTypeInfo* info = findIntegerType(typeKey);
    if (info == nullptr || info->width == 0 || info->width > 64) return bits;
    if (info->width == 64) return bits;

    const std::uint64_t mask = (std::uint64_t{1} << info->width) - std::uint64_t{1};
    const std::uint64_t truncated = bits & mask;
    if (!info->isSigned) return truncated;

    // Extension de signo: si el bit mas alto del ancho real esta puesto, rellenar hacia arriba.
    const std::uint64_t signBit = std::uint64_t{1} << (info->width - 1U);
    return (truncated & signBit) != 0 ? (truncated | ~mask) : truncated;
}

std::optional<ProjectIntegerValue> ProjectIntegerEvaluator::evaluate(
    const ast::AstNode* expression,
    std::optional<std::size_t> declaringTypeOrdinal) const
{
    return evaluateNode(expression, 0, declaringTypeOrdinal);
}

std::optional<ProjectIntegerValue> ProjectIntegerEvaluator::evaluateNode(
    const ast::AstNode* expression,
    std::size_t depth,
    std::optional<std::size_t> declaringTypeOrdinal) const
{
    if (expression == nullptr || depth > kMaximumDepth) return std::nullopt;

    switch (expression->kind())
    {
        case ast::AstNodeKind::IntegerLiteralExpression:
        {
            ProjectIntegerValue value;
            value.typeKey = integerLiteralTypeOf(expression);
            value.bits = normalize(expression->integerValue(), value.typeKey);
            return value;
        }

        case ast::AstNodeKind::CharacterLiteralExpression:
        {
            ProjectIntegerValue value;
            value.typeKey = "builtin:char";
            value.bits = normalize(expression->integerValue(), value.typeKey);
            return value;
        }

        case ast::AstNodeKind::CastExpression:
        {
            // CC1-104. El cast es el punto donde un valor CAMBIA de ancho y de signedness, y donde
            // el evaluador anterior simplemente devolvia el operando intacto -- de ahi C-001. Aqui
            // el operando se re-normaliza al tipo destino, que es lo mismo que hara el backend.
            const std::optional<ProjectIntegerValue> operand =
                evaluateNode(expression->first(), depth + 1, declaringTypeOrdinal);
            if (!operand.has_value()) return std::nullopt;
            if (resolver_ == nullptr || expression->typeSyntax() == nullptr) return std::nullopt;
            const std::string targetKey = resolver_->resolveTypeKey(expression->typeSyntax());
            if (!isIntegerType(targetKey)) return std::nullopt;

            ProjectIntegerValue value;
            value.typeKey = targetKey;
            value.bits = normalize(operand->bits, targetKey);
            return value;
        }

        case ast::AstNodeKind::UnaryExpression:
            return evaluateUnary(expression, depth, declaringTypeOrdinal);

        case ast::AstNodeKind::BinaryExpression:
            return evaluateBinary(expression, depth, declaringTypeOrdinal);

        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
            return evaluateMember(expression, depth, declaringTypeOrdinal);

        default:
            return std::nullopt;
    }
}

std::optional<ProjectIntegerValue> ProjectIntegerEvaluator::evaluateUnary(
    const ast::AstNode* expression,
    std::size_t depth,
    std::optional<std::size_t> declaringTypeOrdinal) const
{
    const std::optional<ProjectIntegerValue> operand =
        evaluateNode(expression->first(), depth + 1, declaringTypeOrdinal);
    if (!operand.has_value()) return std::nullopt;

    ProjectIntegerValue value;
    value.typeKey = operand->typeKey;

    switch (expression->tokenKind())
    {
        case TokenKind::Plus:
            value.bits = operand->bits;
            break;
        case TokenKind::Minus:
            // Sobre el gemelo sin signo: la negacion del minimo con signo envuelve en vez de ser
            // comportamiento indefinido, y `normalize` la devuelve al rango del tipo.
            value.bits = normalize(std::uint64_t{0} - operand->bits, value.typeKey);
            break;
        case TokenKind::Tilde:
            value.bits = normalize(~operand->bits, value.typeKey);
            break;
        default:
            return std::nullopt;
    }
    return value;
}

std::optional<ProjectIntegerValue> ProjectIntegerEvaluator::evaluateBinary(
    const ast::AstNode* expression,
    std::size_t depth,
    std::optional<std::size_t> declaringTypeOrdinal) const
{
    const std::optional<ProjectIntegerValue> left =
        evaluateNode(expression->first(), depth + 1, declaringTypeOrdinal);
    if (!left.has_value()) return std::nullopt;
    const std::optional<ProjectIntegerValue> right =
        evaluateNode(expression->second(), depth + 1, declaringTypeOrdinal);
    if (!right.has_value()) return std::nullopt;

    const TokenKind operation = expression->tokenKind();

    // CC1-103 / C-004. Un SHIFT no toma el tipo comun de sus dos operandos: el resultado tiene el
    // tipo del OPERANDO IZQUIERDO, y el contador se valida contra el ancho de ese tipo, no contra
    // los 64 bits de la representacion interna del compilador. Meter el shift por el camino del
    // tipo comun es exactamente lo que produjo `1u << 28` tipado como double en su dia.
    if (operation == TokenKind::LessLess || operation == TokenKind::GreaterGreater)
    {
        const unsigned width = integerWidth(left->typeKey);
        if (width == 0) return std::nullopt;
        const std::int64_t count = right->asSigned();
        if (count < 0 || static_cast<unsigned>(count) >= width) return std::nullopt;
        const unsigned shift = static_cast<unsigned>(count);

        ProjectIntegerValue value;
        value.typeKey = left->typeKey;
        if (operation == TokenKind::LessLess)
        {
            value.bits = normalize(left->bits << shift, value.typeKey);
        }
        else if (isSignedInteger(left->typeKey))
        {
            // Aritmetico: `bits` ya viene con el signo extendido a 64, asi que el `>>` con signo de
            // C++ sobre int64_t hace lo correcto para cualquier ancho.
            value.bits = normalize(
                static_cast<std::uint64_t>(left->asSigned() >> shift), value.typeKey);
        }
        else
        {
            // Logico: hay que truncar al ancho real ANTES de desplazar, porque `bits` de un tipo
            // sin signo estrecho no tiene basura arriba pero uno de 64 bits si puede tener el bit
            // 63 puesto y ese es justo el caso que C-002 evaluaba con signo.
            const std::uint64_t mask = width == 64
                ? ~std::uint64_t{0}
                : ((std::uint64_t{1} << width) - std::uint64_t{1});
            value.bits = normalize((left->bits & mask) >> shift, value.typeKey);
        }
        return value;
    }

    const std::optional<std::string> common = ProjectTypeConversion::commonNumericType(
        left->typeKey,
        right->typeKey,
        ProjectIntegerConstant{left->bits, !isSignedInteger(left->typeKey)},
        ProjectIntegerConstant{right->bits, !isSignedInteger(right->typeKey)});
    if (!common.has_value() || !isIntegerType(*common)) return std::nullopt;

    const std::uint64_t a = normalize(left->bits, *common);
    const std::uint64_t b = normalize(right->bits, *common);
    const bool resultIsSigned = isSignedInteger(*common);

    ProjectIntegerValue value;
    value.typeKey = *common;

    switch (operation)
    {
        // Aritmetica sobre el gemelo sin signo: el desbordamiento ENVUELVE en vez de ser
        // comportamiento indefinido, y `normalize` recorta al ancho real del tipo comun.
        case TokenKind::Plus: value.bits = normalize(a + b, *common); break;
        case TokenKind::Minus: value.bits = normalize(a - b, *common); break;
        case TokenKind::Star: value.bits = normalize(a * b, *common); break;
        case TokenKind::Amp: value.bits = normalize(a & b, *common); break;
        case TokenKind::Pipe: value.bits = normalize(a | b, *common); break;
        case TokenKind::Caret: value.bits = normalize(a ^ b, *common); break;

        // CC1-103 / C-002 / C-003. Division y modulo son las dos operaciones donde el SIGNO del
        // tipo cambia el resultado, no solo su interpretacion. El evaluador anterior las hacia
        // siempre con signo sobre int64_t, asi que un `ulong` con el bit 63 puesto se dividia como
        // si fuera negativo.
        //
        // Y las dos son las unicas donde C++ tiene comportamiento indefinido alcanzable desde el
        // texto de entrada: division por cero, y minimo-con-signo entre -1. Un compilador no puede
        // permitir que su input lo lleve ahi, asi que las dos salen por nullopt y el llamador emite
        // un diagnostico.
        case TokenKind::Slash:
        case TokenKind::Percent:
        {
            if (b == 0) return std::nullopt;
            if (resultIsSigned)
            {
                const std::int64_t signedA = static_cast<std::int64_t>(a);
                const std::int64_t signedB = static_cast<std::int64_t>(b);
                const unsigned width = integerWidth(*common);
                const std::int64_t minimum = width == 64
                    ? std::numeric_limits<std::int64_t>::min()
                    : -(std::int64_t{1} << (width - 1U));
                if (signedA == minimum && signedB == -1) return std::nullopt;
                value.bits = normalize(
                    static_cast<std::uint64_t>(
                        operation == TokenKind::Slash ? signedA / signedB : signedA % signedB),
                    *common);
            }
            else
            {
                value.bits = normalize(
                    operation == TokenKind::Slash ? (a / b) : (a % b), *common);
            }
            break;
        }

        default:
            return std::nullopt;
    }
    return value;
}

std::optional<ProjectIntegerValue> ProjectIntegerEvaluator::evaluateMember(
    const ast::AstNode* expression,
    std::size_t depth,
    std::optional<std::size_t> declaringTypeOrdinal) const
{
    // Dos formas llegan aqui y las dos importan: `Color.Red`, y `WM_DESTROY` -- que es lo que es
    // todo identificador de mensaje de Win32.
    if (resolver_ == nullptr) return std::nullopt;
    const std::optional<std::size_t> ordinal =
        resolver_->resolveConstantMember(expression, declaringTypeOrdinal);
    if (!ordinal.has_value()) return std::nullopt;

    bool isEnumConstant = false;
    std::int64_t enumConstantValue = 0;
    const ast::AstNode* initializer = nullptr;
    std::string declaredTypeKey;
    std::optional<std::size_t> memberDeclaringType;
    if (!resolver_->describeConstantMember(
            *ordinal,
            isEnumConstant,
            enumConstantValue,
            initializer,
            declaredTypeKey,
            memberDeclaringType))
    {
        return std::nullopt;
    }

    if (isEnumConstant)
    {
        // Un miembro de enum baja a i32 con signo, igual que en el backend.
        ProjectIntegerValue value;
        value.typeKey = "builtin:int";
        value.bits = normalize(static_cast<std::uint64_t>(enumConstantValue), value.typeKey);
        return value;
    }

    const std::optional<ProjectIntegerValue> expanded =
        evaluateNode(initializer, depth + 1, memberDeclaringType);
    if (!expanded.has_value()) return std::nullopt;
    if (!isIntegerType(declaredTypeKey)) return expanded;

    // El valor toma el tipo DECLARADO del campo, no el de su inicializador: `const byte B = 300;`
    // no existe (el modelo semantico ya lo rechazo), pero `const byte B = 200;` vale 200 como byte
    // y es en ese ancho en el que hay que compararlo contra otras etiquetas.
    ProjectIntegerValue value;
    value.typeKey = declaredTypeKey;
    value.bits = normalize(expanded->bits, declaredTypeKey);
    return value;
}
}
