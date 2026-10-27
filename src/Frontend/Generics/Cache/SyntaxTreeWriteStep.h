#pragma once

#include "nativesharp/AST/AstNode.h"
#include "nativesharp/AST/TypeSyntax.h"

#include <cstddef>
#include <cstdint>

namespace nativesharp::frontend::internal
{
// EN: One pending piece of work of the syntax tree serializer in TemplateCatalogCodec.cpp
//     (`appendSyntaxTree`). A node or a type is written as a fixed sequence of fields with nested trees
//     in between. Each step writes the fields up to the next nested tree and schedules that tree and
//     the step that continues after it, so the serializer needs a worklist instead of recursion.
//     `node` is set for the Node* kinds and `type` for the Type* kinds; `index` is the generic
//     parameter, constraint clause or type modifier a step works on. The pointers are borrowed from
//     the tree being written and must outlive the serialization.
// ES: Una pieza de trabajo pendiente del serializador de árboles sintácticos de TemplateCatalogCodec.cpp
//     (`appendSyntaxTree`). Un nodo o un tipo se escribe como una secuencia fija de campos con árboles
//     anidados en medio. Cada paso escribe los campos hasta el siguiente árbol anidado y agenda ese
//     árbol y el paso que continúa después, así que el serializador usa una lista de trabajo en lugar
//     de recursión. `node` se llena en los tipos Node* y `type` en los Type*; `index` es el parámetro
//     genérico, la cláusula de restricción o el modificador de tipo sobre el que trabaja el paso. Los
//     punteros se toman prestados del árbol que se escribe y deben vivir más que la serialización.
struct SyntaxTreeWriteStep final
{
    enum class Kind : std::uint8_t
    {
        Node,
        NodeTypeList,
        NodeMetadata,
        NodeGenericParameter,
        NodeGenericParameterTail,
        NodeLifetimes,
        NodeConstraint,
        NodeConstraintProjection,
        NodeConstraintTypes,
        NodeConstraintTail,
        NodeOperands,
        NodeChildren,
        Type,
        TypeFunction,
        TypeModifiers,
        TypeModifier,
        TypeEnd
    };

    Kind kind = Kind::Node;
    const ast::AstNode* node = nullptr;
    const ast::TypeSyntax* type = nullptr;
    std::size_t index = 0;
};
}
