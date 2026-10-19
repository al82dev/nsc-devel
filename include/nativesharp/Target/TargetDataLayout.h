#pragma once

#include "nativesharp/Target/ArrayLayout.h"
#include "nativesharp/Target/BuiltinLayoutKind.h"
#include "nativesharp/Target/StructFieldLayout.h"
#include "nativesharp/Target/StructFieldRequest.h"
#include "nativesharp/Target/StructLayout.h"
#include "nativesharp/Target/StructLayoutOptions.h"
#include "nativesharp/Target/TypeLayout.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace nativesharp::target
{
class TargetDataLayout final
{
public:
    static TargetDataLayout WindowsX64();
    static TargetDataLayout WindowsArm64();

    TypeLayout layoutOf(BuiltinLayoutKind kind) const noexcept;
    std::optional<std::uint64_t> strideOf(TypeLayout element) const noexcept;
    std::optional<ArrayLayout> layoutArray(TypeLayout element, std::uint64_t count) const noexcept;
    std::optional<StructLayout> layoutStruct(std::span<const TypeLayout> fields) const;
    // C2.2: the general form. The one-argument overload above is exactly this with default
    // options, kept because most callers have nothing to say about layout.
    std::optional<StructLayout> layoutStruct(
        std::span<const StructFieldRequest> fields, const StructLayoutOptions& options) const;

    std::uint64_t maximumNaturalAlignment() const noexcept;
    bool isLittleEndian() const noexcept;

private:
    static std::optional<std::uint64_t> checkedAdd(
        std::uint64_t left,
        std::uint64_t right) noexcept;

    static std::optional<std::uint64_t> checkedMultiply(
        std::uint64_t left,
        std::uint64_t right) noexcept;

    static std::optional<std::uint64_t> alignUp(
        std::uint64_t value,
        std::uint64_t alignment) noexcept;

    static bool layoutBitField(
        const StructFieldRequest& request,
        const StructLayoutOptions& options,
        std::uint64_t effectiveAlignment,
        std::uint64_t& currentOffset,
        std::uint64_t& extent,
        std::uint64_t& structAlignment,
        bool& hasActiveBitFieldUnit,
        std::uint64_t& activeBitFieldOffset,
        std::uint64_t& activeBitFieldSize,
        std::uint64_t& activeBitFieldBitsUsed,
        std::vector<StructFieldLayout>& fieldLayouts) noexcept;

    static bool layoutRegularField(
        const StructFieldRequest& request,
        const StructLayoutOptions& options,
        std::uint64_t effectiveAlignment,
        std::uint64_t& currentOffset,
        std::uint64_t& extent,
        std::uint64_t& structAlignment,
        std::vector<StructFieldLayout>& fieldLayouts) noexcept;
};
}
