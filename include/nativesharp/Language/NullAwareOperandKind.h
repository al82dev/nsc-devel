#pragma once

namespace nativesharp::language
{
enum class NullAwareOperandKind
{
    NotNullable,
    Pointer,
    Reference,
    StaticOptional
};
}
