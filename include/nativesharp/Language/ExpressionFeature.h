#pragma once

namespace nativesharp::language
{
enum class ExpressionFeature
{
    NamedArguments,
    TypedParameterPacks,
    NullAwareOperators,
    RangeIndex,
    GeneralizedInitializers,
    StaticInterpolation,
    WithExpressions
};
}
