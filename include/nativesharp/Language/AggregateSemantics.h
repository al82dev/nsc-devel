#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language
{
struct TupleElement final
{
    std::string typeKey;
    std::string name;
};

std::string tupleIdentity(std::span<const TupleElement> elements);
bool tupleNamesAffectIdentity() noexcept;
bool tupleConversionExists(
    std::span<const std::string> sourceTypes,
    std::span<const std::string> targetTypes) noexcept;

struct AggregateFieldLayout final
{
    std::size_t size = 0;
    std::size_t alignment = 1;
};

struct AggregateLayout final
{
    std::vector<std::size_t> offsets;
    std::size_t size = 0;
    std::size_t alignment = 1;
};

std::optional<AggregateLayout> computeWindowsX64AggregateLayout(
    std::span<const AggregateFieldLayout> fields) noexcept;

enum class WindowsX64AggregateReturn
{
    DirectInteger,
    IndirectSRet
};

WindowsX64AggregateReturn classifyWindowsX64AggregateReturn(std::size_t size) noexcept;

enum class DeconstructionBindingMode
{
    Value,
    Reference,
    Move,
    Discard
};

struct DeconstructionBinding final
{
    std::string name;
    DeconstructionBindingMode mode = DeconstructionBindingMode::Value;
};

bool validateDeconstructionBindings(
    std::span<const DeconstructionBinding> bindings,
    std::string& error);

enum class RecordDerive : std::uint8_t
{
    Equality = 1U << 0U,
    Hash = 1U << 1U,
    Print = 1U << 2U,
    Clone = 1U << 3U
};

class RecordDeriveSet final
{
public:
    constexpr RecordDeriveSet() noexcept = default;
    constexpr explicit RecordDeriveSet(RecordDerive derive) noexcept
        : bits_(static_cast<std::uint8_t>(derive)) {}
    constexpr bool contains(RecordDerive derive) const noexcept
    {
        return (bits_ & static_cast<std::uint8_t>(derive)) != 0;
    }
    constexpr RecordDeriveSet with(RecordDerive derive) const noexcept
    {
        RecordDeriveSet result;
        result.bits_ = bits_ | static_cast<std::uint8_t>(derive);
        return result;
    }
private:
    std::uint8_t bits_ = 0;
};

struct PromotedField final
{
    std::string name;
    std::string typeKey;
    bool isPublic = false;
    bool isReadonly = false;
};

bool validatePromotedFields(std::span<const PromotedField> fields, std::string& error);
bool validateAggregateSpread(
    std::size_t sourceCount,
    std::span<const std::string> explicitFields,
    std::string& error);
}
