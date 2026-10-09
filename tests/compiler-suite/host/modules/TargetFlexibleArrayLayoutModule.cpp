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

StructFieldRequest plain(TypeLayout layout)
{
    return {layout, std::nullopt, std::nullopt, false, std::nullopt, false};
}

StructFieldRequest flexible(TypeLayout layout, std::optional<std::uint64_t> offset = std::nullopt)
{
    return {layout, offset, std::nullopt, true, std::nullopt, false};
}

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-016", "Host.Target", "unit",
        std::move(profile), {"target", "flexible-array-layout"}, std::move(function)});
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
    std::span<const StructFieldRequest> fields, const StructLayoutOptions& options = {})
{
    if (options.pack != 0 && (options.pack & (options.pack - 1)) != 0) return std::nullopt;
    if (options.minimumAlignment != 0 &&
        (options.minimumAlignment & (options.minimumAlignment - 1)) != 0)
        return std::nullopt;
    if (fields.empty())
    {
        const std::uint64_t alignment = options.minimumAlignment == 0 ? 1 : options.minimumAlignment;
        const auto size = alignReference(1, alignment);
        if (!size) return std::nullopt;
        return StructLayout{*size, alignment, {}};
    }

    std::uint64_t currentOffset = 0;
    std::uint64_t extent = 0;
    std::uint64_t aggregateAlignment = 1;
    std::vector<StructFieldLayout> output;
    output.reserve(fields.size());
    for (const auto& item : fields)
    {
        if ((!item.isFlexibleArray && item.layout.size == 0) || item.layout.alignment == 0 ||
            (item.layout.alignment & (item.layout.alignment - 1)) != 0 ||
            (item.isFlexibleArray && item.bitWidth.has_value()))
            return std::nullopt;
        const std::uint64_t effective = options.pack == 0
            ? item.layout.alignment
            : (std::min)(item.layout.alignment, options.pack);
        std::uint64_t offset = 0;
        if (item.explicitOffset)
            offset = *item.explicitOffset;
        else
        {
            const auto aligned = alignReference(currentOffset, effective);
            if (!aligned) return std::nullopt;
            offset = *aligned;
        }
        if (item.layout.size > std::numeric_limits<std::uint64_t>::max() - offset)
            return std::nullopt;
        const std::uint64_t end = offset + item.layout.size;
        currentOffset = end;
        extent = (std::max)(extent, end);
        aggregateAlignment = (std::max)(aggregateAlignment, effective);
        output.push_back({offset, item.layout.size, effective, std::nullopt, std::nullopt});
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
    context.expect(expected && actual && same(*actual, *expected), std::move(message));
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-121", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{flexible({0, 1})};
        expectReference(context, layout, fields, {}, "TF-TGT-121 minimum flexible array must have zero extent");
    });
    addCase(registry, "TF-TGT-122", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array zeroMiddle{plain({4, 4}), flexible({0, 8}), plain({2, 2})};
        const std::array specifiedMiddle{plain({1, 1}), flexible({5, 4}), plain({2, 2})};
        expectReference(context, layout, zeroMiddle, {}, "TF-TGT-122 zero-extent flexible field may precede another field");
        expectReference(context, layout, specifiedMiddle, {}, "TF-TGT-122 specified-extent flexible field must advance the cursor");
    });
    addCase(registry, "TF-TGT-123", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        const std::array fields{flexible({0, 4}), plain({1, 1})};
        expectReference(context, fresh, fields, {}, "TF-TGT-123 fresh layout must require no external prerequisite");
    });
}

void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-124", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array zeroAlignment{flexible({0, 0})};
        const std::array nonPowerAlignment{flexible({0, 3})};
        const std::array flexibleBitField{StructFieldRequest{{4, 4}, std::nullopt, std::nullopt, true, 1, false}};
        context.expect(!layout.layoutStruct(zeroAlignment, {}), "TF-TGT-124 zero alignment must fail");
        context.expect(!layout.layoutStruct(nonPowerAlignment, {}), "TF-TGT-124 non-power alignment must fail");
        context.expect(!layout.layoutStruct(flexibleBitField, {}), "TF-TGT-124 flexible bit-field must fail");
    });
    addCase(registry, "TF-TGT-125", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> empty{};
        const std::array duplicates{flexible({0, 2}), flexible({0, 2})};
        const std::array specified{flexible({1, 1})};
        const std::array maximumOffset{flexible({0, 1}, maximum)};
        expectReference(context, layout, empty, {}, "TF-TGT-125 empty aggregate must remain 1/1");
        expectReference(context, layout, duplicates, {}, "TF-TGT-125 duplicate zero-extent fields must be exact");
        expectReference(context, layout, specified, {}, "TF-TGT-125 specified extent one must be exact");
        expectReference(context, layout, maximumOffset, {}, "TF-TGT-125 maximum zero-extent offset must remain representable");
    });
}

void registerMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-126", "PR", [](TestContext& context) {
        constexpr std::array sizes{0ULL, 1ULL, 5ULL};
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL};
        constexpr std::array packs{0ULL, 1ULL, 2ULL, 4ULL, 8ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (std::size_t position = 0; position < 3; ++position)
            for (const auto size : sizes)
                for (const auto alignment : alignments)
                    for (const auto pack : packs)
                    {
                        std::vector<StructFieldRequest> fields{plain({3, 2}), plain({5, 4})};
                        fields.insert(fields.begin() + static_cast<std::ptrdiff_t>(position), flexible({size, alignment}));
                        expectReference(context, layout, fields, {false, pack, 0}, "TF-TGT-126 position and option matrix must match reference");
                    }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-127", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fixtures{
            std::vector<StructFieldRequest>{flexible({0, 1}), plain({1, 1})},
            std::vector<StructFieldRequest>{plain({3, 2}), flexible({0, 8}), plain({5, 4})},
            std::vector<StructFieldRequest>{plain({1, 1}), flexible({5, 4})},
            std::vector<StructFieldRequest>{flexible({2, 2}), flexible({0, 16}), plain({7, 4})},
            std::vector<StructFieldRequest>{plain({8, 8}), flexible({0, 2}), plain({1, 1})}};
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
                same(*forward[index], *expected) && same(*forward[index], *reverse[index]) &&
                same(*forward[index], *repeated),
                "TF-TGT-127 invocation order and repetition must preserve exact layout");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-128", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t count = 65536;
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        std::vector<StructFieldRequest> fields;
        fields.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            if (index % 3 == 0) fields.push_back(flexible({0, 16}));
            else if (index % 3 == 1) fields.push_back(plain({1, 1}));
            else fields.push_back(flexible({3, 2}));
        }
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const auto expected = reference(fields);
        const auto actual = layout.layoutStruct(fields, {});
        context.expect(expected && actual && actual->fields.size() == count && same(*actual, *expected),
            "TF-TGT-128 large mixed flexible layout must match reference");
        const std::array overflow{flexible({2, 1}, maximum)};
        context.expect(!layout.layoutStruct(overflow, {}), "TF-TGT-128 flexible extent overflow must remain rejected");
    });
}
}

void registerTargetFlexibleArrayLayoutModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
