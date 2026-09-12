#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"

#include "nativesharp/Frontend/BuiltinIntegerTypes.h"
#include "nativesharp/Frontend/IntegerLiteralTyping.h"

#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Frontend/ProjectTypeConversion.h"

#include <algorithm>
#include <array>
#include <limits>

namespace nativesharp::frontend
{
namespace
{
using lexer::TokenKind;

// Expression nesting remains bounded independently from the constant dependency graph. Member
// cycles are detected by ordinal below, so a valid chain of more than 32 constants is no longer
// rejected merely because it is long.
constexpr std::size_t kMaximumExpressionDepth = 1024;

// LA TABLA VIVE EN include/nativesharp/Frontend/BuiltinIntegerTypes.h, y desde el 2026-09-03 hay
// UNA SOLA. Estaba escrita aqui y otra vez en ProjectTypeConversion.cpp, con un comentario que
// decia que la duplicacion era deliberada porque "el test nsc.cc1.integer-evaluator lo fija". La
// direccion de dependencia era un motivo real --este evaluador llama al clasificador de
// conversiones, no al reves-- pero la prueba NO fijaba nada: comprueba las respuestas del
// evaluador y no toca la otra tabla en ninguna linea. Una cabecera de datos `constexpr` no tiene
// direccion de dependencia, asi que resuelve el motivo real y elimina la copia. El detalle
// completo, en el comentario de esa cabecera.
using builtins::IntegerTypeInfo;
using builtins::findIntegerType;

// El tipo de un literal entero. LA REGLA YA NO ESTA AQUI: vive en
// include/nativesharp/Frontend/IntegerLiteralTyping.h y la comparte con el modelo semantico.
//
// Aqui habia una copia, con un comentario que decia "duplicarlo aqui seria justo el error que este
// archivo existe para eliminar, asi que la regla se mantiene deliberadamente identica". Reconocia
// el problema y lo cometia: "se mantiene identica" es una promesa, no un guardian.
std::string integerLiteralTypeOf(const ast::AstNode* node, bool negated = false)
{
    return std::string(builtins::integerLiteralTypeKey(node->text(), node->integerValue(), negated));
}

// ID 122 -- LAS DOS CONVENCIONES, unificadas aqui y no en la struct.
//
// `ProjectIntegerConstant::value` es una MAGNITUD y el signo va aparte en `isNegative`. Lo dice la
// propia struct, y `constantFits` depende de ello: solo compara contra COTAS SUPERIORES, asi que un
// patron de complemento a dos como 0xFFFF...FD no pasaria ninguna.
//
// `ProjectIntegerValue::bits` es lo contrario: un patron de complemento a dos de 64 bits con el
// signo YA extendido por `normalize`. Este archivo pasaba `bits` directamente al campo documentado
// como magnitud y no ponia `isNegative` nunca. Un operando negativo llegaba a `constantFits` como
// una magnitud enorme y no cabia en ningun destino. No mentia -- se rendia -- pero era la
// duplicacion semantica que impedia escribir la regla de promocion entera (ID 121) encima.
//
// LA MAGNITUD SE CALCULA SOBRE EL GEMELO SIN SIGNO (`0 - bits`): negar el minimo con signo en C++
// es comportamiento indefinido, y un compilador no puede permitir que su texto de entrada lo lleve
// ahi. Es la misma tecnica que ya usa el unario `-` de este mismo archivo.
//
// `isNegative` de un cero es falso, igual que en el sitio del modelo semantico que construye la
// negacion (`negated.isNegative = negated.value != 0`). Las dos convenciones coinciden ahora.
ProjectIntegerConstant asIntegerConstant(const ProjectIntegerValue& value) noexcept
{
    const bool typeIsUnsigned = !ProjectIntegerEvaluator::isSignedInteger(value.typeKey);
    if (typeIsUnsigned || value.asSigned() >= 0)
    {
        return ProjectIntegerConstant{value.bits, typeIsUnsigned, false};
    }
    return ProjectIntegerConstant{std::uint64_t{0} - value.bits, false, true};
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
    std::vector<std::size_t> activeMembers;
    return evaluateNode(expression, 0, declaringTypeOrdinal, activeMembers);
}

std::optional<ProjectIntegerValue> ProjectIntegerEvaluator::evaluateNode(
    const ast::AstNode* expression,
    std::size_t depth,
    std::optional<std::size_t> declaringTypeOrdinal,
    std::vector<std::size_t>& activeMembers) const
{
    if (expression == nullptr || depth > kMaximumExpressionDepth) return std::nullopt;

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
                evaluateNode(expression->first(), depth + 1, declaringTypeOrdinal, activeMembers);
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
            return evaluateUnary(expression, depth, declaringTypeOrdinal, activeMembers);

        case ast::AstNodeKind::BinaryExpression:
            return evaluateBinary(expression, depth, declaringTypeOrdinal, activeMembers);

        case ast::AstNodeKind::IdentifierExpression:
        case ast::AstNodeKind::MemberExpression:
            return evaluateMember(expression, depth, declaringTypeOrdinal, activeMembers);

        default:
            return std::nullopt;
    }
}

std::optional<ProjectIntegerValue> ProjectIntegerEvaluator::evaluateUnary(
    const ast::AstNode* expression,
    std::size_t depth,
    std::optional<std::size_t> declaringTypeOrdinal,
    std::vector<std::size_t>& activeMembers) const
{
    const std::optional<ProjectIntegerValue> operand =
        evaluateNode(expression->first(), depth + 1, declaringTypeOrdinal, activeMembers);
    if (!operand.has_value()) return std::nullopt;

    ProjectIntegerValue value;
    value.typeKey = operand->typeKey;

    // EL MENOS UNARIO SOBRE UN LITERAL SE TIPA JUNTO CON EL -- la misma regla que aplica
    // ProjectMethodBodyModel::analyzeUnary, y por el mismo motivo: `int.MinValue` tiene magnitud
    // 2147483648, uno mas que el maximo de `int`, asi que tipar el literal por su cuenta lo hace
    // `long` y arrastra la expresion entera.
    //
    // TIENE QUE ESTAR EN LOS DOS SITIOS. Si el plegado dijera `long` y el modelo semantico `int`,
    // `const int x = -2147483648;` compilaria y el valor plegado tendria otro tipo que el
    // declarado -- exactamente la clase de discrepancia que el paso 6 de la fixture c30 existe
    // para cazar. La REGLA es una sola (IntegerLiteralTyping.h); lo que se repite es la pregunta
    // "¿este nodo es un literal debajo de un menos?", que es sintaxis y no semantica.
    if (expression->tokenKind() == TokenKind::Minus && expression->first() != nullptr &&
        expression->first()->kind() == ast::AstNodeKind::IntegerLiteralExpression)
    {
        value.typeKey = integerLiteralTypeOf(expression->first(), true);
    }

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
    std::optional<std::size_t> declaringTypeOrdinal,
    std::vector<std::size_t>& activeMembers) const
{
    const std::optional<ProjectIntegerValue> left =
        evaluateNode(expression->first(), depth + 1, declaringTypeOrdinal, activeMembers);
    if (!left.has_value()) return std::nullopt;
    const std::optional<ProjectIntegerValue> right =
        evaluateNode(expression->second(), depth + 1, declaringTypeOrdinal, activeMembers);
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
        asIntegerConstant(*left),
        asIntegerConstant(*right));
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
        // permitir que su input lo lleve ahi -- pero eso es una restriccion del lenguaje EN QUE
        // ESTA ESCRITO EL COMPILADOR, y no puede decidir lo que significa el lenguaje compilado.
        //
        // POR ESO LOS DOS CASOS SE SEPARAN, y es la correccion del 2026-09-03:
        //
        //   * b == 0 sale por nullopt y el llamador emite un diagnostico. Es correcto: el doc 110
        //     §2.5 dice que dividir por cero es un fallo del programa, y una constante que divide
        //     por cero se puede rechazar en compilacion en vez de esperar a la trampa.
        //
        //   * minimo / -1 DEVUELVE EL MINIMO. Antes tambien salia por nullopt, o sea que el
        //     programa NO COMPILABA -- y eso era la tercera respuesta distinta a la misma
        //     pregunta: el plegado rechazaba, la ejecucion en -O0 abortaba por hardware y la
        //     ejecucion en -O2 devolvia otra cosa. El doc 110 §2.1 dice que el desbordamiento con
        //     signo envuelve, y §2.5 aplica esa regla aqui.
        //
        // El calculo se hace A MANO, sin llegar a la division de C++, precisamente porque en C++
        // ese caso es UB: el resultado envuelto de `minimo / -1` es `minimo`, y `minimo % -1` es 0.
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
                if (signedA == minimum && signedB == -1)
                {
                    value.bits = normalize(
                        operation == TokenKind::Slash ? static_cast<std::uint64_t>(minimum) : 0U,
                        *common);
                    break;
                }
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
    std::size_t,
    std::optional<std::size_t> declaringTypeOrdinal,
    std::vector<std::size_t>& activeMembers) const
{
    // Dos formas llegan aqui y las dos importan: `Color.Red`, y `WM_DESTROY` -- que es lo que es
    // todo identificador de mensaje de Win32.
    if (resolver_ == nullptr) return std::nullopt;
    const std::optional<std::size_t> ordinal =
        resolver_->resolveConstantMember(expression, declaringTypeOrdinal);
    if (!ordinal.has_value()) return std::nullopt;

    bool isEnumConstant = false;
    std::int64_t enumConstantValue = 0;
    std::string enumUnderlyingKey;
    const ast::AstNode* initializer = nullptr;
    std::string declaredTypeKey;
    std::optional<std::size_t> memberDeclaringType;
    if (!resolver_->describeConstantMember(
            *ordinal,
            isEnumConstant,
            enumConstantValue,
            enumUnderlyingKey,
            initializer,
            declaredTypeKey,
            memberDeclaringType))
    {
        return std::nullopt;
    }

    if (isEnumConstant)
    {
        // UN MIEMBRO DE ENUM VALE LO QUE SU TIPO SUBYACENTE, no `builtin:int` siempre.
        //
        // Esta linea era la QUINTA copia de "un enum es un int con signo", y la que mas lejos
        // llegaba sin que se notase: `normalize` trunca al ancho del tipo que se le da, asi que un
        // enumerador de un `enum : ulong` con valor 2^64-1 salia de aqui valiendo -1, y ese -1 era
        // lo que acababa en el IR. La regla esta en EnumUnderlyingType.h y la contesta ahora quien
        // tiene la declaracion delante.
        ProjectIntegerValue value;
        value.typeKey = enumUnderlyingKey.empty() ? std::string("builtin:int") : enumUnderlyingKey;
        value.bits = normalize(static_cast<std::uint64_t>(enumConstantValue), value.typeKey);
        return value;
    }

    if (std::find(activeMembers.begin(), activeMembers.end(), *ordinal) != activeMembers.end())
    {
        return std::nullopt;
    }
    activeMembers.push_back(*ordinal);
    // The initializer starts a new expression tree. Its syntax depth is independent from how
    // many named constants led to it; `activeMembers` is what guards dependency cycles.
    const std::optional<ProjectIntegerValue> expanded =
        evaluateNode(initializer, 0, memberDeclaringType, activeMembers);
    activeMembers.pop_back();
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
