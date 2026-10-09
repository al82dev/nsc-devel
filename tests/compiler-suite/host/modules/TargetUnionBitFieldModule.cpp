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
    registry.add(TestCase{std::move(id), "CF-TGT-020", "Host.Target", "unit",
        std::move(profile), {"target", "union-bit-field"}, std::move(function)});
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
    std::span<const StructFieldRequest> fields, StructLayoutOptions options = {true, 0, 0})
{
    if (!options.isUnion || (options.pack != 0 && (options.pack & (options.pack - 1)) != 0) ||
        (options.minimumAlignment != 0 &&
         (options.minimumAlignment & (options.minimumAlignment - 1)) != 0))
        return std::nullopt;
    if (fields.empty())
    {
        const std::uint64_t alignment = options.minimumAlignment == 0 ? 1 : options.minimumAlignment;
        const auto size = alignReference(1, alignment);
        if (!size) return std::nullopt;
        return StructLayout{*size, alignment, {}};
    }
    std::uint64_t extent = 0;
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
        output.push_back({0, request.layout.size, effective, 0, width});
        extent = (std::max)(extent, request.layout.size);
    }
    const std::uint64_t alignment = (std::max)(std::uint64_t{1}, options.minimumAlignment);
    const auto size = alignReference(extent, alignment);
    if (!size) return std::nullopt;
    return StructLayout{*size, alignment, std::move(output)};
}

void expectReference(TestContext& context, const TargetDataLayout& layout,
    std::span<const StructFieldRequest> fields, StructLayoutOptions options, std::string message)
{
    const auto expected = reference(fields, options);
    const auto actual = layout.layoutStruct(fields, options);
    context.expect(expected.has_value() == actual.has_value() &&
        (!expected || same(*expected, *actual)), std::move(message));
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-153", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{bitField({4, 4}, 1)};
        expectReference(context, layout, fields, {true, 0, 0}, "TF-TGT-153 minimum union bit-field must be exact");
    });
    addCase(registry, "TF-TGT-154", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{bitField({1, 1}, 3), bitField({8, 8}, 63), bitField({4, 4}, 31)};
        expectReference(context, layout, fields, {true, 0, 0}, "TF-TGT-154 union units must remain at offset zero");
    });
    addCase(registry, "TF-TGT-155", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        const std::array fields{bitField({2, 2}, 7), bitField({2, 2}, 9)};
        expectReference(context, fresh, fields, {true, 0, 0}, "TF-TGT-155 fresh union layout must require no external prerequisite");
    });
}

void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-156", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array zero{bitField({4, 4}, 0)};
        const std::array tooWide{bitField({4, 4}, 33)};
        context.expect(!layout.layoutStruct(zero, {true, 0, 0}), "TF-TGT-156 named zero width must fail");
        context.expect(!layout.layoutStruct(tooWide, {true, 0, 0}), "TF-TGT-156 width above unit must fail");
    });
    addCase(registry, "TF-TGT-157", "PR", [](TestContext& context) {
        constexpr std::uint64_t largestUnit = std::numeric_limits<std::uint64_t>::max() / 8U;
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array exact{bitField({4, 4}, 32)};
        const std::array duplicates{bitField({2, 2}, 7), bitField({2, 2}, 7)};
        const std::array largest{bitField({largestUnit, 1}, 1)};
        expectReference(context, layout, exact, {true, 0, 16}, "TF-TGT-157 minimum aggregate alignment must pad union");
        expectReference(context, layout, duplicates, {true, 0, 0}, "TF-TGT-157 duplicate union fields must remain exact");
        expectReference(context, layout, largest, {true, 0, 0}, "TF-TGT-157 largest valid allocation unit must be accepted");
    });
}

void registerMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-158", "PR", [](TestContext& context) {
        constexpr std::array sizes{1ULL, 2ULL, 4ULL, 8ULL};
        constexpr std::array packs{0ULL, 1ULL, 2ULL, 4ULL, 8ULL};
        constexpr std::array minimumAlignments{0ULL, 8ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (const auto size : sizes)
        {
            const std::uint64_t bits = size * 8U;
            const std::array widths{1ULL, bits / 2U, bits};
            for (const auto pack : packs)
                for (const auto minimumAlignment : minimumAlignments)
                    for (const auto width : widths)
                    {
                        const std::array fields{bitField({size, size}, width)};
                        expectReference(context, layout, fields, {true, pack, minimumAlignment},
                            "TF-TGT-158 union bit-field matrix must match reference");
                    }
        }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-159", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fixtures{
            std::vector<StructFieldRequest>{bitField({1, 1}, 1)},
            std::vector<StructFieldRequest>{bitField({1, 1}, 3), bitField({8, 8}, 63)},
            std::vector<StructFieldRequest>{bitField({2, 2}, 16), bitField({4, 4}, 32)},
            std::vector<StructFieldRequest>{bitField({8, 8}, 1), bitField({1, 1}, 8)},
            std::vector<StructFieldRequest>{bitField({4, 4}, 7), bitField({4, 4}, 9), bitField({4, 4}, 16)}};
        std::array<std::optional<StructLayout>, fixtures.size()> forward{};
        std::array<std::optional<StructLayout>, fixtures.size()> reverse{};
        for (std::size_t index = 0; index < fixtures.size(); ++index)
            forward[index] = layout.layoutStruct(fixtures[index], {true, 0, 0});
        for (std::size_t index = fixtures.size(); index-- > 0;)
            reverse[index] = layout.layoutStruct(fixtures[index], {true, 0, 0});
        for (std::size_t index = 0; index < fixtures.size(); ++index)
        {
            const auto expected = reference(fixtures[index]);
            const auto repeated = layout.layoutStruct(fixtures[index], {true, 0, 0});
            context.expect(expected && forward[index] && reverse[index] && repeated &&
                same(*expected, *forward[index]) && same(*forward[index], *reverse[index]) &&
                same(*forward[index], *repeated),
                "TF-TGT-159 invocation order and repetition must preserve union bit-fields");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-160", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t count = 65536;
        constexpr std::array sizes{1ULL, 2ULL, 4ULL, 8ULL};
        std::vector<StructFieldRequest> fields;
        fields.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
            fields.push_back(bitField({sizes[index % sizes.size()], sizes[index % sizes.size()]}, 1));
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const auto expected = reference(fields);
        const auto actual = layout.layoutStruct(fields, {true, 0, 0});
        const bool offsetsZero = actual && std::all_of(actual->fields.begin(), actual->fields.end(),
            [](const StructFieldLayout& field) { return field.offset == 0 && field.bitOffset == 0; });
        context.expect(expected && actual && actual->size == 8 && actual->alignment == 1 &&
            actual->fields.size() == count && offsetsZero && same(*expected, *actual),
            "TF-TGT-160 large union bit-field set must remain exact");
        const std::array invalid{bitField({1, 1}, 9)};
        context.expect(!layout.layoutStruct(invalid, {true, 0, 0}), "TF-TGT-160 invalid width must remain rejected");
    });
}
}

void registerTargetUnionBitFieldModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
