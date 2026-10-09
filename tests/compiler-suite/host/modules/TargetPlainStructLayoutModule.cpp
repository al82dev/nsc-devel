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
using nativesharp::target::StructLayout;
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TypeLayout;

void addPlainStructCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-010",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "plain-struct-layout"},
        std::move(function),
    });
}

bool sameField(const StructFieldLayout& left, const StructFieldLayout& right)
{
    return left.offset == right.offset && left.size == right.size &&
           left.alignment == right.alignment && left.bitOffset == right.bitOffset &&
           left.bitWidth == right.bitWidth;
}

bool sameStruct(const StructLayout& left, const StructLayout& right)
{
    if (left.size != right.size || left.alignment != right.alignment ||
        left.fields.size() != right.fields.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.fields.size(); ++index)
    {
        if (!sameField(left.fields[index], right.fields[index])) return false;
    }
    return true;
}

std::optional<std::uint64_t> referenceAlign(
    std::uint64_t value,
    std::uint64_t alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return std::nullopt;
    const std::uint64_t remainder = value % alignment;
    if (remainder == 0) return value;
    const std::uint64_t padding = alignment - remainder;
    if (padding > std::numeric_limits<std::uint64_t>::max() - value)
        return std::nullopt;
    return value + padding;
}

std::optional<StructLayout> referenceLayout(std::span<const TypeLayout> fields)
{
    if (fields.empty()) return StructLayout{1, 1, {}};

    std::uint64_t cursor = 0;
    std::uint64_t aggregateAlignment = 1;
    std::vector<StructFieldLayout> output;
    output.reserve(fields.size());
    for (const TypeLayout field : fields)
    {
        if (field.size == 0 || field.alignment == 0 ||
            (field.alignment & (field.alignment - 1)) != 0)
        {
            return std::nullopt;
        }
        const auto offset = referenceAlign(cursor, field.alignment);
        if (!offset || field.size > std::numeric_limits<std::uint64_t>::max() - *offset)
            return std::nullopt;
        output.push_back(StructFieldLayout{
            *offset, field.size, field.alignment, std::nullopt, std::nullopt});
        cursor = *offset + field.size;
        aggregateAlignment = (std::max)(aggregateAlignment, field.alignment);
    }
    const auto size = referenceAlign(cursor, aggregateAlignment);
    if (!size) return std::nullopt;
    return StructLayout{*size, aggregateAlignment, std::move(output)};
}

void expectExact(
    TestContext& context,
    const std::optional<StructLayout>& actual,
    const StructLayout& expected,
    std::string message)
{
    context.expect(actual && sameStruct(*actual, expected), std::move(message));
}

