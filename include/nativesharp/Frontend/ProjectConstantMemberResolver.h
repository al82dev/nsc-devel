#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace nativesharp::frontend
{
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
    //
    // `enumUnderlyingTypeKey` sale con el tipo entero SUBYACENTE del enum --`builtin:byte`,
    // `builtin:ulong`...-- cuando el miembro es una constante de enum. Sin el, este evaluador tenia
    // que suponer `builtin:int`, y esa suposicion truncaba a 32 bits el valor de todo enumerador de
    // un enum mas ancho: `Max = 18446744073709551615` en un `enum : ulong` salia valiendo -1.
    virtual bool describeConstantMember(
        std::size_t memberOrdinal,
        bool& isEnumConstant,
        std::int64_t& enumConstantValue,
        std::string& enumUnderlyingTypeKey,
        const ast::AstNode*& initializer,
        std::string& declaredTypeKey,
        std::optional<std::size_t>& declaringTypeOrdinal) const = 0;

    // La clave de tipo canonica de una sintaxis de tipo -- la que necesita un cast para saber a que
    // ancho normalizar. La resuelve el llamador, que ya tiene el modelo de miembros y el name
    // lookup; reimplementarla aqui a partir del nombre base seria un tercer resolvedor de tipos, y
    // este archivo existe para eliminar duplicacion, no para agregarla.
    virtual std::string resolveTypeKey(const ast::TypeSyntax* typeSyntax) const = 0;
};
}
