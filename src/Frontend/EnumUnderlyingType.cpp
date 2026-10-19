#include "nativesharp/Frontend/EnumUnderlyingType.h"

namespace nativesharp::frontend
{
std::string_view declaredEnumUnderlyingTypeKey(const ast::AstNode* enumSyntax) noexcept
{
    if (enumSyntax == nullptr) return {};
    const ast::TypeSyntax* written = enumSyntax->typeSyntax();
    if (written == nullptr) return {};
    // Un tipo subyacente no puede llevar punteros, arreglos, argumentos genericos ni un
    // cualificador de propiedad: `enum E : int*` y `enum E : owned int` no significan nada. Se
    // rechaza mirando la FORMA y no el nombre -- si solo se mirara `baseName()`, `int*` diria
    // "int" y pasaria.
    if (!written->typeArguments().empty()) return {};
    if (!written->modifiers().empty()) return {};
    if (written->ownershipQualifier() != ast::OwnershipQualifierSyntaxKind::None) return {};
    const std::string_view base = written->baseName();
    // `char` NO VALE, aunque este en la tabla general de enteros.
    //
    // En NativeSharp `char` es un byte de TEXTO --ocho bits, sin signo, CC1-100-- y esta en la
    // tabla porque aritmeticamente se comporta como uno. Pero el tipo subyacente de un enum es una
    // decision de ABI, y "el ancho de un caracter" no es una respuesta a esa pregunta: `byte` dice
    // exactamente lo mismo sin ambiguedad. C# tampoco lo admite. Se excluye aqui, en la unica
    // regla, para que el diagnostico que enumera los tipos permitidos siga siendo cierto: si `char`
    // se colara, el mensaje estaria mintiendo sobre su propia lista.
    if (base == "char") return {};
    // `int` en el fuente es `builtin:int` en la tabla.
    for (const builtins::IntegerTypeInfo& info : builtins::kIntegerTypes)
    {
        // info.key es "builtin:int"; base es "int".
        if (info.key.size() > 8 && info.key.substr(8) == base) return info.key;
    }
    return {};
}

std::string_view enumUnderlyingTypeKey(const ast::AstNode* enumSyntax) noexcept
{
    const std::string_view declared = declaredEnumUnderlyingTypeKey(enumSyntax);
    return declared.empty() ? std::string_view("builtin:int") : declared;
}
}
