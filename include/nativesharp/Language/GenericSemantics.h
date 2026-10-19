#pragma once

#include "nativesharp/Language/AliasKind.h"
#include "nativesharp/Language/ConstGenericKind.h"
#include "nativesharp/Language/ConstGenericValue.h"
#include "nativesharp/Language/ConstraintSet.h"
#include "nativesharp/Language/InstantiationBudget.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
bool normalizeConstraints(
    std::span<const std::string> constraints,
    ConstraintSet& result,
    std::string& error);

bool validateConstGeneric(const ConstGenericValue& value, std::string& error);
std::string constGenericArgumentMarker(
    std::string_view typeKey,
    std::string_view normalizedValue);
bool parseConstGenericArgumentMarker(
    std::string_view marker,
    std::string& typeKey,
    std::string& normalizedValue) noexcept;
std::string genericInstantiationIdentity(
    std::string_view definition,
    std::span<const std::string> typeArguments,
    std::span<const ConstGenericValue> values);

bool validatePackExpansion(
    std::span<const std::size_t> packLengths,
    std::size_t expansionBudget,
    std::string& error) noexcept;

bool bidirectionalInferenceAccepts(
    std::string_view producedType,
    std::string_view expectedType,
    bool requiresArbitraryConversion) noexcept;

std::string aliasIdentity(
    AliasKind kind,
    std::string_view module,
    std::string_view name,
    std::string_view representation);

bool specializationStrictlyDominates(
    std::span<const std::string> candidateConstraints,
    std::span<const std::string> fallbackConstraints);

}
