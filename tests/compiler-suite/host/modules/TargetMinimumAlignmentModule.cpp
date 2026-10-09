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

StructFieldRequest request(TypeLayout layout, std::optional<std::uint64_t> minimum = std::nullopt,
    std::optional<std::uint64_t> offset = std::nullopt)
{
    return {layout, offset, minimum, false, std::nullopt, false};
}

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-013", "Host.Target", "unit",
        std::move(profile), {"target", "minimum-alignment"}, std::move(function)});
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

std::optional<StructLayout> reference(std::span<const StructFieldRequest> fields,
    std::uint64_t pack, std::uint64_t aggregateMinimum)
{
    if ((pack != 0 && (pack & (pack - 1)) != 0) ||
        (aggregateMinimum != 0 && (aggregateMinimum & (aggregateMinimum - 1)) != 0)) return std::nullopt;
    if (fields.empty())
    {
        const std::uint64_t alignment = aggregateMinimum == 0 ? 1 : aggregateMinimum;
        return StructLayout{alignment, alignment, {}};
    }
    std::uint64_t cursor = 0;
    std::uint64_t aggregateAlignment = 1;
    std::vector<StructFieldLayout> output;
    output.reserve(fields.size());
    for (const auto& item : fields)
    {
        if (item.layout.size == 0 || item.layout.alignment == 0 || (item.layout.alignment & (item.layout.alignment - 1)) != 0) return std::nullopt;
        std::uint64_t effective = pack == 0 ? item.layout.alignment : (std::min)(item.layout.alignment, pack);
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
        output.push_back({offset, item.layout.size, effective, std::nullopt, std::nullopt});
        cursor = offset + item.layout.size;
        aggregateAlignment = (std::max)(aggregateAlignment, effective);
    }
    aggregateAlignment = (std::max)(aggregateAlignment, aggregateMinimum);
    const auto size = alignReference(cursor, aggregateAlignment);
    if (!size) return std::nullopt;
    return StructLayout{*size, aggregateAlignment, std::move(output)};
}

void expectReference(TestContext& context, const TargetDataLayout& layout,
    std::span<const StructFieldRequest> fields, StructLayoutOptions options, std::string message)
{
    const auto expected = reference(fields, options.pack, options.minimumAlignment);
    const auto actual = layout.layoutStruct(fields, options);
    context.expect(expected && actual && same(*actual, *expected), std::move(message));
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-097", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{request({1, 1}, 2)};
        expectReference(context, layout, fields, {}, "TF-TGT-097 minimum field alignment must raise layout");
    });
    addCase(registry, "TF-TGT-098", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{request({1, 1}), request({8, 8}, 8)};
        expectReference(context, layout, fields, {false, 2, 16}, "TF-TGT-098 field and aggregate minimums must compose");
    });
    addCase(registry, "TF-TGT-099", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        const std::array fields{request({2, 2}, 4)};
        expectReference(context, fresh, fields, {false, 0, 8}, "TF-TGT-099 fresh value must require no external state");
    });
}

void registerInvalidCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-100", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array zero{request({1, 1}, 0)};
        const std::array three{request({1, 1}, 3)};
        const std::array maximumOdd{request({1, 1}, maximum)};
        const std::array misaligned{request({4, 4}, 4, 2)};
        const std::array overflow{request({maximum - 7, 8}), request({1, 1}, 16)};
        context.expect(!layout.layoutStruct(zero, {}), "TF-TGT-100 zero field minimum must fail");
        context.expect(!layout.layoutStruct(three, {}), "TF-TGT-100 non-power field minimum must fail");
        context.expect(!layout.layoutStruct(maximumOdd, {}), "TF-TGT-100 maximum odd field minimum must fail");
        context.expect(!layout.layoutStruct(misaligned, {}), "TF-TGT-100 explicit offset misalignment must fail");
        context.expect(!layout.layoutStruct(overflow, {}), "TF-TGT-100 raised alignment overflow must fail");
    });
}

void registerBoundaryAndMatrixCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-101", "PR", [](TestContext& context) {
        constexpr std::uint64_t largestPower = std::uint64_t{1} << 63U;
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array ordinary{request({1, 1})};
        const std::array raised{request({1, 1}, largestPower)};
        expectReference(context, layout, ordinary, {false, 0, largestPower}, "TF-TGT-101 aggregate maximum power must remain exact");
        expectReference(context, layout, raised, {}, "TF-TGT-101 field maximum power must remain exact");
    });
    addCase(registry, "TF-TGT-102", "PR", [](TestContext& context) {
        constexpr std::array packs{0ULL, 1ULL, 2ULL, 4ULL, 8ULL};
        constexpr std::array fieldMinimums{1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        constexpr std::array aggregateMinimums{0ULL, 1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (const auto pack : packs) for (const auto fieldMinimum : fieldMinimums)
            for (const auto aggregateMinimum : aggregateMinimums)
            {
                const std::array fields{request({3, 2}), request({5, 8}, fieldMinimum)};
                expectReference(context, layout, fields, {false, pack, aggregateMinimum}, "TF-TGT-102 alignment matrix must match reference");
            }
    });
}

void registerDeterminismAndScaleCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-103", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{request({3, 2}, 4), request({5, 8}, 16)};
        for (const auto pack : {0ULL, 1ULL, 2ULL, 4ULL, 8ULL})
        {
            const StructLayoutOptions options{false, pack, 32};
            const auto first = layout.layoutStruct(fields, options);
            const auto second = layout.layoutStruct(fields, options);
            const auto expected = reference(fields, pack, 32);
            context.expect(first && second && expected && same(*first, *second) && same(*first, *expected), "TF-TGT-103 repeated alignment layout must be exact");
        }
    });
    addCase(registry, "TF-TGT-104", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t count = 65536;
        constexpr std::array minimums{1ULL, 2ULL, 4ULL, 8ULL, 16ULL, 32ULL, 64ULL};
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        std::vector<StructFieldRequest> fields;
        fields.reserve(count);
        for (std::size_t index = 0; index < count; ++index) fields.push_back(request({1, 1}, minimums[index % minimums.size()]));
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const StructLayoutOptions options{false, 1, 64};
        const auto actual = layout.layoutStruct(fields, options);
        const auto expected = reference(fields, 1, 64);
        context.expect(actual && expected && actual->fields.size() == count && same(*actual, *expected), "TF-TGT-104 all 65,536 raised fields must match reference");
        const std::array overflow{request({maximum - 7, 8}), request({1, 1}, 16)};
        context.expect(!layout.layoutStruct(overflow, {}), "TF-TGT-104 near-maximum raised alignment must fail");
    });
}
}

void registerTargetMinimumAlignmentModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidCase(registry);
    registerBoundaryAndMatrixCases(registry);
    registerDeterminismAndScaleCases(registry);
}
}
