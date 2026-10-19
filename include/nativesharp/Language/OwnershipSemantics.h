#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "nativesharp/Language/BorrowPlace.h"
#include "nativesharp/Language/CleanupAction.h"
#include "nativesharp/Language/CleanupKind.h"
#include "nativesharp/Language/EscapeClass.h"
#include "nativesharp/Language/EscapeSummary.h"
#include "nativesharp/Language/MoveState.h"
#include "nativesharp/Language/ValueState.h"

namespace nativesharp::language
{
bool moveWholeValue(MoveState& state, std::string& error) noexcept;
bool moveField(MoveState& state, std::size_t field, std::string& error) noexcept;
ValueState classifyValueState(const MoveState& state) noexcept;
std::size_t requiredDropFlagCount(const MoveState& state) noexcept;

bool borrowsOverlap(const BorrowPlace& left, const BorrowPlace& right) noexcept;
bool mayTakeExclusiveBorrows(const BorrowPlace& left, const BorrowPlace& right) noexcept;

std::optional<std::string> inferElidedOutputLifetime(
    std::span<const std::string> inputLifetimes,
    std::optional<std::string_view> receiverLifetime);
bool outlives(std::span<const std::string> chain, std::string_view longer, std::string_view shorter);

std::vector<CleanupAction> cleanupExitOrder(std::span<const CleanupAction> registered);

bool operationPreservesPin(std::string_view operation) noexcept;

std::string encodeEscapeSummary(const EscapeSummary& summary);
bool decodeEscapeSummary(std::string_view encoded, EscapeSummary& summary, std::string& error);
}
