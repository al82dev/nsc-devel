#pragma once

namespace nativesharp::ast
{
enum class TypeModifierSyntaxKind
{
    Pointer,
    Array,
    DynamicArray,
    RangeArray,
    FlexibleArray,
    // C T[]: unknown bound, not a flexible struct member and not a zero-sized object.
    IncompleteArray
};
}
