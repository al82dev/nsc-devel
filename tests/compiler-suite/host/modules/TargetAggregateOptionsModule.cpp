#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Target/TargetDataLayout.h"

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
using nativesharp::target::StructLayoutOptions;
using nativesharp::target::TargetDataLayout;

void addAggregateOptionsCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-011",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "aggregate-options"},
        std::move(function),
    });
}

bool sameEmpty(const StructLayout& left, const StructLayout& right)
{
    return left.size == right.size && left.alignment == right.alignment &&
           left.fields.empty() && right.fields.empty();
}

void expectEmpty(
    TestContext& context,
    const std::optional<StructLayout>& actual,
    std::uint64_t size,
    std::uint64_t alignment,
    std::string message)
{
    context.expect(
        actual && actual->size == size && actual->alignment == alignment &&
            actual->fields.empty(),
        std::move(message));
}

void registerMinimumCase(TestRegistry& registry)
{
    addAggregateOptionsCase(registry, "TF-TGT-081", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> fields{};
        expectEmpty(
            context,
            layout.layoutStruct(fields, StructLayoutOptions{}),
            1,
            1,
            "TF-TGT-081 default empty aggregate must be exactly 1/1");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addAggregateOptionsCase(registry, "TF-TGT-082", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> fields{};
        expectEmpty(
            context,
            layout.layoutStruct(fields, StructLayoutOptions{true, 8, 16}),
            16,
            16,
            "TF-TGT-082 union and pack must preserve the requested empty aggregate alignment");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addAggregateOptionsCase(registry, "TF-TGT-083", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> fields{};
        expectEmpty(
            context,
            fresh.layoutStruct(fields, StructLayoutOptions{false, 2, 4}),
            4,
            4,
            "TF-TGT-083 a fresh layout value must process options without external state");
    });
}

void registerInvalidCase(TestRegistry& registry)
{
    addAggregateOptionsCase(registry, "TF-TGT-084", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> fields{};
        context.expect(
            !layout.layoutStruct(fields, StructLayoutOptions{false, 3, 0}),
            "TF-TGT-084 non-power pack must fail");
        context.expect(
            !layout.layoutStruct(fields, StructLayoutOptions{true, maximum, 0}),
            "TF-TGT-084 maximum odd pack must fail");
        context.expect(
            !layout.layoutStruct(fields, StructLayoutOptions{false, 0, 3}),
            "TF-TGT-084 non-power minimum alignment must fail");
        context.expect(
            !layout.layoutStruct(fields, StructLayoutOptions{true, 0, maximum}),
            "TF-TGT-084 maximum odd minimum alignment must fail");
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addAggregateOptionsCase(registry, "TF-TGT-085", "PR", [](TestContext& context) {
        constexpr std::uint64_t largestPower = std::uint64_t{1} << 63U;
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> fields{};
        expectEmpty(context, layout.layoutStruct(fields, {}), 1, 1, "TF-TGT-085 zero options must remain 1/1");
        expectEmpty(
            context,
            layout.layoutStruct(fields, StructLayoutOptions{true, largestPower, 0}),
            1,
            1,
            "TF-TGT-085 maximum valid pack must not affect an empty aggregate");
        expectEmpty(
            context,
            layout.layoutStruct(fields, StructLayoutOptions{true, largestPower, largestPower}),
            largestPower,
            largestPower,
            "TF-TGT-085 maximum valid minimum alignment must remain exact");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addAggregateOptionsCase(registry, "TF-TGT-086", "PR", [](TestContext& context) {
        constexpr std::uint64_t largestPower = std::uint64_t{1} << 63U;
        constexpr std::array values{0ULL, 1ULL, 2ULL, 4ULL, 8ULL, largestPower};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> fields{};
        std::uint64_t combinations = 0;
        for (const bool isUnion : {false, true})
        {
            for (const std::uint64_t pack : values)
            {
                for (const std::uint64_t minimumAlignment : values)
                {
                    const std::uint64_t expected = minimumAlignment == 0 ? 1 : minimumAlignment;
                    const auto actual = layout.layoutStruct(
                        fields, StructLayoutOptions{isUnion, pack, minimumAlignment});
                    context.expect(
                        actual && actual->size == expected &&
                            actual->alignment == expected && actual->fields.empty(),
                        "TF-TGT-086 every valid option combination must have an exact empty result");
                    ++combinations;
                }
            }
        }
        context.expect(combinations == 72, "TF-TGT-086 must exercise all 72 option combinations");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addAggregateOptionsCase(registry, "TF-TGT-087", "PR", [](TestContext& context) {
        constexpr std::array fixtures{
            StructLayoutOptions{false, 0, 0},
            StructLayoutOptions{true, 0, 0},
            StructLayoutOptions{false, 2, 4},
            StructLayoutOptions{true, 8, 16},
            StructLayoutOptions{false, 16, 2},
            StructLayoutOptions{true, 1, 64},
        };
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> fields{};
        std::array<std::optional<StructLayout>, fixtures.size()> forward{};
        std::array<std::optional<StructLayout>, fixtures.size()> reverse{};
        for (std::size_t index = 0; index < fixtures.size(); ++index)
            forward[index] = layout.layoutStruct(fields, fixtures[index]);
        for (std::size_t reverseIndex = fixtures.size(); reverseIndex > 0; --reverseIndex)
        {
            const std::size_t index = reverseIndex - 1;
            reverse[index] = layout.layoutStruct(fields, fixtures[index]);
        }
        for (std::size_t index = 0; index < fixtures.size(); ++index)
        {
            const auto repeated = layout.layoutStruct(fields, fixtures[index]);
            context.expect(
                forward[index] && reverse[index] && repeated &&
                    sameEmpty(*forward[index], *reverse[index]) &&
                    sameEmpty(*forward[index], *repeated),
                "TF-TGT-087 option queries must be deterministic and invocation-order independent");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addAggregateOptionsCase(registry, "TF-TGT-088", "NIGHTLY", [](TestContext& context) {
        constexpr std::uint64_t iterations = 65536;
        constexpr std::array minimumAlignments{0ULL, 1ULL, 2ULL, 4ULL, 8ULL, 16ULL, 32ULL, 64ULL};
        constexpr std::array packs{0ULL, 1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        const std::array<StructFieldRequest, 0> fields{};
        std::uint64_t checksum = 0;
        bool allExact = true;
        for (std::uint64_t index = 0; index < iterations; ++index)
        {
            const std::uint64_t minimumAlignment =
                minimumAlignments[index % minimumAlignments.size()];
            const std::uint64_t expected = minimumAlignment == 0 ? 1 : minimumAlignment;
            const auto actual = layout.layoutStruct(
                fields,
                StructLayoutOptions{
                    (index & 1U) != 0,
                    packs[index % packs.size()],
                    minimumAlignment});
            allExact = allExact && actual && actual->size == expected &&
                actual->alignment == expected && actual->fields.empty();
            if (actual) checksum += actual->size + actual->alignment;
        }
        context.expect(allExact, "TF-TGT-088 all 65,536 empty layouts must remain exact");
        context.expect(checksum == 2097152, "TF-TGT-088 option checksum must remain exact");
        context.expect(
            !layout.layoutStruct(fields, StructLayoutOptions{false, maximum, 0}),
            "TF-TGT-088 near-maximum invalid option must remain rejected");
    });
}
}

void registerTargetAggregateOptionsModule(TestRegistry& registry)
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
