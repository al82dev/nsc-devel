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

StructFieldRequest field(TypeLayout layout, std::optional<std::uint64_t> offset = std::nullopt)
{
    return {layout, offset, std::nullopt, false, std::nullopt, false};
}

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-015", "Host.Target", "unit",
        std::move(profile), {"target", "union-layout"}, std::move(function)});
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

std::optional<StructLayout> reference(std::span<const StructFieldRequest> fields, std::uint64_t pack = 0)
{
    if (pack != 0 && (pack & (pack - 1)) != 0) return std::nullopt;
    if (fields.empty()) return StructLayout{1, 1, {}};
    std::uint64_t extent = 0;
    std::uint64_t aggregateAlignment = 1;
    std::vector<StructFieldLayout> output;
    output.reserve(fields.size());
    for (const auto& item : fields)
    {
        if (item.layout.size == 0 || item.layout.alignment == 0 || (item.layout.alignment & (item.layout.alignment - 1)) != 0) return std::nullopt;
        const std::uint64_t effective = pack == 0 ? item.layout.alignment : (std::min)(item.layout.alignment, pack);
        const std::uint64_t offset = item.explicitOffset.value_or(0);
        if (item.layout.size > std::numeric_limits<std::uint64_t>::max() - offset) return std::nullopt;
        const std::uint64_t end = offset + item.layout.size;
        extent = (std::max)(extent, end);
        aggregateAlignment = (std::max)(aggregateAlignment, effective);
        output.push_back({offset, item.layout.size, effective, std::nullopt, std::nullopt});
    }
    const auto size = alignReference(extent, aggregateAlignment);
    if (!size) return std::nullopt;
    return StructLayout{*size, aggregateAlignment, std::move(output)};
}

void expectReference(TestContext& context, const TargetDataLayout& layout,
    std::span<const StructFieldRequest> fields, std::uint64_t pack, std::string message)
{
    const auto expected = reference(fields, pack);
    const auto actual = layout.layoutStruct(fields, StructLayoutOptions{true, pack, 0});
    context.expect(expected && actual && same(*actual, *expected), std::move(message));
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-113", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{field({1, 1})};
        expectReference(context, layout, fields, 0, "TF-TGT-113 minimum union must be exact");
    });
    addCase(registry, "TF-TGT-114", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{field({1, 1}), field({8, 8}), field({3, 2})};
        expectReference(context, layout, fields, 0, "TF-TGT-114 all implicit union fields must start at zero");
    });
    addCase(registry, "TF-TGT-115", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        const std::array fields{field({4, 4}), field({2, 2})};
        expectReference(context, fresh, fields, 0, "TF-TGT-115 fresh union layout must require no external state");
    });
}

void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-116", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array zeroSize{field({0, 1})};
        const std::array zeroAlignment{field({1, 0})};
        const std::array overflow{field({2, 1}, maximum)};
        context.expect(!layout.layoutStruct(zeroSize, {true, 0, 0}), "TF-TGT-116 zero size must fail");
        context.expect(!layout.layoutStruct(zeroAlignment, {true, 0, 0}), "TF-TGT-116 zero alignment must fail");
        context.expect(!layout.layoutStruct(overflow, {true, 0, 0}), "TF-TGT-116 explicit union end overflow must fail");
    });
    addCase(registry, "TF-TGT-117", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> empty{};
        const std::array duplicates{field({2, 2}), field({2, 2})};
        const std::array maximumMember{field({maximum, 1})};
        expectReference(context, layout, empty, 0, "TF-TGT-117 empty union must remain 1/1");
        expectReference(context, layout, duplicates, 0, "TF-TGT-117 duplicate members must share offset zero");
        expectReference(context, layout, maximumMember, 0, "TF-TGT-117 maximum member must remain exact");
    });
}

void registerMatrixAndDeterminismCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-118", "PR", [](TestContext& context) {
        constexpr std::array sizes{1ULL, 3ULL, 5ULL, 17ULL};
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        constexpr std::array packs{0ULL, 1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (const auto pack : packs) for (const auto aSize : sizes) for (const auto aAlignment : alignments)
            for (const auto bSize : sizes) for (const auto bAlignment : alignments)
            {
                const std::array fields{field({aSize, aAlignment}), field({bSize, bAlignment})};
                expectReference(context, layout, fields, pack, "TF-TGT-118 union matrix must match reference");
            }
    });
    addCase(registry, "TF-TGT-119", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{field({3, 2}), field({5, 4}), field({17, 8})};
        for (const auto pack : {0ULL, 1ULL, 2ULL, 4ULL, 8ULL})
        {
            const auto expected = reference(fields, pack);
            const auto first = layout.layoutStruct(fields, {true, pack, 0});
            const auto second = layout.layoutStruct(fields, {true, pack, 0});
            context.expect(expected && first && second && same(*first, *expected) && same(*first, *second), "TF-TGT-119 repetition must preserve exact union layout");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-120", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t count = 65536;
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        constexpr std::array cycle{TypeLayout{1, 1}, TypeLayout{2, 2}, TypeLayout{4, 4}, TypeLayout{8, 8}, TypeLayout{16, 16}};
        std::vector<StructFieldRequest> fields;
        fields.reserve(count);
        for (std::size_t index = 0; index < count; ++index) fields.push_back(field(cycle[index % cycle.size()]));
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const auto actual = layout.layoutStruct(fields, {true, 0, 0});
        const auto expected = reference(fields);
        const bool allOffsetsZero = actual && std::all_of(actual->fields.begin(), actual->fields.end(), [](const StructFieldLayout& item) { return item.offset == 0; });
        context.expect(actual && expected && actual->size == 16 && actual->alignment == 16 && actual->fields.size() == count && allOffsetsZero && same(*actual, *expected), "TF-TGT-120 all union fields must remain at zero");
        const std::array overflow{field({2, 1}, maximum)};
        context.expect(!layout.layoutStruct(overflow, {true, 0, 0}), "TF-TGT-120 union overflow must remain rejected");
    });
}
}

void registerTargetUnionLayoutModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerMatrixAndDeterminismCases(registry);
    registerScaleCase(registry);
}
}
