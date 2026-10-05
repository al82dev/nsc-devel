#include "nativesharp/Target/TargetDataLayout.h"

#include <algorithm>
#include <limits>

namespace nativesharp::target
{
TargetDataLayout TargetDataLayout::WindowsX64()
{
    return TargetDataLayout();
}

TargetDataLayout TargetDataLayout::WindowsArm64()
{
    // Windows ARM64 is LLP64 just like Windows x64: the language-visible scalar, pointer and
    // natural aggregate layouts represented by this class are identical. Calling convention
    // classification remains the responsibility of the target backend.
    return TargetDataLayout();
}

TypeLayout TargetDataLayout::layoutOf(BuiltinLayoutKind kind) const noexcept
{
    switch (kind)
    {
        case BuiltinLayoutKind::Bool:
        case BuiltinLayoutKind::SByte:
        case BuiltinLayoutKind::Byte:
        case BuiltinLayoutKind::Char:
            return {1, 1};

        case BuiltinLayoutKind::Short:
        case BuiltinLayoutKind::UShort:
            return {2, 2};

        case BuiltinLayoutKind::Int:
        case BuiltinLayoutKind::UInt:
        case BuiltinLayoutKind::Float:
        case BuiltinLayoutKind::Enum:
            return {4, 4};

        case BuiltinLayoutKind::Long:
        case BuiltinLayoutKind::ULong:
        case BuiltinLayoutKind::NInt:
        case BuiltinLayoutKind::NUInt:
        case BuiltinLayoutKind::Double:
        case BuiltinLayoutKind::Pointer:
            return {8, 8};

        case BuiltinLayoutKind::Int128:
        case BuiltinLayoutKind::UInt128:
            return {16, 16};
    }

    return {0, 1};
}

std::optional<std::uint64_t> TargetDataLayout::strideOf(TypeLayout element) const noexcept
{
    if (element.size == 0 || element.alignment == 0)
    {
        return std::nullopt;
    }

    return alignUp(element.size, element.alignment);
}

std::optional<ArrayLayout> TargetDataLayout::layoutArray(
    TypeLayout element,
    std::uint64_t count) const noexcept
{
    if (count == 0)
    {
        return std::nullopt;
    }

    const std::optional<std::uint64_t> stride = strideOf(element);
    if (!stride.has_value())
    {
        return std::nullopt;
    }

    const std::optional<std::uint64_t> size = checkedMultiply(*stride, count);
    if (!size.has_value())
    {
        return std::nullopt;
    }

    return ArrayLayout{*size, element.alignment, *stride, count};
}

std::optional<StructLayout> TargetDataLayout::layoutStruct(
    std::span<const StructFieldRequest> fields, const StructLayoutOptions& options) const
{
    // C2.2 "union and explicit layout". One walk covers all four shapes, because they differ only
    // in how a field's offset is chosen and how the running size advances:
    //
    //   plain struct  -- next naturally aligned offset, size grows past each field
    //   union         -- offset 0 for everyone, size is the LARGEST field
    //   packed        -- the field's alignment is capped, so the aligned offset is tighter
    //   explicit      -- the field names its offset outright; overlap is allowed, deliberately
    //
    // The aggregate's own alignment is the largest EFFECTIVE field alignment (post-pack), raised
    // to `minimumAlignment` if one was asked for, and the final size is rounded up to it -- the
    // same trailing-padding rule C applies, which is what makes sizeof() agree with a C compiler.
    if (options.pack != 0 && (options.pack & (options.pack - 1)) != 0) return std::nullopt;
    if (options.minimumAlignment != 0 &&
        (options.minimumAlignment & (options.minimumAlignment - 1)) != 0)
    {
        return std::nullopt;
    }
    if (fields.empty())
    {
        const std::uint64_t alignment = options.minimumAlignment == 0 ? 1 : options.minimumAlignment;
        const std::optional<std::uint64_t> size = alignUp(1, alignment);
        if (!size.has_value()) return std::nullopt;
        return StructLayout{*size, alignment, {}};
    }

    std::uint64_t currentOffset = 0;
    std::uint64_t extent = 0;
    std::uint64_t structAlignment = 1;
    bool hasActiveBitFieldUnit = false;
    std::uint64_t activeBitFieldOffset = 0;
    std::uint64_t activeBitFieldSize = 0;
    std::uint64_t activeBitFieldBitsUsed = 0;
    std::vector<StructFieldLayout> fieldLayouts;
    fieldLayouts.reserve(fields.size());

    for (const StructFieldRequest& request : fields)
    {
        const TypeLayout field = request.layout;
        if ((!request.isFlexibleArray && field.size == 0) || field.alignment == 0 ||
            (request.bitWidth.has_value() &&
             (request.explicitOffset.has_value() || request.minimumAlignment.has_value() ||
              request.isFlexibleArray)))
            return std::nullopt;
        std::uint64_t effectiveAlignment =
            options.pack == 0 ? field.alignment : (std::min)(field.alignment, options.pack);
        if (request.minimumAlignment.has_value())
        {
            const std::uint64_t requested = *request.minimumAlignment;
            if (requested == 0 || (requested & (requested - 1)) != 0) return std::nullopt;
            effectiveAlignment = (std::max)(effectiveAlignment, requested);
        }

        if (request.bitWidth.has_value())
        {
            const std::uint64_t width = *request.bitWidth;
            if (field.size > std::numeric_limits<std::uint64_t>::max() / 8U ||
                width > field.size * 8U || (width == 0 && !request.isUnnamedBitField))
                return std::nullopt;

            // MSVC ignores a leading zero-width field. After a live unit it ends that unit and
            // aligns the next field to the zero-width declaration's base type. This alignment
            // also becomes an aggregate requirement. A union has no sequential active unit.
            if (width == 0)
            {
                std::uint64_t zeroOffset = 0;
                if (!options.isUnion && hasActiveBitFieldUnit)
                {
                    const std::optional<std::uint64_t> aligned =
                        alignUp(currentOffset, effectiveAlignment);
                    if (!aligned.has_value()) return std::nullopt;
                    zeroOffset = *aligned;
                    currentOffset = zeroOffset;
                    extent = (std::max)(extent, zeroOffset);
                    structAlignment = (std::max)(structAlignment, effectiveAlignment);
                }
                fieldLayouts.push_back(StructFieldLayout{
                    zeroOffset, 0, effectiveAlignment, std::uint64_t{0}, std::uint64_t{0}});
                hasActiveBitFieldUnit = false;
                activeBitFieldBitsUsed = 0;
                continue;
            }

            if (options.isUnion)
            {
                // Microsoft unions reserve the full declared allocation unit for a bit-field,
                // while that member deliberately contributes no alignment to the union.
                fieldLayouts.push_back(StructFieldLayout{
                    0, field.size, effectiveAlignment, std::uint64_t{0}, width});
                extent = (std::max)(extent, field.size);
                continue;
            }

            const std::uint64_t unitBits = field.size * 8U;
            if (!hasActiveBitFieldUnit || activeBitFieldSize != field.size ||
                width > unitBits - activeBitFieldBitsUsed)
            {
                const std::optional<std::uint64_t> aligned =
                    alignUp(currentOffset, effectiveAlignment);
                if (!aligned.has_value()) return std::nullopt;
                activeBitFieldOffset = *aligned;
                activeBitFieldSize = field.size;
                activeBitFieldBitsUsed = 0;
                const std::optional<std::uint64_t> end =
                    checkedAdd(activeBitFieldOffset, activeBitFieldSize);
                if (!end.has_value()) return std::nullopt;
                currentOffset = *end;
                extent = (std::max)(extent, *end);
                hasActiveBitFieldUnit = true;
            }
            fieldLayouts.push_back(StructFieldLayout{
                activeBitFieldOffset,
                activeBitFieldSize,
                effectiveAlignment,
                activeBitFieldBitsUsed,
                width});
            activeBitFieldBitsUsed += width;
            structAlignment = (std::max)(structAlignment, effectiveAlignment);
            continue;
        }

        hasActiveBitFieldUnit = false;
        activeBitFieldBitsUsed = 0;

        std::uint64_t offset = 0;
        if (request.explicitOffset.has_value())
        {
            offset = *request.explicitOffset;
            if (request.minimumAlignment.has_value() && offset % effectiveAlignment != 0)
                return std::nullopt;
        }
        else if (!options.isUnion)
        {
            const std::optional<std::uint64_t> aligned = alignUp(currentOffset, effectiveAlignment);
            if (!aligned.has_value()) return std::nullopt;
            offset = *aligned;
        }

        fieldLayouts.push_back(StructFieldLayout{
            offset, field.size, effectiveAlignment, std::nullopt, std::nullopt});

        const std::optional<std::uint64_t> end = checkedAdd(offset, field.size);
        if (!end.has_value()) return std::nullopt;
        extent = (std::max)(extent, *end);
        if (!options.isUnion) currentOffset = *end;
        structAlignment = (std::max)(structAlignment, effectiveAlignment);
    }

    structAlignment = (std::max)(structAlignment, options.minimumAlignment);
    const std::optional<std::uint64_t> finalSize = alignUp(extent, structAlignment);
    if (!finalSize.has_value()) return std::nullopt;
    return StructLayout{*finalSize, structAlignment, std::move(fieldLayouts)};
}

std::optional<StructLayout> TargetDataLayout::layoutStruct(
    std::span<const TypeLayout> fields) const
{
    if (fields.empty())
    {
        return StructLayout{1, 1, {}};
    }

    std::uint64_t currentOffset = 0;
    std::uint64_t structAlignment = 1;
    std::vector<StructFieldLayout> fieldLayouts;
    fieldLayouts.reserve(fields.size());

    for (const TypeLayout field : fields)
    {
        if (field.size == 0 || field.alignment == 0)
        {
            return std::nullopt;
        }

        const std::optional<std::uint64_t> alignedOffset = alignUp(currentOffset, field.alignment);
        if (!alignedOffset.has_value())
        {
            return std::nullopt;
        }

        fieldLayouts.push_back(StructFieldLayout{
            *alignedOffset, field.size, field.alignment, std::nullopt, std::nullopt});

        const std::optional<std::uint64_t> nextOffset = checkedAdd(*alignedOffset, field.size);
        if (!nextOffset.has_value())
        {
            return std::nullopt;
        }

        currentOffset = *nextOffset;
        structAlignment = std::max(structAlignment, field.alignment);
    }

    const std::optional<std::uint64_t> finalSize = alignUp(currentOffset, structAlignment);
    if (!finalSize.has_value())
    {
        return std::nullopt;
    }

    return StructLayout{*finalSize, structAlignment, std::move(fieldLayouts)};
}

std::uint64_t TargetDataLayout::maximumNaturalAlignment() const noexcept
{
    return 8;
}

bool TargetDataLayout::isLittleEndian() const noexcept
{
    return true;
}

std::optional<std::uint64_t> TargetDataLayout::checkedAdd(
    std::uint64_t left,
    std::uint64_t right) noexcept
{
    if (right > std::numeric_limits<std::uint64_t>::max() - left)
    {
        return std::nullopt;
    }

    return left + right;
}

std::optional<std::uint64_t> TargetDataLayout::checkedMultiply(
    std::uint64_t left,
    std::uint64_t right) noexcept
{
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left)
    {
        return std::nullopt;
    }

    return left * right;
}

std::optional<std::uint64_t> TargetDataLayout::alignUp(
    std::uint64_t value,
    std::uint64_t alignment) noexcept
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    {
        return std::nullopt;
    }

    const std::uint64_t mask = alignment - 1;
    const std::optional<std::uint64_t> withMask = checkedAdd(value, mask);
    if (!withMask.has_value())
    {
        return std::nullopt;
    }

    return *withMask & ~mask;
}
}
