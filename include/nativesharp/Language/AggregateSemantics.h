#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/AggregateFieldLayout.h"
#include "nativesharp/Language/AggregateLayout.h"
#include "nativesharp/Language/DeconstructionBinding.h"
#include "nativesharp/Language/DeconstructionBindingMode.h"
#include "nativesharp/Language/PromotedField.h"
#include "nativesharp/Language/RecordDerive.h"
#include "nativesharp/Language/RecordDeriveSet.h"
#include "nativesharp/Language/TupleElement.h"
#include "nativesharp/Language/WindowsX64AggregateReturn.h"

namespace nativesharp::language
{
std::string tupleIdentity(std::span<const TupleElement> elements);
bool tupleNamesAffectIdentity() noexcept;
bool tupleConversionExists(
    std::span<const std::string> sourceTypes,
    std::span<const std::string> targetTypes) noexcept;

std::optional<AggregateLayout> computeWindowsX64AggregateLayout(
    std::span<const AggregateFieldLayout> fields) noexcept;

WindowsX64AggregateReturn classifyWindowsX64AggregateReturn(std::size_t size) noexcept;

bool validateDeconstructionBindings(
    std::span<const DeconstructionBinding> bindings,
    std::string& error);

bool validatePromotedFields(std::span<const PromotedField> fields, std::string& error);
bool validateAggregateSpread(
    std::size_t sourceCount,
    std::span<const std::string> explicitFields,
    std::string& error);
}
