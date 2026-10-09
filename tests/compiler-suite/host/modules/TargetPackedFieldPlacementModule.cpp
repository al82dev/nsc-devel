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
using namespace nativesharp::target;

StructFieldRequest field(TypeLayout layout)
{
    return {layout, std::nullopt, std::nullopt, false, std::nullopt, false};
}

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-012", "Host.Target", "unit",
        std::move(profile), {"target", "packed-field-placement"}, std::move(function)});
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

std::optional<StructLayout> reference(std::span<const StructFieldRequest> fields, std::uint64_t pack)
{
    if (pack != 0 && (pack & (pack - 1)) != 0) return std::nullopt;
    if (fields.empty()) return StructLayout{1, 1, {}};
    std::uint64_t cursor = 0;
    std::uint64_t aggregateAlignment = 1;
    std::vector<StructFieldLayout> output;
    output.reserve(fields.size());
    for (const auto& request : fields)
    {
        const TypeLayout value = request.layout;
        if (value.size == 0 || value.alignment == 0 || (value.alignment & (value.alignment - 1)) != 0) return std::nullopt;
        const std::uint64_t effective = pack == 0 ? value.alignment : (std::min)(value.alignment, pack);
        const auto offset = alignReference(cursor, effective);
        if (!offset || value.size > std::numeric_limits<std::uint64_t>::max() - *offset) return std::nullopt;
        output.push_back({*offset, value.size, effective, std::nullopt, std::nullopt});
        cursor = *offset + value.size;
        aggregateAlignment = (std::max)(aggregateAlignment, effective);
    }
    const auto size = alignReference(cursor, aggregateAlignment);
    if (!size) return std::nullopt;
    return StructLayout{*size, aggregateAlignment, std::move(output)};
}

void expectReference(TestContext& context, const TargetDataLayout& layout,
    std::span<const StructFieldRequest> fields, std::uint64_t pack, std::string message)
{
    const auto expected = reference(fields, pack);
    const auto actual = layout.layoutStruct(fields, StructLayoutOptions{false, pack, 0});
    context.expect(expected && actual && same(*actual, *expected), std::move(message));
}

void registerCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-089", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{field({8, 8})};
        expectReference(context, layout, fields, 1, "TF-TGT-089 minimum packed field must be exact");
    });
    addCase(registry, "TF-TGT-090", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{field({1, 1}), field({8, 8}), field({2, 2})};
        expectReference(context, layout, fields, 2, "TF-TGT-090 representative packed fields must be exact");
    });
    addCase(registry, "TF-TGT-091", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        const std::array fields{field({4, 4}), field({8, 8})};
        expectReference(context, fresh, fields, 4, "TF-TGT-091 fresh layout must require no external state");
    });
    addCase(registry, "TF-TGT-092", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array zeroSize{field({0, 1})};
        const std::array zeroAlignment{field({1, 0})};
        const std::array alignmentOverflow{field({maximum - 7, 8}), field({1, 16})};
        const std::array endOverflow{field({maximum, 1}), field({1, 1})};
        context.expect(!layout.layoutStruct(zeroSize, {false, 1, 0}), "TF-TGT-092 zero size must fail");
        context.expect(!layout.layoutStruct(zeroAlignment, {false, 1, 0}), "TF-TGT-092 zero alignment must fail");
        context.expect(!layout.layoutStruct(alignmentOverflow, {false, 16, 0}), "TF-TGT-092 offset overflow must fail");
        context.expect(!layout.layoutStruct(endOverflow, {false, 1, 0}), "TF-TGT-092 end overflow must fail");
    });
    addCase(registry, "TF-TGT-093", "PR", [](TestContext& context) {
        constexpr std::uint64_t largestPower = std::uint64_t{1} << 63U;
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{field({1, 1}), field({8, 8}), field({2, 2})};
        expectReference(context, layout, fields, 0, "TF-TGT-093 natural packing must be exact");
        expectReference(context, layout, fields, 1, "TF-TGT-093 byte packing must be exact");
        expectReference(context, layout, fields, largestPower, "TF-TGT-093 largest pack must equal natural layout");
    });
    addCase(registry, "TF-TGT-094", "PR", [](TestContext& context) {
        constexpr std::array sizes{1ULL, 3ULL, 5ULL, 17ULL};
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        constexpr std::array packs{0ULL, 1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (const auto pack : packs) for (const auto aSize : sizes) for (const auto aAlign : alignments)
            for (const auto bSize : sizes) for (const auto bAlign : alignments)
            {
                const std::array fields{field({aSize, aAlign}), field({bSize, bAlign})};
                expectReference(context, layout, fields, pack, "TF-TGT-094 packed matrix must match the reference");
            }
    });
    addCase(registry, "TF-TGT-095", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{field({3, 2}), field({5, 4}), field({17, 8})};
        for (const auto pack : {0ULL, 1ULL, 2ULL, 4ULL, 8ULL})
        {
            const auto first = layout.layoutStruct(fields, {false, pack, 0});
            const auto second = layout.layoutStruct(fields, {false, pack, 0});
            const auto expected = reference(fields, pack);
            context.expect(first && second && expected && same(*first, *second) && same(*first, *expected), "TF-TGT-095 repetition must preserve exact packed layout");
        }
    });
    addCase(registry, "TF-TGT-096", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t count = 65536;
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        constexpr std::array cycle{TypeLayout{1, 1}, TypeLayout{2, 2}, TypeLayout{4, 4}, TypeLayout{8, 8}, TypeLayout{16, 16}};
        std::vector<StructFieldRequest> fields;
        fields.reserve(count);
        for (std::size_t index = 0; index < count; ++index) fields.push_back(field(cycle[index % cycle.size()]));
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const auto actual = layout.layoutStruct(fields, {false, 1, 0});
        context.expect(actual && actual->size == 406318 && actual->alignment == 1 && actual->fields.size() == count && actual->fields.back().offset == 406317, "TF-TGT-096 scale summary must be exact");
        const std::array overflow{field({maximum, 1}), field({1, 1})};
        context.expect(!layout.layoutStruct(overflow, {false, 1, 0}), "TF-TGT-096 near-maximum overflow must fail");
    });
}
}

void registerTargetPackedFieldPlacementModule(TestRegistry& registry)
{
    registerCases(registry);
}
}
