#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace nativesharp::frontend
{
enum class ProjectConversionKind
{
    None,
    Identity,
    Never,
    IntegralPromotion,
    ConstantInteger,
    Numeric,
    Tuple,
    Function,
    NullPointer,
    NullReference,
    ReferenceUpcast,
    ReferenceInterface,
    PointerToVoid,
    StringLiteralToCharPointer,
    StringLiteralToNativeString,
    // NS304-T11-F01: a mutable view (Span<T>) becomes an immutable view (ReadOnlySpan<T>). Identity
    // element, one-way (immutable never gains mutability). Structural (pointer const + length).
    View,
    UserDefinedImplicit,
    UserDefinedExplicit
};
}
