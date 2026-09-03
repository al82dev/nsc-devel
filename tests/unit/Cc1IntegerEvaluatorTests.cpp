// CC1-108 -- matriz de regresion del evaluador entero tipado.
//
// POR QUE ESTA SUITE EXISTE, dicho sin rodeos: el compilador tenia DOS evaluadores de constantes
// enteras, y el segundo -- escrito para plegar las etiquetas de `switch` -- produjo cuatro defectos
// P1 de una sola vez. Ninguno era un descuido puntual. Los cuatro salen de la MISMA causa: operar
// sobre `std::int64_t` sin consultar el tipo semantico del valor.
//
//   C-001  un cast no normalizaba: `(byte)300` seguia valiendo 300 para el frontend, y 44 para el
//          backend. Con un `switch`, eso es un valor de caso duplicado en LLVM, que es
//          comportamiento indefinido -- justo lo que el chequeo de duplicados existe para impedir.
//   C-002  `/`, `%` y `>>` se hacian con signo aunque el tipo fuera `ulong`.
//   C-003  minimo-con-signo entre -1 no estaba protegido: comportamiento indefinido DENTRO del
//          compilador, disparado por texto de entrada. Un compilador no puede permitir eso.
//   C-004  el contador de shift se validaba contra los 64 bits de la representacion interna, no
//          contra el ancho real del tipo del operando izquierdo.
//
// Cada bloque de abajo fija uno de esos cuatro, mas el ancho de `char` (C-005), que es el camino
// por el que C-001 era alcanzable con tres lineas de NativeSharp.

#include "nativesharp/AST/AstArena.h"
#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"

#include "../support/TestContext.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace
{
using nativesharp::SourceRange;
using nativesharp::ast::AstArena;
using nativesharp::ast::AstNode;
using nativesharp::ast::AstNodeKind;
using nativesharp::frontend::ProjectConstantMemberResolver;
using nativesharp::frontend::ProjectIntegerEvaluator;
using nativesharp::frontend::ProjectIntegerValue;
using nativesharp::lexer::TokenKind;
using nativesharp::tests::TestContext;

// Resolvedor vacio: esta suite mide la aritmetica y la normalizacion, no la resolucion de nombres.
// Los miembros `const` y de enum se cubren de punta a punta en las fixtures de nsc.cc1.correctness,
// donde hay un modelo de miembros real detras.
class NullResolver final : public ProjectConstantMemberResolver
{
public:
    std::optional<std::size_t> resolveConstantMember(
        const AstNode*, std::optional<std::size_t>) const override
    {
        return std::nullopt;
    }

    bool describeConstantMember(
        std::size_t,
        bool&,
        std::int64_t&,
        const AstNode*&,
        std::string&,
        std::optional<std::size_t>&) const override
    {
        return false;
    }

    std::string resolveTypeKey(const nativesharp::ast::TypeSyntax*) const override
    {
        return {};
    }
};

AstNode* literal(AstArena& arena, std::uint64_t value, std::string suffix = {})
{
    AstNode* node = arena.make<AstNode>(AstNodeKind::IntegerLiteralExpression, SourceRange());
    node->setIntegerValue(value);
    node->setText(std::to_string(value) + suffix);
    return node;
}

AstNode* binary(AstArena& arena, TokenKind operation, AstNode* left, AstNode* right)
{
    AstNode* node = arena.make<AstNode>(AstNodeKind::BinaryExpression, SourceRange());
    node->setTokenKind(operation);
    node->setFirst(left);
    node->setSecond(right);
    return node;
}

AstNode* unary(AstArena& arena, TokenKind operation, AstNode* operand)
{
    AstNode* node = arena.make<AstNode>(AstNodeKind::UnaryExpression, SourceRange());
    node->setTokenKind(operation);
    node->setFirst(operand);
    return node;
}
}

