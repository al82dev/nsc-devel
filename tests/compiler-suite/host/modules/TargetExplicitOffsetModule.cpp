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

StructFieldRequest request(TypeLayout layout, std::optional<std::uint64_t> offset = std::nullopt,
    std::optional<std::uint64_t> minimum = std::nullopt)
{
    return {layout, offset, minimum, false, std::nullopt, false};
}

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-014", "Host.Target", "unit",
        std::move(profile), {"target", "explicit-offset"}, std::move(function)});
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

std::optional<StructLayout> reference(std::span<const StructFieldRequest> fields)
{
    if (fields.empty()) return StructLayout{1, 1, {}};
    std::uint64_t cursor = 0;
    std::uint64_t extent = 0;
    std::uint64_t aggregateAlignment = 1;
    std::vector<StructFieldLayout> output;
    output.reserve(fields.size());
    for (const auto& item : fields)
    {
        if (item.layout.size == 0 || item.layout.alignment == 0 || (item.layout.alignment & (item.layout.alignment - 1)) != 0) return std::nullopt;
        std::uint64_t effective = item.layout.alignment;
        if (item.minimumAlignment)
        {
            const std::uint64_t minimum = *item.minimumAlignment;
            if (minimum == 0 || (minimum & (minimum - 1)) != 0) return std::nullopt;
            effective = (std::max)(effective, minimum);
        }
        std::uint64_t offset = 0;
        if (item.explicitOffset)
        {
            offset = *item.explicitOffset;
            if (item.minimumAlignment && offset % effective != 0) return std::nullopt;
        }
        else
        {
            const auto aligned = alignReference(cursor, effective);
            if (!aligned) return std::nullopt;
            offset = *aligned;
        }
        if (item.layout.size > std::numeric_limits<std::uint64_t>::max() - offset) return std::nullopt;
        const std::uint64_t end = offset + item.layout.size;
        output.push_back({offset, item.layout.size, effective, std::nullopt, std::nullopt});
        cursor = end;
        extent = (std::max)(extent, end);
        aggregateAlignment = (std::max)(aggregateAlignment, effective);
    }
    const auto size = alignReference(extent, aggregateAlignment);
    if (!size) return std::nullopt;
    return StructLayout{*size, aggregateAlignment, std::move(output)};
}

void expectReference(TestContext& context, const TargetDataLayout& layout,
    std::span<const StructFieldRequest> fields, std::string message)
{
    const auto expected = reference(fields);
    const auto actual = layout.layoutStruct(fields, StructLayoutOptions{});
    context.expect(expected && actual && same(*actual, *expected), std::move(message));
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-105", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{request({4, 4}, 1)};
        expectReference(context, layout, fields, "TF-TGT-105 misaligned natural offset without override must be accepted");
    });
    addCase(registry, "TF-TGT-106", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{request({4, 4}, 8), request({8, 8}, 4), request({1, 1})};
        expectReference(context, layout, fields, "TF-TGT-106 explicit overlap and following implicit field must be exact");
    });
    addCase(registry, "TF-TGT-107", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        const std::array fields{request({2, 2}, 3)};
        expectReference(context, fresh, fields, "TF-TGT-107 fresh value must require no external state");
    });
}

void registerInvalidCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-108", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array misaligned{request({4, 4}, 2, 4)};
        const std::array overflow{request({2, 1}, maximum)};
        context.expect(!layout.layoutStruct(misaligned, {}), "TF-TGT-108 explicit minimum alignment must reject misalignment");
        context.expect(!layout.layoutStruct(overflow, {}), "TF-TGT-108 explicit field end overflow must fail");
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-109", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array zero{request({1, 1}, 0)};
        const std::array duplicates{request({2, 1}, 0), request({3, 1}, 0)};
        const std::array maximumEnd{request({1, 1}, maximum - 1)};
        expectReference(context, layout, zero, "TF-TGT-109 zero explicit offset must be accepted");
        expectReference(context, layout, duplicates, "TF-TGT-109 duplicate offsets must overlap");
        expectReference(context, layout, maximumEnd, "TF-TGT-109 maximum representable end must remain exact");
    });
}

void registerMatrixAndDeterminismCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-110", "PR", [](TestContext& context) {
        constexpr std::array sizes{1ULL, 3ULL, 5ULL, 9ULL};
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL};
        constexpr std::array offsets{0ULL, 1ULL, 2ULL, 7ULL, 16ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (const auto size : sizes) for (const auto alignment : alignments) for (const auto offset : offsets)
        {
            const std::array fields{request({size, alignment}, offset)};
            expectReference(context, layout, fields, "TF-TGT-110 every explicit offset must match the extent reference");
        }
    });
    addCase(registry, "TF-TGT-111", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{request({3, 2}, 8), request({5, 4}, 2), request({1, 1})};
        const auto expected = reference(fields);
        const auto first = layout.layoutStruct(fields, {});
        const auto second = layout.layoutStruct(fields, {});
        context.expect(expected && first && second && same(*first, *expected) && same(*first, *second), "TF-TGT-111 repetition must preserve exact offsets and extent");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-112", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t count = 65536;
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        std::vector<StructFieldRequest> fields;
        fields.reserve(count);
        for (std::size_t index = 0; index < count; ++index) fields.push_back(request({1, 1}, index % 1024));
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const auto actual = layout.layoutStruct(fields, {});
        const auto expected = reference(fields);
        context.expect(actual && expected && actual->size == 1024 && actual->alignment == 1 && actual->fields.size() == count && same(*actual, *expected), "TF-TGT-112 all overlapping fields must match reference");
        const std::array overflow{request({2, 1}, maximum)};
        context.expect(!layout.layoutStruct(overflow, {}), "TF-TGT-112 field end overflow must remain rejected");
    });
}
}

void registerTargetExplicitOffsetModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidCase(registry);
    registerBoundaryCase(registry);
    registerMatrixAndDeterminismCases(registry);
    registerScaleCase(registry);
}
}
