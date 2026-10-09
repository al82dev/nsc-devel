#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Target/TargetDataLayout.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TypeLayout;

void addAlignmentCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-007",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "power-of-two-alignment"},
        std::move(function),
    });
}

std::optional<std::uint64_t> observedAlign(
    const TargetDataLayout& layout,
    std::uint64_t value,
    std::uint64_t alignment)
{
    return layout.strideOf(TypeLayout{value, alignment});
}

std::uint64_t referenceAlign(std::uint64_t value, std::uint64_t alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

void registerMinimumCase(TestRegistry& registry)
{
    addAlignmentCase(registry, "TF-TGT-049", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(observedAlign(layout, 1, 1) == 1, "TF-TGT-049 minimum alignment must preserve one");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addAlignmentCase(registry, "TF-TGT-050", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(observedAlign(layout, 5, 4) == 8, "TF-TGT-050 five aligned to four must be eight");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addAlignmentCase(registry, "TF-TGT-051", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        context.expect(observedAlign(fresh, 3, 2) == 4, "TF-TGT-051 a fresh layout value must align without external state");
    });
}

void registerInvalidCase(TestRegistry& registry)
{
    addAlignmentCase(registry, "TF-TGT-052", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (const std::uint64_t alignment : std::array{0ULL, 3ULL, 6ULL})
        {
            context.expect(!observedAlign(layout, 5, alignment), "TF-TGT-052 zero and non-power-of-two alignments must return no value");
        }
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addAlignmentCase(registry, "TF-TGT-053", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(!observedAlign(layout, 0, 1), "TF-TGT-053 the public stride consumer must reject zero size");
        context.expect(observedAlign(layout, 1, 1) == 1, "TF-TGT-053 one aligned to one must remain one");
        context.expect(observedAlign(layout, maximum, 1) == maximum, "TF-TGT-053 maximum aligned to one must remain maximum");
        context.expect(observedAlign(layout, maximum - 15, 16) == maximum - 15, "TF-TGT-053 aligned maximum-minus-fifteen must remain exact");
        context.expect(!observedAlign(layout, maximum - 14, 16), "TF-TGT-053 mask addition overflow must return no value");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addAlignmentCase(registry, "TF-TGT-054", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL, 16ULL, 32ULL, 64ULL};
        constexpr std::array values{1ULL, 2ULL, 3ULL, 5ULL, 17ULL, 33ULL, 65ULL};
        for (const std::uint64_t alignment : alignments)
        {
            for (const std::uint64_t value : values)
            {
                context.expect(observedAlign(layout, value, alignment) == referenceAlign(value, alignment), "TF-TGT-054 every power-of-two matrix entry must match integer reference alignment");
            }
        }
        constexpr std::uint64_t highAlignment = std::uint64_t{1} << 63U;
        context.expect(observedAlign(layout, 1, highAlignment) == highAlignment, "TF-TGT-054 highest uint64 power of two must remain valid");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addAlignmentCase(registry, "TF-TGT-055", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array values{3ULL, 5ULL, 17ULL, 65ULL, 129ULL};
        constexpr std::array alignments{2ULL, 4ULL, 8ULL, 16ULL, 64ULL};
        std::array<std::optional<std::uint64_t>, values.size()> forward{};
        std::array<std::optional<std::uint64_t>, values.size()> reverse{};
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            forward[index] = observedAlign(layout, values[index], alignments[index]);
        }
        for (std::size_t reverseIndex = values.size(); reverseIndex > 0; --reverseIndex)
        {
            const std::size_t index = reverseIndex - 1;
            reverse[index] = observedAlign(layout, values[index], alignments[index]);
        }
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            context.expect(forward[index] == reverse[index], "TF-TGT-055 query order must not affect alignment");
            context.expect(forward[index] == observedAlign(layout, values[index], alignments[index]), "TF-TGT-055 repeated alignment must be exact");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addAlignmentCase(registry, "TF-TGT-056", "NIGHTLY", [](TestContext& context) {
        constexpr std::uint64_t iterationCount = 65536;
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL, 16ULL, 32ULL, 64ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        std::uint64_t checksum = 0;
        bool allExact = true;
        for (std::uint64_t index = 1; index <= iterationCount; ++index)
        {
            const std::uint64_t value = index * 17;
            const std::uint64_t alignment = alignments[(index - 1) % alignments.size()];
            const auto observed = observedAlign(layout, value, alignment);
            const std::uint64_t expected = referenceAlign(value, alignment);
            allExact = allExact && observed == expected;
            if (observed) checksum += *observed;
        }
        context.expect(allExact, "TF-TGT-056 all 65,536 alignments must match integer reference arithmetic");
        context.expect(checksum == 36508340804ULL, "TF-TGT-056 alignment checksum must remain exact");
    });
}
}

void registerTargetPowerOfTwoAlignmentModule(TestRegistry& registry)
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
