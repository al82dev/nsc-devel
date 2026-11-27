#pragma once
#include "nativesharp/Frontend/ProjectImplicitConversion.h"
#include <array>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
inline bool isCppArithmeticArgumentType(std::string_view typeKey) noexcept
{
    static constexpr std::array<std::string_view, 12> arithmetic = {
        "builtin:sbyte", "builtin:byte", "builtin:short", "builtin:ushort", "builtin:int", "builtin:uint",
        "builtin:long", "builtin:ulong", "builtin:nint", "builtin:nuint", "builtin:float", "builtin:double"};
    for (const std::string_view candidate : arithmetic)
    {
        if (candidate == typeKey) return true;
    }
    return false;
}

inline bool isCppIntegerArgumentType(std::string_view typeKey) noexcept
{
    return isCppArithmeticArgumentType(typeKey) && typeKey != "builtin:float" && typeKey != "builtin:double";
}

// EN: The standard arithmetic conversions C++ applies to an argument of an imported C++ function, method or
//     constructor ([conv.integral], [conv.double], [conv.fpint]): `double` to a `float` parameter, `int` to
//     `unsigned int`, and `bool` to an integer ([conv.prom]). It is the set the C boundary already converts
//     (Items 3.3 and L-015) and it applies only at a C++ boundary, never between NativeSharp values; a number still
//     reaches a `bool` only through an explicit cast. Rank 5, as at the C boundary, so a NativeSharp implicit
//     conversion stays better and two C++ overloads that both need one are ordered by T19-006's C++ ranking.
inline ProjectImplicitConversion classifyCppArithmeticArgumentConversion(
    std::string_view source, std::string_view target)
{
    const bool sourceFits = isCppArithmeticArgumentType(source) ||
        (source == "builtin:bool" && isCppIntegerArgumentType(target));
    const bool converts = source != target && sourceFits && isCppArithmeticArgumentType(target);
    return {converts ? ProjectConversionKind::Numeric : ProjectConversionKind::None,
            converts ? 5 : 1000, std::string(source), std::string(target), std::nullopt};
}
}
