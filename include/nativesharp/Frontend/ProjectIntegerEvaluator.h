#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Lexer/TokenKind.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
// CC1-102 "un unico evaluador integral tipado".
//
// POR QUE EXISTE. Antes de esto el compilador tenia DOS evaluadores de constantes enteras: el de
// Stage 37, completo y tipado, encerrado dentro de `isCompileTimeConstantExpression` que solo
// devuelve `bool`; y el que `switch` tuvo que escribir para plegar sus etiquetas, que operaba todo
// sobre `std::int64_t` sin consultar el tipo semantico. El segundo produjo cuatro defectos P1 de
// una sola vez (C-001..C-004), y no por descuido puntual: un evaluador que no conoce el tipo del
// valor que evalua NO PUEDE dar la respuesta correcta para division sin signo, para shift
// aritmetico vs logico, ni para un cast que estrecha.
//
// EL INVARIANTE. Un valor entero constante es SIEMPRE el par (bits, tipo). Nunca `int64_t` suelto.
// Toda operacion normaliza su resultado al tipo del resultado antes de devolverlo, de modo que los
// bits que salen de aqui son exactamente los que el backend materializara. Eso es lo que permite
// comparar dos etiquetas de `switch` con la certeza de que LLVM no las va a volver a colisionar.
struct ProjectIntegerValue final
{
    // Patron de bits YA normalizado a `typeKey`: truncado a su ancho, y con el signo extendido a 64
    // bits si el tipo tiene signo. Guardarlo asi -- y no como `int64_t` -- es lo que hace que la
    // comparacion de igualdad entre dos constantes sea la misma comparacion que hara el hardware.
    std::uint64_t bits = 0;
    std::string typeKey;

    std::int64_t asSigned() const noexcept
    {
        return static_cast<std::int64_t>(bits);
    }
};

class ProjectMemberModel;

// El evaluador necesita resolver `Color.Red` y `WM_DESTROY` a sus valores, y para eso necesita
// alcanzar los miembros. Se le pasa por esta interfaz en vez de acoplarlo a ProjectMethodBodyModel,
// que es de donde vino el codigo, para que el mismo evaluador sirva a inicializadores de campo, a
// etiquetas de `switch` y a lo que venga despues sin arrastrar el modelo entero.
class ProjectConstantMemberResolver
{
public:
    virtual ~ProjectConstantMemberResolver() = default;

    // El ordinal del miembro al que se liga esta sintaxis, si se ligo a alguno.
    // `declaringTypeOrdinal` esta puesto solo mientras se expande el inicializador de un campo
    // `const`, y nombra el tipo al que ese campo pertenece: dentro de ese inicializador puede no
    // haber binding que leer todavia, porque `ProjectMethodBodyModel::build()` analiza los cuerpos
    // de metodo ANTES que los inicializadores de campo.
    virtual std::optional<std::size_t> resolveConstantMember(
        const ast::AstNode* syntax,
        std::optional<std::size_t> declaringTypeOrdinal) const = 0;

    // Para un ordinal de miembro: su valor si es constante de enum, o el AST de su inicializador si
    // es un campo `const`, junto con el tipo que lo declara para seguir expandiendo.
    virtual bool describeConstantMember(
        std::size_t memberOrdinal,
        bool& isEnumConstant,
        std::int64_t& enumConstantValue,
        const ast::AstNode*& initializer,
        std::string& declaredTypeKey,
        std::optional<std::size_t>& declaringTypeOrdinal) const = 0;

    // La clave de tipo canonica de una sintaxis de tipo -- la que necesita un cast para saber a que
    // ancho normalizar. La resuelve el llamador, que ya tiene el modelo de miembros y el name
    // lookup; reimplementarla aqui a partir del nombre base seria un tercer resolvedor de tipos, y
    // este archivo existe para eliminar duplicacion, no para agregarla.
    virtual std::string resolveTypeKey(const ast::TypeSyntax* typeSyntax) const = 0;
};

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

    explicit ProjectIntegerEvaluator(const ProjectConstantMemberResolver* resolver) noexcept
        : resolver_(resolver)
    {
    }

    // Evalua una expresion constante entera. Devuelve nullopt cuando no es constante, cuando la
    // operacion no esta definida (division por cero, minimo-con-signo / -1, shift fuera de rango)
    // o cuando el evaluador no cubre esa forma. NUNCA ejecuta una operacion de C++ con
    // comportamiento indefinido: un texto de entrada no puede llevar al compilador ahi.
    std::optional<ProjectIntegerValue> evaluate(
        const ast::AstNode* expression,
        std::optional<std::size_t> declaringTypeOrdinal = std::nullopt) const;

private:
    std::optional<ProjectIntegerValue> evaluateNode(
        const ast::AstNode* expression,
        std::size_t depth,
        std::optional<std::size_t> declaringTypeOrdinal) const;

    std::optional<ProjectIntegerValue> evaluateUnary(
        const ast::AstNode* expression,
        std::size_t depth,
        std::optional<std::size_t> declaringTypeOrdinal) const;

    std::optional<ProjectIntegerValue> evaluateBinary(
        const ast::AstNode* expression,
        std::size_t depth,
        std::optional<std::size_t> declaringTypeOrdinal) const;

    std::optional<ProjectIntegerValue> evaluateMember(
        const ast::AstNode* expression,
        std::size_t depth,
        std::optional<std::size_t> declaringTypeOrdinal) const;

    const ProjectConstantMemberResolver* resolver_ = nullptr;
};
}
