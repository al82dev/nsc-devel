#pragma once

#include "../LlvmCApi.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

// EN: Freeze 3.0 DEC-11: where the live/dead bit of one array element is. `byte` is the address of
//     the bitmap byte that holds it (the bitmap follows the element data) and `mask` the i8 with
//     that bit set. Both values belong to the LLVM function being lowered; the element address
//     lowering produces it and the store, move or delete around the element consumes it.
// ES: Congelamiento 3.0 DEC-11: donde esta el bit vivo/muerto de un elemento de arreglo. `byte` es
//     la direccion del byte del mapa de bits que lo contiene (el mapa sigue a los datos) y `mask` el
//     i8 con ese bit encendido. Ambos pertenecen a la funcion LLVM en curso; la bajada de la
//     direccion del elemento lo produce y el store, move o delete alrededor del elemento lo consume.
struct OopLlvmArrayElementLiveness final
{
    LLVMValueRef byte = nullptr;
    LLVMValueRef mask = nullptr;
};
}
