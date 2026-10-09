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
using nativesharp::target::ArrayLayout;
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TypeLayout;

void addArrayLayoutCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-009",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "array-layout"},
        std::move(function),
    });
}

bool sameArray(const ArrayLayout& left, const ArrayLayout& right)
{
    return left.size == right.size && left.alignment == right.alignment &&
           left.stride == right.stride && left.count == right.count;
}

std::uint64_t referenceStride(std::uint64_t size, std::uint64_t alignment)
{
    return ((size + alignment - 1) / alignment) * alignment;
}

void expectArray(
    TestContext& context,
    const std::optional<ArrayLayout>& value,
    std::uint64_t size,
    std::uint64_t alignment,
    std::uint64_t stride,
    std::uint64_t count,
    std::string message)
{
    context.expect(
        value && value->size == size && value->alignment == alignment &&
            value->stride == stride && value->count == count,
        std::move(message));
}

void registerMinimumCase(TestRegistry& registry)
{
    addArrayLayoutCase(registry, "TF-TGT-065", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        expectArray(context, layout.layoutArray({1, 1}, 1), 1, 1, 1, 1, "TF-TGT-065 minimum array layout must be exact");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addArrayLayoutCase(registry, "TF-TGT-066", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        expectArray(context, layout.layoutArray({5, 4}, 3), 24, 4, 8, 3, "TF-TGT-066 representative array layout must be exact");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addArrayLayoutCase(registry, "TF-TGT-067", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        expectArray(context, fresh.layoutArray({7, 8}, 2), 16, 8, 8, 2, "TF-TGT-067 a fresh layout value must create arrays without external state");
    });
}

void registerInvalidCase(TestRegistry& registry)
{
    addArrayLayoutCase(registry, "TF-TGT-068", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(!layout.layoutArray({1, 1}, 0), "TF-TGT-068 zero count must return no layout");
        context.expect(!layout.layoutArray({0, 1}, 1), "TF-TGT-068 zero element size must return no layout");
        context.expect(!layout.layoutArray({1, 0}, 1), "TF-TGT-068 zero element alignment must return no layout");
        context.expect(!layout.layoutArray({5, 3}, 1), "TF-TGT-068 non-power-of-two alignment must return no layout");
        context.expect(!layout.layoutArray({maximum, 1}, 2), "TF-TGT-068 array size overflow must return no layout");
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addArrayLayoutCase(registry, "TF-TGT-069", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        expectArray(context, layout.layoutArray({1, 1}, maximum), maximum, 1, 1, maximum, "TF-TGT-069 maximum count of byte elements must remain exact");
        expectArray(context, layout.layoutArray({maximum, 1}, 1), maximum, 1, maximum, 1, "TF-TGT-069 maximum-size single element must remain exact");
        expectArray(context, layout.layoutArray({maximum - 15, 16}, 1), maximum - 15, 16, maximum - 15, 1, "TF-TGT-069 aligned near-maximum element must remain exact");
        context.expect(!layout.layoutArray({maximum, 1}, 2), "TF-TGT-069 maximum size times two must overflow");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addArrayLayoutCase(registry, "TF-TGT-070", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array sizes{1ULL, 3ULL, 5ULL, 17ULL};
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL, 16ULL};
        constexpr std::array counts{1ULL, 2ULL, 3ULL, 17ULL};
        for (const std::uint64_t size : sizes)
        {
            for (const std::uint64_t alignment : alignments)
            {
                const std::uint64_t stride = referenceStride(size, alignment);
                for (const std::uint64_t count : counts)
                {
                    expectArray(context, layout.layoutArray({size, alignment}, count), stride * count, alignment, stride, count, "TF-TGT-070 every array field must match the independent reference");
                }
            }
        }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addArrayLayoutCase(registry, "TF-TGT-071", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array elements{
            TypeLayout{3, 2}, TypeLayout{5, 4}, TypeLayout{17, 8},
            TypeLayout{33, 16}, TypeLayout{65, 64},
        };
        std::array<std::optional<ArrayLayout>, elements.size()> forward{};
        std::array<std::optional<ArrayLayout>, elements.size()> reverse{};
        for (std::size_t index = 0; index < elements.size(); ++index)
        {
            forward[index] = layout.layoutArray(elements[index], index + 1);
        }
        for (std::size_t reverseIndex = elements.size(); reverseIndex > 0; --reverseIndex)
        {
            const std::size_t index = reverseIndex - 1;
            reverse[index] = layout.layoutArray(elements[index], index + 1);
        }
        for (std::size_t index = 0; index < elements.size(); ++index)
        {
            context.expect(forward[index] && reverse[index] && sameArray(*forward[index], *reverse[index]), "TF-TGT-071 query order must not affect array layout");
            const auto repeated = layout.layoutArray(elements[index], index + 1);
            context.expect(forward[index] && repeated && sameArray(*forward[index], *repeated), "TF-TGT-071 repeated array layout must be exact");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addArrayLayoutCase(registry, "TF-TGT-072", "NIGHTLY", [](TestContext& context) {
        constexpr std::uint64_t iterationCount = 65536;
        constexpr std::array alignments{1ULL, 2ULL, 4ULL, 8ULL, 16ULL, 32ULL, 64ULL};
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        std::uint64_t checksum = 0;
        bool allExact = true;
        for (std::uint64_t index = 0; index < iterationCount; ++index)
        {
            const std::uint64_t size = (index % 127) + 1;
            const std::uint64_t alignment = alignments[index % alignments.size()];
            const std::uint64_t count = (index % 31) + 1;
            const std::uint64_t stride = referenceStride(size, alignment);
            const auto observed = layout.layoutArray({size, alignment}, count);
            allExact = allExact && observed && observed->size == stride * count &&
                observed->alignment == alignment && observed->stride == stride &&
                observed->count == count;
            if (observed) checksum += observed->size + observed->alignment + observed->stride + observed->count;
        }
        context.expect(allExact, "TF-TGT-072 all 65,536 arrays must match the independent reference");
        context.expect(checksum == 83156681ULL, "TF-TGT-072 array-layout checksum must remain exact");
    });
}
}

void registerTargetArrayLayoutModule(TestRegistry& registry)
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
