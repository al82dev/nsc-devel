#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"
#include "nativesharp/Frontend/BuiltinIntegerTypes.h"

#include <string_view>

namespace nativesharp::frontend
{
// EL TIPO SUBYACENTE DE UN ENUM, en un solo sitio.
//
// POR QUE ESTA FUNCION EXISTE, y por que esta aqui y no repartida. Hasta el 2026-09-03 todo enum
// media CUATRO BYTES: `TargetDataLayout.cpp` devolvia {4,4} para `BuiltinLayoutKind::Enum` y
// `ProjectOopLlvmLowerer::llvmType` devolvia `i32`, sin preguntarle nada a nadie. Un header de C
// con `enum : uint8_t` dentro de una struct producia entonces un layout SILENCIOSAMENTE
// EQUIVOCADO -- el campo ocupaba cuatro bytes donde C pone uno, y todo lo que iba detras se
// desplazaba.
//
// Eso no es una carencia de expresividad: es una RESPUESTA FALSA, que por el criterio de este
// proyecto es P1. Y el ancho y el signo de un enum son parte del ABI, asi que la unica forma de
// arreglarlo es dejar que el autor los diga.
//
// LA PREGUNTA SE CONTESTA EN CUATRO SITIOS al menos --el tamano, la alineacion, el tipo LLVM y el
// signo de las comparaciones y las extensiones-- y las cuatro respuestas TIENEN que salir de aqui.
// Cuatro copias de esta regla es exactamente la forma de defecto que los docs 102, 103, 106, 112 y
// 115 documentan una y otra vez en este arbol: una regla escrita varias veces con una nota
// pidiendo que alguien se acuerde.
//
// SIN `: tipo` ES `int`, que es lo que ya hacia y lo que hace C por omision. Nada de lo escrito
// hasta hoy cambia de tamano.
//
// SOLO ENTEROS INCORPORADOS. Ni `bool`, ni coma flotante, ni otro enum, ni un tipo de usuario. La
// validacion se hace mirando el nombre escrito contra la tabla unica de enteros
// (BuiltinIntegerTypes.h), sin pasar por la resolucion de nombres: un enum no puede heredar de un
// alias ni de un generico, asi que no hace falta -- y no depender de la resolucion de nombres es
// lo que permite contestar esta pregunta desde el modelo de layout y desde el backend por igual.

// El tipo escrito tras los dos puntos, o vacio si no se escribio ninguno o no es un entero
// incorporado. Devolver vacio en el caso invalido y no `builtin:int` es deliberado: quien valida
// necesita distinguir "no escribio nada" de "escribio algo que no vale", y son dos diagnosticos
// distintos.
inline std::string_view declaredEnumUnderlyingTypeKey(const ast::AstNode* enumSyntax) noexcept
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

// El tipo subyacente EFECTIVO: lo escrito, o `builtin:int` si no se escribio nada. Es lo que
// consultan el modelo de layout, el backend y las reglas de signo.
inline std::string_view enumUnderlyingTypeKey(const ast::AstNode* enumSyntax) noexcept
{
    const std::string_view declared = declaredEnumUnderlyingTypeKey(enumSyntax);
    return declared.empty() ? std::string_view("builtin:int") : declared;
}

static_assert(builtins::findIntegerType("builtin:int") != nullptr,
    "el tipo por omision de un enum tiene que existir en la tabla de enteros");
static_assert(builtins::findIntegerType("builtin:int")->width == 32,
    "un enum sin `: tipo` mide 32 bits, que es lo que hacia antes de que esto existiera");
}