int main()
{
    TestContext tests;
    AstArena arena;
    const NullResolver resolver;
    const ProjectIntegerEvaluator evaluator(&resolver);

    // -------------------------------------------------------------------------------------
    // Anchos y signo. `char` mide OCHO bits -- C-005. Si esta tabla y la de `constantFits`
    // discrepan, un `switch` sobre `char` vuelve a poder colisionar dos etiquetas.
    // -------------------------------------------------------------------------------------
    tests.expect(ProjectIntegerEvaluator::integerWidth("builtin:byte") == 8, "byte is 8 bits");
    tests.expect(ProjectIntegerEvaluator::integerWidth("builtin:char") == 8, "char is 8 bits");
    tests.expect(ProjectIntegerEvaluator::integerWidth("builtin:short") == 16, "short is 16 bits");
    tests.expect(ProjectIntegerEvaluator::integerWidth("builtin:ushort") == 16, "ushort is 16 bits");
    tests.expect(ProjectIntegerEvaluator::integerWidth("builtin:int") == 32, "int is 32 bits");
    tests.expect(ProjectIntegerEvaluator::integerWidth("builtin:uint") == 32, "uint is 32 bits");
    tests.expect(ProjectIntegerEvaluator::integerWidth("builtin:long") == 64, "long is 64 bits");
    tests.expect(ProjectIntegerEvaluator::integerWidth("builtin:ulong") == 64, "ulong is 64 bits");
    tests.expect(ProjectIntegerEvaluator::integerWidth("builtin:double") == 0, "double is not integral");
    tests.expect(
        ProjectIntegerEvaluator::isSignedInteger("builtin:int") &&
            !ProjectIntegerEvaluator::isSignedInteger("builtin:uint") &&
            !ProjectIntegerEvaluator::isSignedInteger("builtin:char"),
        "signedness table matches the language");

    // -------------------------------------------------------------------------------------
    // normalize -- la operacion central. C-001.
    // -------------------------------------------------------------------------------------
    tests.expect(
        ProjectIntegerEvaluator::normalize(321, "builtin:char") == 65,
        "321 normalized to char is 65 -- the exact collision that reached LLVM");
    tests.expect(
        ProjectIntegerEvaluator::normalize(300, "builtin:byte") == 44,
        "300 normalized to byte is 44");
    tests.expect(
        ProjectIntegerEvaluator::normalize(255, "builtin:byte") == 255,
        "255 fits in byte unchanged");
    tests.expect(
        static_cast<std::int64_t>(ProjectIntegerEvaluator::normalize(255, "builtin:short")) == 255,
        "255 in short stays positive");
    tests.expect(
        static_cast<std::int64_t>(ProjectIntegerEvaluator::normalize(0xFFFF, "builtin:short")) == -1,
        "0xFFFF normalized to short sign-extends to -1");
    tests.expect(
        ProjectIntegerEvaluator::normalize(0xFFFF, "builtin:ushort") == 65535,
        "0xFFFF normalized to ushort stays 65535");
    tests.expect(
        static_cast<std::int64_t>(
            ProjectIntegerEvaluator::normalize(0x80000000ULL, "builtin:int")) ==
            std::numeric_limits<std::int32_t>::min(),
        "0x80000000 normalized to int is int.MinValue");

    // -------------------------------------------------------------------------------------
    // Overflow envolvente por ancho. Sin comportamiento indefinido en ningun ancho.
    // -------------------------------------------------------------------------------------
    {
        // (byte)200 + (byte)100 -> el tipo comun de dos literales `int` es `int`, asi que esto NO
        // envuelve: el punto es que el ancho del RESULTADO manda, no el de los operandos.
        const auto sum = evaluator.evaluate(binary(arena, TokenKind::Plus, literal(arena, 200), literal(arena, 100)));
        tests.expect(
            sum.has_value() && sum->typeKey == "builtin:int" && sum->asSigned() == 300,
            "200 + 100 is 300 as int -- operand width does not narrow the result");
    }
    {
        const auto product = evaluator.evaluate(
            binary(arena, TokenKind::Star, literal(arena, 65536), literal(arena, 65536)));
        tests.expect(
            product.has_value() && product->typeKey == "builtin:int" && product->asSigned() == 0,
            "65536 * 65536 wraps to 0 in int instead of being undefined");
    }

    // -------------------------------------------------------------------------------------
    // Division y modulo SIN SIGNO con el bit 63 puesto. C-002.
    // -------------------------------------------------------------------------------------
    {
        // 0x8000000000000000ul / 2ul. Con signo daria un valor negativo; sin signo es 2^62.
        const auto quotient = evaluator.evaluate(binary(
            arena,
            TokenKind::Slash,
            literal(arena, 0x8000000000000000ULL, "ul"),
            literal(arena, 2, "ul")));
        tests.expect(
            quotient.has_value() && quotient->typeKey == "builtin:ulong" &&
                quotient->bits == 0x4000000000000000ULL,
            "unsigned division with bit 63 set is unsigned");
    }
    {
        const auto remainder = evaluator.evaluate(binary(
            arena,
            TokenKind::Percent,
            literal(arena, 0x8000000000000001ULL, "ul"),
            literal(arena, 2, "ul")));
        tests.expect(
            remainder.has_value() && remainder->bits == 1,
            "unsigned modulo with bit 63 set is unsigned");
    }
    {
        const auto quotient = evaluator.evaluate(
            binary(arena, TokenKind::Slash, unary(arena, TokenKind::Minus, literal(arena, 7)), literal(arena, 2)));
        tests.expect(
            quotient.has_value() && quotient->asSigned() == -3,
            "signed division truncates toward zero");
    }

    // -------------------------------------------------------------------------------------
    // Division por cero y minimo-con-signo / -1. C-003. Las dos salen por nullopt: el texto de
    // entrada NO puede llevar al compilador a una operacion indefinida de C++. El minimo entre -1
    // YA NO sale por nullopt: envuelve. Ver el bloque de abajo y el doc 110 2.5.
    // -------------------------------------------------------------------------------------
    tests.expect(
        !evaluator.evaluate(binary(arena, TokenKind::Slash, literal(arena, 1), literal(arena, 0)))
             .has_value(),
        "division by zero is not a constant");
    tests.expect(
        !evaluator.evaluate(binary(arena, TokenKind::Percent, literal(arena, 1), literal(arena, 0)))
             .has_value(),
        "modulo by zero is not a constant");
    {
        // int.MinValue / -1. Se escribe como (-2147483647 - 1) / -1 porque la magnitud
        // 2147483648 sola no cabe en un `int` positivo -- el mismo truco que usa NumericLimits.ns.
        //
        // ESTE PAR DE COMPROBACIONES CAMBIO DE SENTIDO EL 2026-09-03, y el motivo importa porque
        // el proyecto no toca una prueba para que pase. Antes exigian nullopt, o sea que el
        // programa NO COMPILABA. Eso era la TERCERA respuesta distinta a la misma pregunta: el
        // plegado rechazaba, la ejecucion en -O0 abortaba por hardware (0xC0000095) y la
        // ejecucion en -O2 devolvia otro valor. La fixture c30 lo midio.
        //
        // El doc 110 §2.1 decide que el desbordamiento con signo esta DEFINIDO y envuelve, y §2.5
        // aplica esa regla aqui: `MinValue / -1` es `MinValue` y `MinValue % -1` es 0, en el
        // plegado y en la ejecucion. Lo que se arreglo es el COMPILADOR, en tres sitios; esta
        // prueba solo deja de exigir lo contrario de lo que el contrato dice.
        //
        // Lo que NO cambia: el UB de C++ dentro del compilador se sigue evitando -- el evaluador
        // calcula este caso a mano en vez de dejar que `signedA / signedB` lo ejecute.
        AstNode* minimum = binary(
            arena,
            TokenKind::Minus,
            unary(arena, TokenKind::Minus, literal(arena, 2147483647)),
            literal(arena, 1));
        AstNode* negativeOne = unary(arena, TokenKind::Minus, literal(arena, 1));
        const auto quotient = evaluator.evaluate(binary(arena, TokenKind::Slash, minimum, negativeOne));
        // Se compara con asSigned() y no con `bits`: `bits` guarda el valor YA normalizado y
        // extendido a 64 bits, asi que int.MinValue son 0xFFFFFFFF80000000 y no 0x80000000.
        tests.expect(
            quotient.has_value() && quotient->typeKey == "builtin:int" &&
                quotient->asSigned() == -2147483647LL - 1,
            "int.MinValue / -1 wraps to int.MinValue -- doc 110 2.5");
        const auto remainder = evaluator.evaluate(binary(arena, TokenKind::Percent, minimum, negativeOne));
        tests.expect(
            remainder.has_value() && remainder->typeKey == "builtin:int" && remainder->bits == 0ULL,
            "int.MinValue % -1 is 0");
    }
    {
        // El mismo caso en 64 bits. Va aparte porque el ancho lo decide `integerWidth` y un fallo
        // ahi solo se ve en el tipo que NO se probo.
        AstNode* minimumLong = binary(
            arena,
            TokenKind::Minus,
            unary(arena, TokenKind::Minus, literal(arena, 9223372036854775807ULL, "L")),
            literal(arena, 1));
        AstNode* negativeOne = unary(arena, TokenKind::Minus, literal(arena, 1));
        const auto quotient =
            evaluator.evaluate(binary(arena, TokenKind::Slash, minimumLong, negativeOne));
        tests.expect(
            quotient.has_value() && quotient->typeKey == "builtin:long" &&
                quotient->bits == (std::uint64_t{1} << 63),
            "long.MinValue / -1 wraps to long.MinValue");
        const auto remainder =
            evaluator.evaluate(binary(arena, TokenKind::Percent, minimumLong, negativeOne));
        tests.expect(
            remainder.has_value() && remainder->bits == 0ULL, "long.MinValue % -1 is 0");
    }

    // -------------------------------------------------------------------------------------
    // Shift: logico vs aritmetico, y ancho real del operando izquierdo. C-002 y C-004.
    // -------------------------------------------------------------------------------------
    {
        // (-8) >> 1 con signo es -4. Un shift logico daria un numero enorme.
        const auto shifted = evaluator.evaluate(binary(
            arena, TokenKind::GreaterGreater, unary(arena, TokenKind::Minus, literal(arena, 8)), literal(arena, 1)));
        tests.expect(
            shifted.has_value() && shifted->asSigned() == -4,
            "signed right shift is arithmetic");
    }
    {
        // 0x8000000000000000ul >> 1 sin signo es 2^62. Con signo aritmetico daria -2^62.
        const auto shifted = evaluator.evaluate(binary(
            arena,
            TokenKind::GreaterGreater,
            literal(arena, 0x8000000000000000ULL, "ul"),
            literal(arena, 1)));
        tests.expect(
            shifted.has_value() && shifted->bits == 0x4000000000000000ULL,
            "unsigned right shift is logical");
    }
    {
        const auto shifted = evaluator.evaluate(
            binary(arena, TokenKind::LessLess, literal(arena, 1, "u"), literal(arena, 28)));
        tests.expect(
            shifted.has_value() && shifted->typeKey == "builtin:uint" && shifted->bits == (1ULL << 28),
            "1u << 28 stays uint -- the shift result takes the LEFT operand's type");
    }
    tests.expect(
        !evaluator.evaluate(binary(arena, TokenKind::LessLess, literal(arena, 1), literal(arena, 32)))
             .has_value(),
        "int << 32 is refused -- the count is validated against the operand width, not 64");
    tests.expect(
        evaluator.evaluate(binary(arena, TokenKind::LessLess, literal(arena, 1), literal(arena, 31)))
            .has_value(),
        "int << 31 is still accepted");
    tests.expect(
        !evaluator
             .evaluate(binary(
                 arena, TokenKind::LessLess, literal(arena, 1), unary(arena, TokenKind::Minus, literal(arena, 1))))
             .has_value(),
        "a negative shift count is refused");

    // -------------------------------------------------------------------------------------
    // Literales: tipo por sufijo, igual que el modelo semantico.
    // -------------------------------------------------------------------------------------
    {
        const auto plain = evaluator.evaluate(literal(arena, 7));
        const auto unsignedLiteral = evaluator.evaluate(literal(arena, 7, "u"));
        const auto longLiteral = evaluator.evaluate(literal(arena, 7, "L"));
        tests.expect(
            plain.has_value() && plain->typeKey == "builtin:int", "an unsuffixed literal is int");
        tests.expect(
            unsignedLiteral.has_value() && unsignedLiteral->typeKey == "builtin:uint",
            "a u-suffixed literal is uint");
        tests.expect(
            longLiteral.has_value() && longLiteral->typeKey == "builtin:long",
            "an L-suffixed literal is long");
    }

    // -------------------------------------------------------------------------------------
    // Formas no constantes: nullopt, nunca un valor inventado.
    // -------------------------------------------------------------------------------------
    tests.expect(
        !evaluator.evaluate(arena.make<AstNode>(AstNodeKind::CallExpression, SourceRange())).has_value(),
        "a call is not a constant");
    tests.expect(!evaluator.evaluate(nullptr).has_value(), "a null expression is not a constant");

    return tests.result();
}
