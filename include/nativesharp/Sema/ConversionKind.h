#pragma once

#include <string_view>

namespace nativesharp::sema
{
enum class ConversionKind
{
    Identity,
    IntegralPromotion,
    ImplicitConstantInteger,
    ImplicitNumeric,
    ImplicitNullPointer,
    ImplicitPointerToVoid,
    ImplicitStringLiteralToCharPointer,
    ExplicitNumeric,
    ExplicitEnum,
    ExplicitPointer,
    ExplicitPointerInteger,
    ExplicitNullPointer,
    ToVoid
};

std::string_view ToString(ConversionKind kind) noexcept;
}
