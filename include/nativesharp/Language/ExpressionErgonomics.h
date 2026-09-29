#pragma once

#include "nativesharp/Language/LanguageEdition.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

struct ExpressionLoweringContract final
{
    ExpressionFeature feature = ExpressionFeature::NamedArguments;
    std::string_view spelling;
    bool sourceOrder = true;
    bool primaryEvaluatedOnce = true;
    bool mayAllocateImplicitly = false;
    bool preservesAbi = true;
};

const ExpressionLoweringContract& expressionLoweringContract(ExpressionFeature feature) noexcept;
bool expressionFeatureAvailable(ExpressionFeature feature, LanguageEdition edition) noexcept;

bool isTypedParameterPackType(std::string_view typeKey) noexcept;

enum class NullAwareOperandKind
{
    NotNullable,
    Pointer,
    Reference,
    StaticOptional
};

NullAwareOperandKind classifyNullAwareOperand(std::string_view typeKey) noexcept;
bool supportsNullAwareOperators(std::string_view typeKey) noexcept;

struct RangeEndpoint final
{
    std::size_t value = 0;
    bool fromEnd = false;
};

struct ResolvedRange final
{
    std::size_t start = 0;
    std::size_t end = 0;
};

std::optional<std::size_t> resolveRangeEndpoint(
    RangeEndpoint endpoint, std::size_t length) noexcept;
std::optional<ResolvedRange> resolveHalfOpenRange(
    std::optional<RangeEndpoint> start,
    std::optional<RangeEndpoint> end,
    std::size_t length) noexcept;

enum class InterpolationPartKind
{
    Literal,
    Hole
};

struct InterpolationPart final
{
    InterpolationPartKind kind = InterpolationPartKind::Literal;
    std::string text;
    std::string format;
};

bool parseStaticInterpolation(
    std::string_view contents,
    std::vector<InterpolationPart>& parts,
    std::string& error);
}
