#pragma once

#include <cstdint>
#include <string_view>

namespace nativesharp::frontend::builtins
{
// LA UNICA REGLA de que tipo tiene un literal entero escrito en el codigo fuente.
//
// POR QUE EXISTE ESTA CABECERA, y es la correccion del 2026-09-03. La regla estaba escrita DOS
// VECES -- `integerLiteralType` en src/Frontend/ProjectMethodBodyModel.cpp y
// `integerLiteralTypeOf` en src/Frontend/ProjectIntegerEvaluator.cpp -- y la segunda llevaba este
// comentario:
//
//     "Duplicarlo aqui seria justo el error que este archivo existe para eliminar, asi que la
//      regla se mantiene deliberadamente identica y el test la fija contra casos concretos."
//
// Reconocia el problema y luego lo cometia. "Se mantiene identica" no es un guardian: es una
// promesa. Es la misma clase de fallo que las cinco copias de la tabla de imports (docs 102 y
// 103), las dos tablas de enteros (doc 112) y las tres cadenas del HIR (doc 106). Ahora hay una
// funcion y ningun sitio mas donde escribirla distinta.
//
// EL CASO QUE MOTIVO EL CAMBIO: `-2147483648` NO SE PODIA ESCRIBIR COMO `int`.
//
//     cannot initialize local 'x' of type 'builtin:int' from 'builtin:long'
//
// El menos unario se aplicaba a `2147483648`, que no cabe en un `int` positivo y por tanto era
// `long`, asi que la expresion entera era `long`. Se rodeaba escribiendo `-2147483647 - 1`, y ese
// rodeo estaba en la fixture c30, en NumericLimits.ns y en el evaluador entero.
//
// LA REGLA, que es la de C#: el menos unario aplicado DIRECTAMENTE a un literal entero se tipa
// JUNTO CON EL, asi que la magnitud admitida sube en uno -- exactamente lo que hace falta para el
// minimo de cada ancho, que es uno mas que su maximo. `-(2147483648)` con parentesis NO es el caso
// especial y sigue siendo `long`; es asi tambien en C#.
//
// Lo que NO cambia: un literal SIN menos delante se tipa como siempre. `2147483648` a secas sigue
// siendo `long`.
struct IntegerLiteralSuffix final
{
    bool isUnsigned;
    bool isLong;
};

constexpr IntegerLiteralSuffix integerLiteralSuffix(std::string_view text) noexcept
{
    IntegerLiteralSuffix suffix{false, false};
    for (const char character : text)
    {
        if (character == 'u' || character == 'U') suffix.isUnsigned = true;
        if (character == 'l' || character == 'L') suffix.isLong = true;
    }
    return suffix;
}

// `negated` es true SOLO cuando este literal es el operando directo de un menos unario.
constexpr std::string_view integerLiteralTypeKey(
    std::string_view text, std::uint64_t value, bool negated) noexcept
{
    const IntegerLiteralSuffix suffix = integerLiteralSuffix(text);
    if (suffix.isUnsigned && suffix.isLong) return "builtin:ulong";
    if (suffix.isUnsigned)
    {
        return value <= 0xFFFFFFFFULL ? "builtin:uint" : "builtin:ulong";
    }
    if (suffix.isLong) return "builtin:long";

    // Con menos delante la cota sube en uno: 2147483648 es la MAGNITUD de int.MinValue.
    const std::uint64_t intBound = negated ? 2147483648ULL : 2147483647ULL;
    return value <= intBound ? "builtin:int" : "builtin:long";
}

static_assert(integerLiteralTypeKey("7", 7, false) == "builtin:int", "un literal sin sufijo es int");
static_assert(integerLiteralTypeKey("7u", 7, false) == "builtin:uint", "el sufijo u da uint");
static_assert(integerLiteralTypeKey("7L", 7, false) == "builtin:long", "el sufijo L da long");
static_assert(integerLiteralTypeKey("7ul", 7, false) == "builtin:ulong", "u+L da ulong");
static_assert(integerLiteralTypeKey("2147483647", 2147483647ULL, false) == "builtin:int",
    "int.MaxValue cabe en int sin menos delante");
static_assert(integerLiteralTypeKey("2147483648", 2147483648ULL, false) == "builtin:long",
    "2147483648 A SECAS sigue siendo long -- esto es lo que NO cambia");
static_assert(integerLiteralTypeKey("2147483648", 2147483648ULL, true) == "builtin:int",
    "con menos unario delante, 2147483648 es la magnitud de int.MinValue y el par es int");
static_assert(integerLiteralTypeKey("9223372036854775808", 9223372036854775808ULL, true) ==
        "builtin:long",
    "y lo mismo en 64 bits: -9223372036854775808 es long.MinValue");
}