void registerMinimumCase(TestRegistry& registry)
{
    addPlainStructCase(registry, "TF-TGT-073", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array fields{TypeLayout{1, 1}};
        expectExact(
            context,
            layout.layoutStruct(fields),
            StructLayout{1, 1, {{0, 1, 1, std::nullopt, std::nullopt}}},
            "TF-TGT-073 minimum plain struct must be exact");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addPlainStructCase(registry, "TF-TGT-074", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array fields{
            TypeLayout{1, 1}, TypeLayout{4, 4}, TypeLayout{2, 2}, TypeLayout{8, 8}};
        expectExact(
            context,
            layout.layoutStruct(fields),
            StructLayout{
                24,
                8,
                {
                    {0, 1, 1, std::nullopt, std::nullopt},
                    {4, 4, 4, std::nullopt, std::nullopt},
                    {8, 2, 2, std::nullopt, std::nullopt},
                    {16, 8, 8, std::nullopt, std::nullopt},
                }},
            "TF-TGT-074 representative plain struct must be exact");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addPlainStructCase(registry, "TF-TGT-075", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        constexpr std::array fields{TypeLayout{2, 2}, TypeLayout{1, 1}};
        expectExact(
            context,
            fresh.layoutStruct(fields),
            StructLayout{
                4,
                2,
                {
                    {0, 2, 2, std::nullopt, std::nullopt},
                    {2, 1, 1, std::nullopt, std::nullopt},
                }},
            "TF-TGT-075 a fresh layout value must create a plain struct without external state");
    });
}

void registerInvalidCase(TestRegistry& registry)
{
    addPlainStructCase(registry, "TF-TGT-076", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array zeroSize{TypeLayout{0, 1}};
        constexpr std::array zeroAlignment{TypeLayout{1, 0}};
        constexpr std::array nonPowerAlignment{TypeLayout{1, 3}};
        constexpr std::array alignmentOverflow{
            TypeLayout{maximum - 7, 8}, TypeLayout{1, 16}};
        constexpr std::array endOverflow{
            TypeLayout{maximum, 1}, TypeLayout{1, 1}};
        context.expect(!layout.layoutStruct(zeroSize), "TF-TGT-076 zero field size must fail");
        context.expect(!layout.layoutStruct(zeroAlignment), "TF-TGT-076 zero field alignment must fail");
        context.expect(!layout.layoutStruct(nonPowerAlignment), "TF-TGT-076 non-power alignment must fail");
        context.expect(!layout.layoutStruct(alignmentOverflow), "TF-TGT-076 aligned offset overflow must fail");
        context.expect(!layout.layoutStruct(endOverflow), "TF-TGT-076 field end overflow must fail");
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addPlainStructCase(registry, "TF-TGT-077", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<TypeLayout, 0> empty{};
        constexpr std::array repeated{TypeLayout{2, 2}, TypeLayout{2, 2}};
        constexpr std::array maximumAlignedOne{TypeLayout{maximum, 1}};
        constexpr std::array nearMaximumAlignedSixteen{TypeLayout{maximum - 15, 16}};
        expectExact(context, layout.layoutStruct(empty), StructLayout{1, 1, {}}, "TF-TGT-077 empty struct must be 1/1");
        expectExact(
            context,
            layout.layoutStruct(repeated),
            StructLayout{
                4,
                2,
                {
                    {0, 2, 2, std::nullopt, std::nullopt},
                    {2, 2, 2, std::nullopt, std::nullopt},
                }},
            "TF-TGT-077 repeated fields must preserve ordered offsets");
        expectExact(
            context,
            layout.layoutStruct(maximumAlignedOne),
            StructLayout{maximum, 1, {{0, maximum, 1, std::nullopt, std::nullopt}}},
            "TF-TGT-077 maximum alignment-one field must remain exact");
        expectExact(
            context,
            layout.layoutStruct(nearMaximumAlignedSixteen),
            StructLayout{
                maximum - 15,
                16,
                {{0, maximum - 15, 16, std::nullopt, std::nullopt}}},
            "TF-TGT-077 near-maximum aligned field must remain exact");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addPlainStructCase(registry, "TF-TGT-078", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array sizes{1ULL, 3ULL, 5ULL, 17ULL};
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        for (const std::uint64_t firstSize : sizes)
        {
            for (const std::uint64_t firstAlignment : alignments)
            {
                for (const std::uint64_t secondSize : sizes)
                {
                    for (const std::uint64_t secondAlignment : alignments)
                    {
                        const std::array fields{
                            TypeLayout{firstSize, firstAlignment},
                            TypeLayout{secondSize, secondAlignment},
                        };
                        const auto expected = referenceLayout(fields);
                        const auto actual = layout.layoutStruct(fields);
                        context.expect(
                            expected && actual && sameStruct(*actual, *expected),
                            "TF-TGT-078 every two-field layout must match the sequential reference");
                    }
                }
            }
        }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addPlainStructCase(registry, "TF-TGT-079", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<std::vector<TypeLayout>, 5> fixtures{
            std::vector<TypeLayout>{{1, 1}},
            std::vector<TypeLayout>{{1, 1}, {4, 4}},
            std::vector<TypeLayout>{{8, 8}, {1, 1}, {2, 2}},
            std::vector<TypeLayout>{{3, 2}, {5, 4}, {17, 8}},
            std::vector<TypeLayout>{{16, 16}, {3, 1}, {9, 8}, {2, 2}},
        };
        std::array<std::optional<StructLayout>, fixtures.size()> forward{};
        std::array<std::optional<StructLayout>, fixtures.size()> reverse{};
        for (std::size_t index = 0; index < fixtures.size(); ++index)
        {
            forward[index] = layout.layoutStruct(fixtures[index]);
        }
        for (std::size_t reverseIndex = fixtures.size(); reverseIndex > 0; --reverseIndex)
        {
            const std::size_t index = reverseIndex - 1;
            reverse[index] = layout.layoutStruct(fixtures[index]);
        }
        for (std::size_t index = 0; index < fixtures.size(); ++index)
        {
            const auto expected = referenceLayout(fixtures[index]);
            const auto repeated = layout.layoutStruct(fixtures[index]);
            context.expect(
                expected && forward[index] && reverse[index] && repeated &&
                    sameStruct(*forward[index], *expected) &&
                    sameStruct(*forward[index], *reverse[index]) &&
                    sameStruct(*forward[index], *repeated),
                "TF-TGT-079 invocation order and repetition must preserve exact struct layout");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addPlainStructCase(registry, "TF-TGT-080", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t fieldCount = 65536;
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        constexpr std::array cycle{
            TypeLayout{1, 1}, TypeLayout{2, 2}, TypeLayout{4, 4},
            TypeLayout{8, 8}, TypeLayout{16, 16}};
        std::vector<TypeLayout> fields;
        fields.reserve(fieldCount);
        for (std::size_t index = 0; index < fieldCount; ++index)
            fields.push_back(cycle[index % cycle.size()]);

        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const auto expected = referenceLayout(fields);
        const auto actual = layout.layoutStruct(fields);
        context.expect(
            expected && actual && sameStruct(*actual, *expected),
            "TF-TGT-080 all 65,536 fields must match the sequential reference");
        context.expect(
            actual && actual->size == 419440 && actual->alignment == 16 &&
                actual->fields.size() == fieldCount &&
                actual->fields.back().offset == 419424,
            "TF-TGT-080 scale result must preserve the exact aggregate summary");

        constexpr std::array nearMaximumOverflow{
            TypeLayout{maximum - 7, 8}, TypeLayout{1, 16}};
        context.expect(
            !layout.layoutStruct(nearMaximumOverflow),
            "TF-TGT-080 near-maximum alignment overflow must remain bounded");
    });
}
}

void registerTargetPlainStructLayoutModule(TestRegistry& registry)
{
    registerMinimumCase(registry);
    registerRepresentativeCase(registry);
    registerNoPrerequisiteCase(registry);
    registerInvalidCase(registry);
    registerBoundaryCase(registry);
    registerVariantMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
