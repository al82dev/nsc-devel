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

void addElementStrideCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-008",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "element-stride"},
        std::move(function),
    });
}

std::uint64_t referenceStride(std::uint64_t size, std::uint64_t alignment)
{
    return ((size + alignment - 1) / alignment) * alignment;
}

void registerMinimumCase(TestRegistry& registry)
{
    addElementStrideCase(registry, "TF-TGT-057", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(layout.strideOf({1, 1}) == 1, "TF-TGT-057 minimum element stride must be one");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addElementStrideCase(registry, "TF-TGT-058", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(layout.strideOf({5, 4}) == 8, "TF-TGT-058 representative element stride must round to eight");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addElementStrideCase(registry, "TF-TGT-059", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        context.expect(fresh.strideOf({7, 8}) == 8, "TF-TGT-059 a fresh layout value must calculate stride without external state");
    });
}

void registerInvalidCase(TestRegistry& registry)
{
    addElementStrideCase(registry, "TF-TGT-060", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(!layout.strideOf({0, 1}), "TF-TGT-060 zero size must return no stride");
        context.expect(!layout.strideOf({1, 0}), "TF-TGT-060 zero alignment must return no stride");
        context.expect(!layout.strideOf({5, 3}), "TF-TGT-060 non-power-of-two alignment must return no stride");
        context.expect(!layout.strideOf({maximum - 14, 16}), "TF-TGT-060 alignment overflow must return no stride");
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addElementStrideCase(registry, "TF-TGT-061", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        constexpr std::uint64_t highAlignment = std::uint64_t{1} << 63U;
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(layout.strideOf({1, 1}) == 1, "TF-TGT-061 minimum stride must remain one");
        context.expect(layout.strideOf({maximum, 1}) == maximum, "TF-TGT-061 maximum stride aligned to one must remain maximum");
        context.expect(layout.strideOf({maximum - 15, 16}) == maximum - 15, "TF-TGT-061 aligned maximum-minus-fifteen must remain exact");
        context.expect(layout.strideOf({1, highAlignment}) == highAlignment, "TF-TGT-061 highest power-of-two alignment must remain valid");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addElementStrideCase(registry, "TF-TGT-062", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array sizes{1ULL, 2ULL, 3ULL, 5ULL, 17ULL, 33ULL, 65ULL};
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL, 16ULL, 32ULL, 64ULL};
        for (const std::uint64_t size : sizes)
        {
            for (const std::uint64_t alignment : alignments)
            {
                context.expect(layout.strideOf({size, alignment}) == referenceStride(size, alignment), "TF-TGT-062 every element layout must match reference stride");
            }
        }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addElementStrideCase(registry, "TF-TGT-063", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array elements{
            TypeLayout{3, 2}, TypeLayout{5, 4}, TypeLayout{17, 8},
            TypeLayout{33, 16}, TypeLayout{65, 64},
        };
        std::array<std::optional<std::uint64_t>, elements.size()> forward{};
        std::array<std::optional<std::uint64_t>, elements.size()> reverse{};
        for (std::size_t index = 0; index < elements.size(); ++index)
        {
            forward[index] = layout.strideOf(elements[index]);
        }
        for (std::size_t reverseIndex = elements.size(); reverseIndex > 0; --reverseIndex)
        {
            const std::size_t index = reverseIndex - 1;
            reverse[index] = layout.strideOf(elements[index]);
        }
        for (std::size_t index = 0; index < elements.size(); ++index)
        {
            context.expect(forward[index] == reverse[index], "TF-TGT-063 query order must not affect stride");
            context.expect(forward[index] == layout.strideOf(elements[index]), "TF-TGT-063 repeated stride must be exact");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addElementStrideCase(registry, "TF-TGT-064", "NIGHTLY", [](TestContext& context) {
        constexpr std::uint64_t iterationCount = 65536;
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL, 16ULL, 32ULL, 64ULL, 128ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        std::uint64_t checksum = 0;
        bool allExact = true;
        for (std::uint64_t index = 0; index < iterationCount; ++index)
        {
            const std::uint64_t size = (index * 29) + 1;
            const std::uint64_t alignment = alignments[index % alignments.size()];
            const auto observed = layout.strideOf({size, alignment});
            const std::uint64_t expected = referenceStride(size, alignment);
            allExact = allExact && observed == expected;
            if (observed) checksum += *observed;
        }
        context.expect(allExact, "TF-TGT-064 all 65,536 element strides must match the reference");
        context.expect(checksum == 62277115904ULL, "TF-TGT-064 element-stride checksum must remain exact");
    });
}
}

void registerTargetElementStrideModule(TestRegistry& registry)
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
