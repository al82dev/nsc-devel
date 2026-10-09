#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::target::StructFieldLayout;
using nativesharp::target::StructFieldRequest;
using nativesharp::target::StructLayout;
using nativesharp::target::StructLayoutOptions;
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TypeLayout;

enum class BarrierPosition : std::uint8_t
{
    Leading,
    Middle,
    Trailing
};

StructFieldRequest bitField(TypeLayout layout, std::uint64_t width, bool unnamed = false)
{
    return {layout, std::nullopt, std::nullopt, false, width, unnamed};
}

StructFieldRequest barrier(TypeLayout layout)
{
    return bitField(layout, 0, true);
}

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-019", "Host.Target", "unit",
        std::move(profile), {"target", "zero-width-bit-field"}, std::move(function)});
}

bool same(const StructLayout& left, const StructLayout& right)
{
    if (left.size != right.size || left.alignment != right.alignment || left.fields.size() != right.fields.size()) return false;
    for (std::size_t index = 0; index < left.fields.size(); ++index)
    {
        const auto& a = left.fields[index];
        const auto& b = right.fields[index];
        if (a.offset != b.offset || a.size != b.size || a.alignment != b.alignment || a.bitOffset != b.bitOffset || a.bitWidth != b.bitWidth) return false;
    }
    return true;
}

std::optional<std::uint64_t> alignReference(std::uint64_t value, std::uint64_t alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return std::nullopt;
    const std::uint64_t remainder = value % alignment;
    const std::uint64_t padding = remainder == 0 ? 0 : alignment - remainder;
    if (padding > std::numeric_limits<std::uint64_t>::max() - value) return std::nullopt;
    return value + padding;
}

std::optional<StructLayout> reference(
    std::span<const StructFieldRequest> fields, StructLayoutOptions options = {})
{
    if (options.isUnion || (options.pack != 0 && (options.pack & (options.pack - 1)) != 0))
        return std::nullopt;
    if (fields.empty()) return StructLayout{1, 1, {}};
    std::uint64_t currentOffset = 0;
    std::uint64_t extent = 0;
    std::uint64_t aggregateAlignment = 1;
    bool active = false;
    std::uint64_t activeOffset = 0;
    std::uint64_t activeSize = 0;
    std::uint64_t bitsUsed = 0;
    std::vector<StructFieldLayout> output;
    output.reserve(fields.size());
    for (const auto& request : fields)
    {
        if (!request.bitWidth || request.layout.size == 0 || request.layout.alignment == 0 ||
            request.explicitOffset || request.minimumAlignment || request.isFlexibleArray)
            return std::nullopt;
        const std::uint64_t width = request.bitWidth.value_or(0);
        if (request.layout.size > std::numeric_limits<std::uint64_t>::max() / 8U ||
            width > request.layout.size * 8U || (width == 0 && !request.isUnnamedBitField))
            return std::nullopt;
        const std::uint64_t effective = options.pack == 0
            ? request.layout.alignment
            : (std::min)(request.layout.alignment, options.pack);
        if (width == 0)
        {
            std::uint64_t offset = 0;
            if (active)
            {
                const auto aligned = alignReference(currentOffset, effective);
                if (!aligned) return std::nullopt;
                offset = *aligned;
                currentOffset = offset;
                extent = (std::max)(extent, offset);
                aggregateAlignment = (std::max)(aggregateAlignment, effective);
            }
            output.push_back({offset, 0, effective, 0, 0});
            active = false;
            bitsUsed = 0;
            continue;
        }
        const std::uint64_t unitBits = request.layout.size * 8U;
        if (!active || activeSize != request.layout.size || width > unitBits - bitsUsed)
        {
            const auto aligned = alignReference(currentOffset, effective);
            if (!aligned || request.layout.size > std::numeric_limits<std::uint64_t>::max() - *aligned)
                return std::nullopt;
            activeOffset = *aligned;
            activeSize = request.layout.size;
            bitsUsed = 0;
            currentOffset = activeOffset + activeSize;
            extent = (std::max)(extent, currentOffset);
            active = true;
        }
        output.push_back({activeOffset, activeSize, effective, bitsUsed, width});
        bitsUsed += width;
        aggregateAlignment = (std::max)(aggregateAlignment, effective);
    }
    const auto size = alignReference(extent, aggregateAlignment);
    if (!size) return std::nullopt;
    return StructLayout{*size, aggregateAlignment, std::move(output)};
}

void expectReference(TestContext& context, const TargetDataLayout& layout,
    std::span<const StructFieldRequest> fields, StructLayoutOptions options, std::string message)
{
    const auto expected = reference(fields, options);
    const auto actual = layout.layoutStruct(fields, options);
    context.expect(expected.has_value() == actual.has_value() &&
        (!expected || same(*expected, *actual)), std::move(message));
}

