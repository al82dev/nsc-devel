#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::target::StructFieldRequest;
using nativesharp::target::StructLayout;
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TypeLayout;

StructFieldRequest field(TypeLayout layout)
{
    return {layout, std::nullopt, std::nullopt, false, std::nullopt, false};
}

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-021", "Host.Target", "unit",
        std::move(profile), {"target", "aggregate-finalization"}, std::move(function)});
}

std::optional<std::uint64_t> roundUp(std::uint64_t extent, std::uint64_t alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return std::nullopt;
    const std::uint64_t remainder = extent % alignment;
    const std::uint64_t padding = remainder == 0 ? 0 : alignment - remainder;
    if (padding > std::numeric_limits<std::uint64_t>::max() - extent) return std::nullopt;
    return extent + padding;
}

void expectSingle(TestContext& context, const TargetDataLayout& layout,
    std::uint64_t size, std::uint64_t alignment, std::uint64_t minimumAlignment, std::string message)
{
    const std::array fields{field({size, alignment})};
    const std::uint64_t finalAlignment = (std::max)(alignment, minimumAlignment);
    const auto expectedSize = roundUp(size, finalAlignment);
    const auto actual = layout.layoutStruct(fields, {false, 0, minimumAlignment});
    context.expect(expectedSize.has_value() == actual.has_value() &&
        (!expectedSize || (actual->size == *expectedSize && actual->alignment == finalAlignment)),
        std::move(message));
}

void expectProperties(TestContext& context, const TargetDataLayout& layout, std::string prefix)
{
    context.expect(layout.maximumNaturalAlignment() == 8, prefix + " maximum natural alignment must be eight");
    context.expect(layout.isLittleEndian(), prefix + " target must be little-endian");
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-161", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        expectSingle(context, layout, 3, 2, 0, "TF-TGT-161 final padding must round three to four");
        expectProperties(context, layout, "TF-TGT-161");
    });
    addCase(registry, "TF-TGT-162", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{field({1, 1}), field({3, 2}), field({5, 4})};
        const auto actual = layout.layoutStruct(fields, {false, 0, 16});
        context.expect(actual && actual->size == 16 && actual->alignment == 16,
            "TF-TGT-162 representative aggregate must finalize at 16/16");
        expectProperties(context, layout, "TF-TGT-162");
    });
    addCase(registry, "TF-TGT-163", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        expectSingle(context, fresh, 5, 4, 0, "TF-TGT-163 fresh value must finalize without external state");
        expectProperties(context, fresh, "TF-TGT-163");
    });
}

void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-164", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fields{field({maximum, 16})};
        context.expect(!layout.layoutStruct(fields, {}), "TF-TGT-164 final padding overflow must fail");
    });
    addCase(registry, "TF-TGT-165", "PR", [](TestContext& context) {
        constexpr std::uint64_t hugeAlignment = std::uint64_t{1} << 63U;
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> empty{};
        const auto hugeEmpty = layout.layoutStruct(empty, {false, 0, hugeAlignment});
        context.expect(hugeEmpty && hugeEmpty->size == hugeAlignment && hugeEmpty->alignment == hugeAlignment,
            "TF-TGT-165 empty aggregate must honor largest power-of-two minimum");
        expectSingle(context, layout, 16, 16, 0, "TF-TGT-165 exact extent must need no padding");
        expectProperties(context, layout, "TF-TGT-165");
    });
}

void registerMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-166", "PR", [](TestContext& context) {
        constexpr std::array sizes{1ULL, 3ULL, 5ULL, 17ULL};
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        constexpr std::array minimumAlignments{0ULL, 1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (const auto size : sizes)
            for (const auto alignment : alignments)
                for (const auto minimumAlignment : minimumAlignments)
                    expectSingle(context, layout, size, alignment, minimumAlignment,
                        "TF-TGT-166 finalization matrix must match round-up reference");
        expectProperties(context, layout, "TF-TGT-166");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-167", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array fixtures{TypeLayout{1, 1}, TypeLayout{3, 2}, TypeLayout{5, 4},
            TypeLayout{17, 8}, TypeLayout{31, 16}};
        std::array<std::optional<StructLayout>, fixtures.size()> forward{};
        std::array<std::optional<StructLayout>, fixtures.size()> reverse{};
        for (std::size_t index = 0; index < fixtures.size(); ++index)
        {
            const std::array fields{field(fixtures[index])};
            forward[index] = layout.layoutStruct(fields, {});
        }
        for (std::size_t index = fixtures.size(); index-- > 0;)
        {
            const std::array fields{field(fixtures[index])};
            reverse[index] = layout.layoutStruct(fields, {});
        }
        for (std::size_t index = 0; index < fixtures.size(); ++index)
        {
            const std::array fields{field(fixtures[index])};
            const auto repeated = layout.layoutStruct(fields, {});
            context.expect(forward[index] && reverse[index] && repeated &&
                forward[index]->size == reverse[index]->size &&
                forward[index]->size == repeated->size &&
                forward[index]->alignment == reverse[index]->alignment &&
                forward[index]->alignment == repeated->alignment,
                "TF-TGT-167 order and repetition must preserve scalar finalization");
        }
        expectProperties(context, layout, "TF-TGT-167");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-168", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t count = 65536;
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        std::vector<StructFieldRequest> fields(count, field({1, 1}));
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const auto actual = layout.layoutStruct(fields, {});
        context.expect(actual && actual->size == count && actual->alignment == 1 &&
            actual->fields.size() == count, "TF-TGT-168 large aggregate must finalize exactly");
        const std::array overflow{field({maximum, 16})};
        context.expect(!layout.layoutStruct(overflow, {}), "TF-TGT-168 final padding overflow must remain rejected");
        expectProperties(context, layout, "TF-TGT-168");
    });
}
}

void registerTargetAggregateFinalizationModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
