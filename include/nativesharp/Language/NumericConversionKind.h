#pragma once

namespace nativesharp::language
{
enum class NumericConversionKind
{
    Identity,
    ImplicitLossless,
    ImplicitConstant,
    ExplicitNarrowing,
    ExplicitSignChange,
    ExplicitFloatInteger,
    ExplicitEnumUnderlying,
    Invalid
};
}