std::vector<StructFieldRequest> fixture(
    BarrierPosition position, std::uint64_t barrierAlignment)
{
    std::vector<StructFieldRequest> fields;
    if (position != BarrierPosition::Leading) fields.push_back(bitField({1, 1}, 3));
    fields.push_back(barrier({barrierAlignment, barrierAlignment}));
    if (position != BarrierPosition::Trailing) fields.push_back(bitField({1, 1}, 2));
    return fields;
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-145", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{barrier({4, 4})};
        expectReference(context, layout, fields, {}, "TF-TGT-145 leading barrier must stay at offset zero");
    });
    addCase(registry, "TF-TGT-146", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{bitField({1, 1}, 3), barrier({4, 4}), bitField({1, 1}, 2)};
        expectReference(context, layout, fields, {}, "TF-TGT-146 active barrier must align and close the unit");
    });
    addCase(registry, "TF-TGT-147", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        const auto fields = fixture(BarrierPosition::Middle, 8);
        expectReference(context, fresh, fields, {}, "TF-TGT-147 fresh layout must require no external prerequisite");
    });
}

void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-148", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array namedZero{bitField({4, 4}, 0, false)};
        context.expect(!layout.layoutStruct(namedZero, {}), "TF-TGT-148 named zero-width field must fail");
    });
    addCase(registry, "TF-TGT-149", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const auto leading = fixture(BarrierPosition::Leading, 4);
        const std::array duplicateLeading{barrier({2, 2}), barrier({8, 8})};
        const auto middle = fixture(BarrierPosition::Middle, 4);
        const auto trailing = fixture(BarrierPosition::Trailing, 8);
        expectReference(context, layout, leading, {}, "TF-TGT-149 leading barrier must be exact");
        expectReference(context, layout, duplicateLeading, {}, "TF-TGT-149 duplicate leading barriers must stay at zero");
        expectReference(context, layout, middle, {}, "TF-TGT-149 middle barrier must close the active unit");
        expectReference(context, layout, trailing, {}, "TF-TGT-149 trailing barrier must raise final boundary");
    });
}

void registerMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-150", "PR", [](TestContext& context) {
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL};
        constexpr std::array packs{0ULL, 1ULL, 2ULL, 4ULL, 8ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array positions{BarrierPosition::Leading,
            BarrierPosition::Middle, BarrierPosition::Trailing};
        for (const auto position : positions)
            for (const auto alignment : alignments)
                for (const auto pack : packs)
                {
                    const auto fields = fixture(position, alignment);
                    expectReference(context, layout, fields, {false, pack, 0},
                        "TF-TGT-150 barrier position matrix must match reference");
                }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-151", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fixtures{fixture(BarrierPosition::Leading, 4),
            fixture(BarrierPosition::Middle, 4), fixture(BarrierPosition::Trailing, 8),
            std::vector<StructFieldRequest>{barrier({2, 2}), barrier({8, 8})},
            std::vector<StructFieldRequest>{bitField({2, 2}, 7), barrier({4, 4}), bitField({2, 2}, 9)}};
        std::array<std::optional<StructLayout>, fixtures.size()> forward{};
        std::array<std::optional<StructLayout>, fixtures.size()> reverse{};
        for (std::size_t index = 0; index < fixtures.size(); ++index)
            forward[index] = layout.layoutStruct(fixtures[index], {});
        for (std::size_t index = fixtures.size(); index-- > 0;)
            reverse[index] = layout.layoutStruct(fixtures[index], {});
        for (std::size_t index = 0; index < fixtures.size(); ++index)
        {
            const auto expected = reference(fixtures[index]);
            const auto repeated = layout.layoutStruct(fixtures[index], {});
            context.expect(expected && forward[index] && reverse[index] && repeated &&
                same(*expected, *forward[index]) && same(*forward[index], *reverse[index]) &&
                same(*forward[index], *repeated),
                "TF-TGT-151 invocation order and repetition must preserve exact barrier layout");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-152", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t pairCount = 32768;
        std::vector<StructFieldRequest> fields;
        fields.reserve(pairCount * 2);
        for (std::size_t index = 0; index < pairCount; ++index)
        {
            fields.push_back(bitField({1, 1}, 1));
            fields.push_back(barrier({1, 1}));
        }
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const auto expected = reference(fields);
        const auto actual = layout.layoutStruct(fields, {});
        context.expect(expected && actual && actual->size == pairCount && actual->alignment == 1 &&
            actual->fields.size() == pairCount * 2 && same(*expected, *actual),
            "TF-TGT-152 large barrier sequence must remain exact");
        const std::array namedZero{bitField({1, 1}, 0, false)};
        context.expect(!layout.layoutStruct(namedZero, {}), "TF-TGT-152 named zero-width field must remain rejected");
    });
}
}

void registerTargetZeroWidthBitFieldModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
