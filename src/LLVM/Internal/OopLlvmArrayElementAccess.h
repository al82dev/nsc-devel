#pragma once

#include <cstdint>

namespace nativesharp::llvm_backend
{
// EN: Freeze 3.0 DEC-11: what the expression around an element of an array of structs with a
//     destructor (or owned fields) does with that element. Computed once per lowered tree by
//     OopLlvmImplementation::classifyArrayElementAccesses; it decides the liveness work done when
//     the element address is lowered.
//     - Read: any use that reads the element (a load, a field read, a method receiver).
//     - WholeStore: the target of `a[i] = v`.
//     - FieldStore: the base of a field store `a[i].F = v` (also nested fields).
//     - Move: the operand of `move(a[i])`.
//     - Delete: the operand of `delete a[i]`.
// ES: Congelamiento 3.0 DEC-11: lo que hace la expresion alrededor de un elemento de un arreglo de
//     structs con destructor (o campos owned) con ese elemento. Se calcula una vez por arbol bajado
//     en OopLlvmImplementation::classifyArrayElementAccesses; decide el trabajo de vida que se hace
//     al bajar la direccion del elemento.
//     - Read: todo uso que lee el elemento (una carga, la lectura de un campo, un receptor).
//     - WholeStore: el destino de `a[i] = v`.
//     - FieldStore: la base de una escritura de campo `a[i].F = v` (tambien campos anidados).
//     - Move: el operando de `move(a[i])`.
//     - Delete: el operando de `delete a[i]`.
enum class OopLlvmArrayElementAccess : std::uint8_t
{
    Read,
    WholeStore,
    FieldStore,
    Move,
    Delete
};
}
