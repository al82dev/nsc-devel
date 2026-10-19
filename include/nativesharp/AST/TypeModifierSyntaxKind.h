#pragma once

namespace nativesharp::ast
{
enum class TypeModifierSyntaxKind
{
    Pointer,
    Array,
    DynamicArray,
    RangeArray,
    FlexibleArray
};
}
