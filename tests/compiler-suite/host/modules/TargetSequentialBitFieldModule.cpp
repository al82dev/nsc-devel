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

StructFieldRequest bitField(TypeLayout layout, std::uint64_t width)
{
    return {layout, std::nullopt, std::nullopt, false, width, false};
}

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-018", "Host.Target", "unit",
        std::move(profile), {"target", "sequential-bit-field"}, std::move(function)});
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
    if (options.isUnion || (options.pack != 0 && (options.pack & (options.pack - 1)) != 0) ||
        (options.minimumAlignment != 0 &&
         (options.minimumAlignment & (options.minimumAlignment - 1)) != 0))
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
        if (width == 0 || request.layout.size > std::numeric_limits<std::uint64_t>::max() / 8U ||
            width > request.layout.size * 8U)
            return std::nullopt;
        const std::uint64_t effective = options.pack == 0
            ? request.layout.alignment
            : (std::min)(request.layout.alignment, options.pack);
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
    aggregateAlignment = (std::max)(aggregateAlignment, options.minimumAlignment);
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

std::vector<StructFieldRequest> overflowSequence()
{
    constexpr std::uint64_t first = std::numeric_limits<std::uint64_t>::max() / 8U;
    constexpr std::uint64_t second = first - 1U;
    std::vector<StructFieldRequest> fields;
    fields.reserve(9);
    for (std::size_t index = 0; index < 9; ++index)
        fields.push_back(bitField({index % 2 == 0 ? first : second, 1}, 1));
    return fields;
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-137", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{bitField({1, 1}, 1)};
        expectReference(context, layout, fields, {}, "TF-TGT-137 minimum sequential bit-field must be exact");
    });
    addCase(registry, "TF-TGT-138", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{bitField({1, 1}, 3), bitField({1, 1}, 5),
            bitField({1, 1}, 1), bitField({2, 2}, 9)};
        expectReference(context, layout, fields, {}, "TF-TGT-138 sharing and new units must match reference");
    });
    addCase(registry, "TF-TGT-139", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        const std::array fields{bitField({4, 4}, 7), bitField({4, 4}, 9)};
        expectReference(context, fresh, fields, {}, "TF-TGT-139 fresh layout must require no external prerequisite");
    });
}

void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-140", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array invalidAlignment{bitField({1, 3}, 1)};
        const auto overflow = overflowSequence();
        context.expect(!layout.layoutStruct(invalidAlignment, {}), "TF-TGT-140 invalid unit alignment must fail");
        context.expect(!layout.layoutStruct(overflow, {}), "TF-TGT-140 sequential extent overflow must fail");
    });
    addCase(registry, "TF-TGT-141", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array exactFill{bitField({1, 1}, 3), bitField({1, 1}, 5)};
        const std::array insufficient{bitField({1, 1}, 7), bitField({1, 1}, 2)};
        const std::array sizeChange{bitField({1, 1}, 1), bitField({2, 2}, 1)};
        const std::array duplicates{bitField({1, 1}, 4), bitField({1, 1}, 4)};
        expectReference(context, layout, exactFill, {}, "TF-TGT-141 exact fill must share one unit");
        expectReference(context, layout, insufficient, {}, "TF-TGT-141 insufficient remaining bits must open a unit");
        expectReference(context, layout, sizeChange, {}, "TF-TGT-141 allocation size change must open a unit");
        expectReference(context, layout, duplicates, {}, "TF-TGT-141 duplicate widths must remain exact");
    });
}

void registerMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-142", "PR", [](TestContext& context) {
        constexpr std::array sizes{1ULL, 2ULL, 4ULL, 8ULL};
        constexpr std::array packs{0ULL, 1ULL, 2ULL, 4ULL, 8ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (const auto size : sizes)
        {
            const std::uint64_t bits = size * 8U;
            const std::array firstWidths{1ULL, bits / 2U, bits - 1U, bits};
            const std::array secondWidths{1ULL, bits / 2U, bits};
            for (const auto pack : packs)
                for (const auto firstWidth : firstWidths)
                    for (const auto secondWidth : secondWidths)
                    {
                        const std::array fields{bitField({size, size}, firstWidth),
                            bitField({size, size}, secondWidth)};
                        expectReference(context, layout, fields, {false, pack, 0},
                            "TF-TGT-142 allocation matrix must match reference");
                    }
        }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-143", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fixtures{
            std::vector<StructFieldRequest>{bitField({1, 1}, 1)},
            std::vector<StructFieldRequest>{bitField({1, 1}, 3), bitField({1, 1}, 5)},
            std::vector<StructFieldRequest>{bitField({1, 1}, 7), bitField({1, 1}, 2)},
            std::vector<StructFieldRequest>{bitField({2, 2}, 5), bitField({2, 2}, 6), bitField({2, 2}, 7)},
            std::vector<StructFieldRequest>{bitField({4, 4}, 31), bitField({8, 8}, 63)}};
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
                "TF-TGT-143 invocation order and repetition must preserve exact packing");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-144", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t count = 65536;
        std::vector<StructFieldRequest> fields(count, bitField({1, 1}, 1));
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const auto expected = reference(fields);
        const auto actual = layout.layoutStruct(fields, {});
        context.expect(expected && actual && actual->size == 8192 && actual->alignment == 1 &&
            actual->fields.size() == count && same(*expected, *actual),
            "TF-TGT-144 large packing sequence must remain exact");
        const auto overflow = overflowSequence();
        context.expect(!layout.layoutStruct(overflow, {}), "TF-TGT-144 overflow sequence must remain rejected");
    });
}
}

void registerTargetSequentialBitFieldModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
