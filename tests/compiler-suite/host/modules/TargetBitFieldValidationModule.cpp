#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Target/TargetDataLayout.h"

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
using nativesharp::target::StructFieldRequest;
using nativesharp::target::StructLayout;
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TypeLayout;

StructFieldRequest bitField(TypeLayout layout, std::uint64_t width, bool unnamed = false)
{
    return {layout, std::nullopt, std::nullopt, false, width, unnamed};
}

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-017", "Host.Target", "unit",
        std::move(profile), {"target", "bit-field-validation"}, std::move(function)});
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

bool acceptedByTruthTable(const StructFieldRequest& request)
{
    if (request.layout.size == 0 || request.layout.alignment == 0 ||
        request.explicitOffset || request.minimumAlignment || request.isFlexibleArray ||
        !request.bitWidth)
        return false;
    const std::uint64_t width = request.bitWidth.value_or(0);
    if (request.layout.size > std::numeric_limits<std::uint64_t>::max() / 8U)
        return false;
    if (width > request.layout.size * 8U || (width == 0 && !request.isUnnamedBitField))
        return false;
    if (width == 0) return true;
    return (request.layout.alignment & (request.layout.alignment - 1)) == 0;
}

std::optional<StructLayout> exactSingleFieldOracle(const StructFieldRequest& request)
{
    if (!acceptedByTruthTable(request)) return std::nullopt;
    const std::uint64_t width = request.bitWidth.value_or(0);
    if (width == 0)
        return StructLayout{0, 1, {{0, 0, request.layout.alignment, 0, 0}}};
    const std::uint64_t remainder = request.layout.size % request.layout.alignment;
    const std::uint64_t padding = remainder == 0 ? 0 : request.layout.alignment - remainder;
    if (padding > std::numeric_limits<std::uint64_t>::max() - request.layout.size)
        return std::nullopt;
    return StructLayout{request.layout.size + padding, request.layout.alignment,
        {{0, request.layout.size, request.layout.alignment, 0, width}}};
}

void expectSingle(TestContext& context, const TargetDataLayout& layout,
    const StructFieldRequest& request, std::string message)
{
    const std::array fields{request};
    const auto expected = exactSingleFieldOracle(request);
    const auto actual = layout.layoutStruct(fields, {});
    context.expect(expected.has_value() == actual.has_value() &&
        (!expected || same(*actual, *expected)), std::move(message));
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-129", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        expectSingle(context, layout, bitField({1, 1}, 1), "TF-TGT-129 minimum bit-field must be exact");
    });
    addCase(registry, "TF-TGT-130", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        expectSingle(context, layout, bitField({2, 2}, 16), "TF-TGT-130 16-bit allocation unit must be accepted");
        expectSingle(context, layout, bitField({4, 4}, 31), "TF-TGT-130 partial 32-bit allocation unit must be accepted");
        expectSingle(context, layout, bitField({8, 8}, 64), "TF-TGT-130 full 64-bit allocation unit must be accepted");
    });
    addCase(registry, "TF-TGT-131", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        expectSingle(context, fresh, bitField({4, 4}, 7), "TF-TGT-131 fresh layout must require no external prerequisite");
    });
}

void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-132", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array explicitOffset{StructFieldRequest{{4, 4}, 0, std::nullopt, false, 1, false}};
        const std::array minimumAlignment{StructFieldRequest{{4, 4}, std::nullopt, 4, false, 1, false}};
        const std::array flexibleArray{StructFieldRequest{{4, 4}, std::nullopt, std::nullopt, true, 1, false}};
        const std::array zeroAlignment{bitField({4, 0}, 1)};
        context.expect(!layout.layoutStruct(explicitOffset, {}), "TF-TGT-132 explicit offset must fail");
        context.expect(!layout.layoutStruct(minimumAlignment, {}), "TF-TGT-132 field minimum must fail");
        context.expect(!layout.layoutStruct(flexibleArray, {}), "TF-TGT-132 flexible bit-field must fail");
        context.expect(!layout.layoutStruct(zeroAlignment, {}), "TF-TGT-132 zero alignment must fail");
    });
    addCase(registry, "TF-TGT-133", "PR", [](TestContext& context) {
        constexpr std::uint64_t overflowSize = std::numeric_limits<std::uint64_t>::max() / 8U + 1U;
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        expectSingle(context, layout, bitField({4, 4}, 32), "TF-TGT-133 exact unit width must be accepted");
        expectSingle(context, layout, bitField({4, 4}, 0, true), "TF-TGT-133 unnamed zero width must be accepted");
        expectSingle(context, layout, bitField({4, 4}, 0), "TF-TGT-133 named zero width must be rejected");
        expectSingle(context, layout, bitField({4, 4}, 33), "TF-TGT-133 width above unit must be rejected");
        expectSingle(context, layout, bitField({overflowSize, 1}, 1), "TF-TGT-133 unit-bit conversion overflow must be rejected");
    });
}

void registerMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-134", "PR", [](TestContext& context) {
        constexpr std::array sizes{1ULL, 2ULL, 4ULL, 8ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (const auto size : sizes)
        {
            const std::array widths{0ULL, 1ULL, size * 8U, size * 8U + 1U};
            for (const auto width : widths)
                for (unsigned flags = 0; flags < 16; ++flags)
                {
                    StructFieldRequest request{{size, size}, std::nullopt, std::nullopt,
                        (flags & 4U) != 0, width, (flags & 8U) != 0};
                    if ((flags & 1U) != 0) request.explicitOffset = 0;
                    if ((flags & 2U) != 0) request.minimumAlignment = size;
                    expectSingle(context, layout, request, "TF-TGT-134 request must match validation truth table");
                }
        }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-135", "PR", [](TestContext& context) {
        constexpr std::uint64_t overflowSize = std::numeric_limits<std::uint64_t>::max() / 8U + 1U;
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array fixtures{
            bitField({1, 1}, 1), bitField({2, 2}, 0, true), bitField({4, 4}, 32),
            bitField({4, 4}, 33), bitField({4, 4}, 0), bitField({4, 0}, 1),
            bitField({overflowSize, 1}, 1), bitField({8, 8}, 63)};
        std::array<std::optional<StructLayout>, fixtures.size()> forward{};
        std::array<std::optional<StructLayout>, fixtures.size()> reverse{};
        for (std::size_t index = 0; index < fixtures.size(); ++index)
        {
            const std::array field{fixtures[index]};
            forward[index] = layout.layoutStruct(field, {});
        }
        for (std::size_t index = fixtures.size(); index-- > 0;)
        {
            const std::array field{fixtures[index]};
            reverse[index] = layout.layoutStruct(field, {});
        }
        for (std::size_t index = 0; index < fixtures.size(); ++index)
        {
            const std::array field{fixtures[index]};
            const auto repeated = layout.layoutStruct(field, {});
            const auto expected = exactSingleFieldOracle(fixtures[index]);
            const bool equal = expected.has_value() == forward[index].has_value() &&
                forward[index].has_value() == reverse[index].has_value() &&
                forward[index].has_value() == repeated.has_value() &&
                (!expected || (same(*expected, *forward[index]) &&
                    same(*forward[index], *reverse[index]) && same(*forward[index], *repeated)));
            context.expect(equal, "TF-TGT-135 order and repetition must preserve validation result");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-136", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t count = 65536;
        constexpr std::array sizes{1ULL, 2ULL, 4ULL, 8ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (std::size_t index = 0; index < count; ++index)
        {
            const std::uint64_t size = sizes[index % sizes.size()];
            const std::array widths{0ULL, 1ULL, size * 8U, size * 8U + 1U};
            const std::uint64_t width = widths[(index / sizes.size()) % widths.size()];
            const auto request = bitField({size, size}, width, (index & 1U) != 0);
            const std::array field{request};
            const bool expected = acceptedByTruthTable(request);
            const bool actual = layout.layoutStruct(field, {}).has_value();
            context.expect(actual == expected, "TF-TGT-136 large validation matrix must match truth table");
        }
        constexpr std::uint64_t overflowSize = std::numeric_limits<std::uint64_t>::max() / 8U + 1U;
        expectSingle(context, layout, bitField({overflowSize, 1}, 1), "TF-TGT-136 overflow unit must remain rejected");
    });
}
}

void registerTargetBitFieldValidationModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
