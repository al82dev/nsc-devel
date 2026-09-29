#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
enum class ValueState { Initialized, Moved, PartiallyMoved };

struct MoveState final
{
    bool isCopy = false;
    bool hasCustomDestructor = false;
    std::vector<bool> initializedFields;
};

bool moveWholeValue(MoveState& state, std::string& error) noexcept;
bool moveField(MoveState& state, std::size_t field, std::string& error) noexcept;
ValueState classifyValueState(const MoveState& state) noexcept;
std::size_t requiredDropFlagCount(const MoveState& state) noexcept;

struct BorrowPlace final
{
    std::string root;
    std::vector<std::string> projections;
};

bool borrowsOverlap(const BorrowPlace& left, const BorrowPlace& right) noexcept;
bool mayTakeExclusiveBorrows(const BorrowPlace& left, const BorrowPlace& right) noexcept;

std::optional<std::string> inferElidedOutputLifetime(
    std::span<const std::string> inputLifetimes,
    std::optional<std::string_view> receiverLifetime);
bool outlives(std::span<const std::string> chain, std::string_view longer, std::string_view shorter);

enum class CleanupKind { Drop, DeferByReference, DeferByValue };
struct CleanupAction final { CleanupKind kind; std::string name; };
std::vector<CleanupAction> cleanupExitOrder(std::span<const CleanupAction> registered);

bool operationPreservesPin(std::string_view operation) noexcept;

enum class EscapeClass { NoEscape, Return, CallerOwned, Global, Unknown };
struct EscapeSummary final
{
    std::string functionIdentity;
    std::vector<EscapeClass> parameters;
};
std::string encodeEscapeSummary(const EscapeSummary& summary);
bool decodeEscapeSummary(std::string_view encoded, EscapeSummary& summary, std::string& error);
}
