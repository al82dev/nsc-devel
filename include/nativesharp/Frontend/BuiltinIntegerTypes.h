#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace nativesharp::frontend::builtins
{
// LA UNICA TABLA de los tipos enteros incorporados: su clave, su ancho en bits y si tienen signo.
//
// POR QUE EXISTE ESTA CABECERA, y es la correccion del 2026-09-03 (fase M1.1). Esta tabla estaba
// escrita DOS VECES, en `src/Frontend/ProjectIntegerEvaluator.cpp` y en
// `src/Frontend/ProjectTypeConversion.cpp`, y las dos copias llevaban un comentario que decia que
// la duplicacion era deliberada:
//
//     "DICE LO MISMO que ProjectIntegerEvaluator::kIntegerTypes, y esta escrita dos veces A
//      PROPOSITO: es el evaluador de constantes quien llama aqui, no al reves, asi que este
//      archivo tiene que poder contestar sin depender de el. Las dos tablas se fijan la una contra
//      la otra en nsc.cc1.integer-evaluator y en la fixture c17."
//
// LA MITAD DE ESO ERA CIERTA Y LA OTRA NO.
//
// Cierta: la direccion de dependencia. `ProjectIntegerEvaluator` llama a `ProjectTypeConversion`,
// asi que este ultimo no puede incluir al primero. Eso sigue siendo verdad -- y es exactamente el
// problema que una CABECERA de datos `constexpr` resuelve, porque una cabecera no tiene direccion:
// los dos la incluyen y ninguno depende del otro.
//
// Falsa: la prueba. `nsc.cc1.integer-evaluator` comprueba las respuestas del evaluador
// (`ProjectIntegerEvaluator::isSignedInteger("builtin:int")` y compania) y NO toca la tabla de
// `ProjectTypeConversion` en ninguna linea. No habia ninguna comprobacion cruzada. Las dos copias
// coincidian byte a byte por suerte y por vigilancia, no porque algo lo impidiera. Es la misma
// clase de fallo que las cinco copias de la tabla de imports (docs 102 y 103) y que la busqueda
// del enlazador reimplementada en Python (doc 105): un comentario que promete un guardian que no
// esta.
//
// AHORA HAY UNA SOLA TABLA y el guardian es el compilador: no se puede desincronizar lo que no
// esta duplicado.
//
// LOS DOS HECHOS QUE ESTA TABLA FIJA, y que no son obvios:
//
//   * `builtin:char` mide OCHO bits y NO tiene signo (CC1-100). No es un `wchar_t` ni una unidad
//     UTF-16: es un byte de texto. De ahi cuelga toda la decision de UTF-8 del proyecto.
//   * `builtin:sbyte` es el UNICO entero de 8 bits CON signo (ID 89). Es lo que hace que
//     `(sbyte)200` normalice a -56 y `(byte)300` a 44.
//   * `nint`/`nuint` miden 64 bits en este objetivo (Windows x64). El dia que haya otro objetivo,
//     estas dos filas son lo que hay que hacer depender del target -- y estan en un solo sitio.
struct IntegerTypeInfo final
{
    std::string_view key;
    unsigned width;
    bool isSigned;
};

inline constexpr std::array<IntegerTypeInfo, 11> kIntegerTypes = {{
    {"builtin:sbyte", 8, true},
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

// Devuelve la fila de `typeKey`, o nullptr si no es un entero incorporado. `constexpr` para que
// una comprobacion en tiempo de compilacion pueda usarla.
constexpr const IntegerTypeInfo* findIntegerType(std::string_view typeKey) noexcept
{
    for (const IntegerTypeInfo& info : kIntegerTypes)
    {
        if (info.key == typeKey) return &info;
    }
    return nullptr;
}

constexpr bool isIntegerTypeKey(std::string_view typeKey) noexcept
{
    return findIntegerType(typeKey) != nullptr;
}

// Las tres propiedades que este proyecto ha decidido y que conviene que fallen en el BUILD si
// alguien las cambia sin querer, no en una fixture tres pasos mas abajo.
static_assert(findIntegerType("builtin:char") != nullptr &&
                  findIntegerType("builtin:char")->width == 8 &&
                  !findIntegerType("builtin:char")->isSigned,
    "CC1-100: builtin:char mide 8 bits y no tiene signo");
static_assert(findIntegerType("builtin:sbyte") != nullptr &&
                  findIntegerType("builtin:sbyte")->width == 8 &&
                  findIntegerType("builtin:sbyte")->isSigned,
    "ID 89: sbyte es el unico entero de 8 bits con signo");
static_assert(findIntegerType("builtin:nint") != nullptr &&
                  findIntegerType("builtin:nint")->width == 64,
    "nint mide 64 bits en Windows x64");
static_assert(findIntegerType("builtin:bool") == nullptr,
    "bool NO es un entero incorporado a efectos de promocion ni de evaluacion de constantes");
}
