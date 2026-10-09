#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Language/MemoryModel.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using namespace nativesharp::language;

constexpr std::array kOrders = {
    MemoryOrder::Relaxed,
    MemoryOrder::Acquire,
    MemoryOrder::Release,
    MemoryOrder::AcquireRelease,
    MemoryOrder::SequentiallyConsistent,
};

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CL-LANG-013", "Host.Language", "unit", "PR",
        {"language", "memory-model"}, std::move(function)});
}

void registerDecodeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-162", [](TestContext& context) {
        for (std::size_t index = 0; index < kOrders.size(); ++index)
        {
            const auto decoded = decodeMemoryOrder(index);
            context.expect(decoded.has_value() && *decoded == kOrders[index],
                "TL-LANG-162 ABI values zero through four must decode to their exact memory orders");
        }
        context.expect(!decodeMemoryOrder(5).has_value(),
            "TL-LANG-162 the first value after the public ABI range must be rejected");
        context.expect(!decodeMemoryOrder((std::numeric_limits<std::uint64_t>::max)()).has_value(),
            "TL-LANG-162 the maximum external value must be rejected without truncation");
    });
}

void registerLoadCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-163", [](TestContext& context) {
        constexpr std::array expected = {true, true, false, false, true};
        for (std::size_t index = 0; index < kOrders.size(); ++index)
        {
            context.expect(validMemoryOrder(AtomicOperation::Load, kOrders[index]) == expected[index],
                "TL-LANG-163 loads must reject only release and acquire-release");
        }
    });
}

void registerStoreCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-164", [](TestContext& context) {
        constexpr std::array expected = {true, false, true, false, true};
        for (std::size_t index = 0; index < kOrders.size(); ++index)
        {
            context.expect(validMemoryOrder(AtomicOperation::Store, kOrders[index]) == expected[index],
                "TL-LANG-164 stores must reject only acquire and acquire-release");
        }
    });
}

void registerReadModifyWriteCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-165", [](TestContext& context) {
        for (const MemoryOrder order : kOrders)
        {
            context.expect(validMemoryOrder(AtomicOperation::ReadModifyWrite, order),
                "TL-LANG-165 read-modify-write must accept every public memory order");
        }
    });
}

void registerFenceCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-166", [](TestContext& context) {
        constexpr std::array expected = {false, true, true, true, true};
        for (std::size_t index = 0; index < kOrders.size(); ++index)
        {
            context.expect(validMemoryOrder(AtomicOperation::Fence, kOrders[index]) == expected[index],
                "TL-LANG-166 fences must reject relaxed and accept every ordered variant");
        }
    });
}

void registerCompareExchangeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-167", [](TestContext& context) {
        constexpr std::array<std::array<bool, 5>, 5> expected = {{
            {{true, false, false, false, false}},
            {{true, true, false, false, false}},
            {{true, false, false, false, false}},
            {{true, true, false, false, false}},
            {{true, true, false, false, true}},
        }};
        for (std::size_t success = 0; success < kOrders.size(); ++success)
        {
            for (std::size_t failure = 0; failure < kOrders.size(); ++failure)
            {
                context.expect(validCompareExchangeOrders(kOrders[success], kOrders[failure]) ==
                        expected[success][failure],
                    "TL-LANG-167 the complete success/failure order matrix must remain exact");
            }
        }
    });
}

void registerDataRaceCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-168", [](TestContext& context) {
        context.expect(dataRaceBehavior() == DataRaceBehavior::UndefinedBehavior,
            "TL-LANG-168 a NativeSharp data race must have undefined behavior");
        context.expect(dataRaceBehavior() == DataRaceBehavior::UndefinedBehavior,
            "TL-LANG-168 repeated queries must preserve the same data-race contract");
    });
}

void registerLockFreeCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-169", [](TestContext& context) {
        constexpr std::array<std::uint32_t, 6> widths = {0, 8, 16, 32, 64, 128};
        constexpr std::array expectedAligned = {false, true, true, true, true, false};
        for (std::size_t index = 0; index < widths.size(); ++index)
        {
            context.expect(atomicAccessIsLockFreeWindowsX64(widths[index], true) == expectedAligned[index],
                "TL-LANG-169 aligned accesses must be lock-free only for 8, 16, 32, and 64 bits");
            context.expect(!atomicAccessIsLockFreeWindowsX64(widths[index], false),
                "TL-LANG-169 no misaligned access may be classified as lock-free");
        }
    });
}

void registerVolatileCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-170", [](TestContext& context) {
        context.expect(!volatileProvidesSynchronization(),
            "TL-LANG-170 volatile access must not provide synchronization");
        constexpr std::array<std::uint32_t, 6> widths = {0, 8, 16, 32, 64, 128};
        for (const std::uint32_t width : widths)
        {
            context.expect(volatileAccessMayTear(width, true) == (width > 64U),
                "TL-LANG-170 aligned volatile access may tear exactly when wider than 64 bits");
            context.expect(volatileAccessMayTear(width, false),
                "TL-LANG-170 every misaligned volatile access may tear");
        }
    });
}

void registerManifestCase(TestRegistry& registry)
{
    addCase(registry, "TL-LANG-171", [](TestContext& context) {
        const std::string expected =
            "NSMEMORY 3.0\n"
            "target=x86_64-pc-windows-msvc\n"
            "data-race=undefined-behavior\n"
            "atomic-load=relaxed|acquire|seq-cst\n"
            "atomic-store=relaxed|release|seq-cst\n"
            "atomic-rmw=relaxed|acquire|release|acq-rel|seq-cst\n"
            "atomic-fence=acquire|release|acq-rel|seq-cst\n"
            "compare-exchange=failure-not-stronger-than-success\n"
            "volatile=observable-access-only\n"
            "volatile-synchronization=no\n"
            "volatile-tearing=possible-if-misaligned-or-wider-than-64\n";
        const std::string first = memoryModelManifest();
        const std::string second = memoryModelManifest();
        context.expect(first == expected,
            "TL-LANG-171 complete manifest bytes, ordering, labels, and trailing newline must be exact");
        context.expect(second == first && !first.empty() && first.back() == '\n',
            "TL-LANG-171 repeated manifest calls must be deterministic and newline terminated");
    });
}
}

void registerLanguageMemoryModelModule(TestRegistry& registry)
{
    registerDecodeCase(registry);
    registerLoadCase(registry);
    registerStoreCase(registry);
    registerReadModifyWriteCase(registry);
    registerFenceCase(registry);
    registerCompareExchangeCase(registry);
    registerDataRaceCase(registry);
    registerLockFreeCase(registry);
    registerVolatileCase(registry);
    registerManifestCase(registry);
}
}
