#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/ExpressionFeature.h"
#include "nativesharp/Language/ExpressionLoweringContract.h"
#include "nativesharp/Language/InterpolationPart.h"
#include "nativesharp/Language/InterpolationPartKind.h"
#include "nativesharp/Language/NullAwareOperandKind.h"
#include "nativesharp/Language/RangeEndpoint.h"
#include "nativesharp/Language/ResolvedRange.h"
#include "nativesharp/Language/StaticInterpolationHandler.h"

namespace nativesharp::language
{
const ExpressionLoweringContract* expressionLoweringContract(ExpressionFeature feature) noexcept;
bool expressionFeatureAvailable(ExpressionFeature feature) noexcept;

bool isTypedParameterPackType(std::string_view typeKey) noexcept;

NullAwareOperandKind classifyNullAwareOperand(std::string_view typeKey) noexcept;
bool supportsNullAwareOperators(std::string_view typeKey) noexcept;

std::optional<std::size_t> resolveRangeEndpoint(
    RangeEndpoint endpoint, std::size_t length) noexcept;
std::optional<ResolvedRange> resolveHalfOpenRange(
    std::optional<RangeEndpoint> start,
    std::optional<RangeEndpoint> end,
    std::size_t length) noexcept;

bool parseStaticInterpolation(
    std::string_view contents,
    std::vector<InterpolationPart>& parts,
    std::string& error);

bool renderStaticInterpolation(
    std::span<const InterpolationPart> parts,
    StaticInterpolationHandler& handler,
    std::string& error);

// Built-in handler selected for an ordinary $"..." whose holes are bool/integer constants.
// Its result is emitted as immutable program data; it does not call String or observe a locale.
bool builtinStaticInterpolationHoleSupported(const InterpolationPart& part) noexcept;
bool renderBuiltinStaticInterpolation(
    std::span<const InterpolationPart> parts,
    std::string& result,
    std::string& error);
}
