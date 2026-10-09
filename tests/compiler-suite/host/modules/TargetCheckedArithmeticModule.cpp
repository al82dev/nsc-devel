#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Target/TargetDataLayout.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::target::StructFieldRequest;
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TypeLayout;

void addCheckedArithmeticCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-006",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "checked-arithmetic"},
        std::move(function),
    });
}

std::optional<std::uint64_t> observedAdd(
    const TargetDataLayout& layout,
    std::uint64_t left,
    std::uint64_t right)
{
    const std::array fields{StructFieldRequest{{right, 1}, left, std::nullopt, right == 0, std::nullopt, false}};
    const auto result = layout.layoutStruct(fields, {});
    if (!result) return std::nullopt;
    return result->size;
}

std::optional<std::uint64_t> observedMultiply(
    const TargetDataLayout& layout,
    std::uint64_t left,
    std::uint64_t right)
{
    const auto result = layout.layoutArray(TypeLayout{left, 1}, right);
    if (!result) return std::nullopt;
    return result->size;
}

void registerMinimumCase(TestRegistry& registry)
{
    addCheckedArithmeticCase(registry, "TF-TGT-041", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(observedAdd(layout, 0, 1) == 1, "TF-TGT-041 checked add minimum must equal one");
        context.expect(observedMultiply(layout, 1, 1) == 1, "TF-TGT-041 checked multiply minimum must equal one");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addCheckedArithmeticCase(registry, "TF-TGT-042", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(observedAdd(layout, 32, 10) == 42, "TF-TGT-042 representative checked add must equal 42");
        context.expect(observedMultiply(layout, 6, 7) == 42, "TF-TGT-042 representative checked multiply must equal 42");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addCheckedArithmeticCase(registry, "TF-TGT-043", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        context.expect(observedAdd(fresh, 7, 5) == 12, "TF-TGT-043 checked add must need no external state");
        context.expect(observedMultiply(fresh, 7, 5) == 35, "TF-TGT-043 checked multiply must need no external state");
    });
}

void registerInvalidCase(TestRegistry& registry)
{
    addCheckedArithmeticCase(registry, "TF-TGT-044", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(!observedAdd(layout, maximum, 2), "TF-TGT-044 overflowing checked add must return no value");
        context.expect(!observedMultiply(layout, maximum, 2), "TF-TGT-044 overflowing checked multiply must return no value");
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addCheckedArithmeticCase(registry, "TF-TGT-045", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(observedAdd(layout, maximum, 0) == maximum, "TF-TGT-045 maximum plus zero must remain exact");
        context.expect(observedAdd(layout, 0, maximum) == maximum, "TF-TGT-045 zero plus maximum must remain exact");
        context.expect(observedMultiply(layout, maximum, 1) == maximum, "TF-TGT-045 maximum times one must remain exact");
        context.expect(observedMultiply(layout, maximum / 2, 2) == maximum - 1, "TF-TGT-045 near-maximum product must remain exact");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addCheckedArithmeticCase(registry, "TF-TGT-046", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array left{1ULL, 2ULL, 17ULL, 1024ULL, 4294967295ULL};
        constexpr std::array right{1ULL, 3ULL, 19ULL, 4096ULL, 65537ULL};
        for (std::size_t index = 0; index < left.size(); ++index)
        {
            context.expect(observedAdd(layout, left[index], right[index]) == left[index] + right[index], "TF-TGT-046 checked add matrix must match reference arithmetic");
            context.expect(observedMultiply(layout, left[index], right[index]) == left[index] * right[index], "TF-TGT-046 checked multiply matrix must match reference arithmetic");
        }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCheckedArithmeticCase(registry, "TF-TGT-047", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        constexpr std::array values{1ULL, 7ULL, 31ULL, 255ULL, 4095ULL};
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            const std::uint64_t left = values[index];
            const std::uint64_t right = values[values.size() - index - 1];
            context.expect(observedAdd(layout, left, right) == observedAdd(layout, right, left), "TF-TGT-047 checked add must be order-independent");
            context.expect(observedMultiply(layout, left, right) == observedMultiply(layout, right, left), "TF-TGT-047 checked multiply must be order-independent");
            context.expect(observedAdd(layout, left, right) == observedAdd(layout, left, right), "TF-TGT-047 repeated checked add must be exact");
            context.expect(observedMultiply(layout, left, right) == observedMultiply(layout, left, right), "TF-TGT-047 repeated checked multiply must be exact");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCheckedArithmeticCase(registry, "TF-TGT-048", "NIGHTLY", [](TestContext& context) {
        constexpr std::uint64_t iterationCount = 65536;
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        std::uint64_t checksum = 0;
        bool acceptedExact = true;
        bool overflowRejected = true;
        for (std::uint64_t index = 1; index <= iterationCount; ++index)
        {
            const auto product = observedMultiply(layout, index, 17);
            const auto sum = observedAdd(layout, index * 3, index * 5);
            acceptedExact = acceptedExact && product == index * 17 && sum == index * 8;
            if (product) checksum += *product;
            if (sum) checksum += *sum;
            overflowRejected = overflowRejected &&
                !observedMultiply(layout, maximum - index + 1, 2) &&
                !observedAdd(layout, maximum - index, index + 1);
        }
        context.expect(acceptedExact, "TF-TGT-048 all accepted scale operations must match reference arithmetic");
        context.expect(overflowRejected, "TF-TGT-048 all overflowing scale operations must return no value");
        context.expect(checksum == 53687910400ULL, "TF-TGT-048 accepted-operation checksum must remain exact");
    });
}
}

void registerTargetCheckedArithmeticModule(TestRegistry& registry)
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
