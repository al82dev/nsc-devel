#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/Frontend/ProjectIntegerValue.h"
#include "nativesharp/Frontend/ProjectConstantMemberResolver.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::frontend
{
class ProjectMemberModel;

class ProjectIntegerEvaluator final
{
public:
    // Ancho en bits de un tipo integral de NativeSharp, o 0 si no es integral.
    // `builtin:char` es 8 -- ver CC1-100 y el comentario de `constantFits`.
    static unsigned integerWidth(std::string_view typeKey) noexcept;
    static bool isSignedInteger(std::string_view typeKey) noexcept;
    static bool isIntegerType(std::string_view typeKey) noexcept;

    // Trunca `bits` al ancho de `typeKey` y, si el tipo tiene signo, extiende el signo a 64 bits.
    // ES LA OPERACION CENTRAL: cualquier valor que salga de este evaluador paso por aqui, y por eso
    // dos valores iguales aqui son iguales tambien despues de que el backend los materialice.
    static std::uint64_t normalize(std::uint64_t bits, std::string_view typeKey) noexcept;

    explicit ProjectIntegerEvaluator(const ProjectConstantMemberResolver* resolver) noexcept;

    // Evalua una expresion constante entera. Devuelve nullopt cuando no es constante, cuando la
    // operacion no esta definida (division por cero, shift fuera de rango)
    // o cuando el evaluador no cubre esa forma. MinValue / -1 wraps by the language contract.
    // NUNCA ejecuta una operacion de C++ con
    // comportamiento indefinido: un texto de entrada no puede llevar al compilador ahi.
    std::optional<ProjectIntegerValue> evaluate(
        const ast::AstNode* expression,
        std::optional<std::size_t> declaringTypeOrdinal = std::nullopt) const;

private:
    std::optional<ProjectIntegerValue> evaluateNode(
        const ast::AstNode* expression,
        std::size_t depth,
        std::optional<std::size_t> declaringTypeOrdinal,
        std::vector<std::size_t>& activeMembers) const;

    std::optional<ProjectIntegerValue> evaluateUnary(
        const ast::AstNode* expression,
        std::size_t depth,
        std::optional<std::size_t> declaringTypeOrdinal,
        std::vector<std::size_t>& activeMembers) const;

    std::optional<ProjectIntegerValue> evaluateBinary(
        const ast::AstNode* expression,
        std::size_t depth,
        std::optional<std::size_t> declaringTypeOrdinal,
        std::vector<std::size_t>& activeMembers) const;

    std::optional<ProjectIntegerValue> evaluateMember(
        const ast::AstNode* expression,
        std::size_t depth,
        std::optional<std::size_t> declaringTypeOrdinal,
        std::vector<std::size_t>& activeMembers) const;

    const ProjectConstantMemberResolver* resolver_ = nullptr;
};
}
