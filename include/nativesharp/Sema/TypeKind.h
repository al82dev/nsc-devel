#pragma once

#include <string_view>

namespace nativesharp::sema
{
enum class TypeKind
{
    Error,
    Void,
    Bool,
    Byte,
    Char,
    Short,
    UShort,
    Int,
    UInt,
    Long,
    ULong,
    NInt,
    NUInt,
    Float,
    Double,
    Pointer,
    Array,
    Struct,
    Enum,
    Function,
    NullLiteral,
    StringLiteral
};

std::string_view typeKindName(TypeKind kind) noexcept;
}
