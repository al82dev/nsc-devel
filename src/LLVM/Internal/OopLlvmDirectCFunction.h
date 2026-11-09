#pragma once

#include "nativesharp/Frontend/ProjectOopMirFunction.h"

namespace nativesharp::llvm_backend
{
// EN: T08-D18: an imported C or C/C++ external function (a declaration without a NativeSharp body) converted to a
//     C function pointer is its own address, as in C; a callback boundary only guards NativeSharp bodies.
// ES: T08-D18: una funcion C importada o externa C/C++ (una declaracion sin cuerpo NativeSharp) convertida a un
//     puntero a funcion C es su propia direccion, como en C; la frontera de callback solo protege cuerpos
//     NativeSharp.
inline bool directCFunctionAddress(const frontend::ProjectOopMirFunction* function) noexcept
{
    return function != nullptr && function->body == nullptr && (function->isImportedC || function->isCppExternal);
}
}
