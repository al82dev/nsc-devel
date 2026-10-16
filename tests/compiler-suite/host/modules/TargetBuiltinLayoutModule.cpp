#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Target/TargetDataLayout.h"

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::target::BuiltinLayoutKind;
using nativesharp::target::TargetDataLayout;
using nativesharp::target::TypeLayout;

constexpr std::array kBuiltinKinds{
    BuiltinLayoutKind::Bool,
    BuiltinLayoutKind::SByte,
    BuiltinLayoutKind::Byte,
    BuiltinLayoutKind::Char,
    BuiltinLayoutKind::Short,
    BuiltinLayoutKind::UShort,
    BuiltinLayoutKind::Int,
    BuiltinLayoutKind::UInt,
    BuiltinLayoutKind::Long,
    BuiltinLayoutKind::ULong,
    BuiltinLayoutKind::NInt,
    BuiltinLayoutKind::NUInt,
    BuiltinLayoutKind::Int128,
    BuiltinLayoutKind::UInt128,
    BuiltinLayoutKind::Float,
    BuiltinLayoutKind::Double,
    BuiltinLayoutKind::Enum,
    BuiltinLayoutKind::Pointer,
    BuiltinLayoutKind::Decimal,
};

constexpr std::array kExpectedLayouts{
    TypeLayout{1, 1}, TypeLayout{1, 1}, TypeLayout{1, 1}, TypeLayout{1, 1},
    TypeLayout{2, 2}, TypeLayout{2, 2}, TypeLayout{4, 4}, TypeLayout{4, 4},
    TypeLayout{8, 8}, TypeLayout{8, 8}, TypeLayout{8, 8}, TypeLayout{8, 8},
    TypeLayout{16, 16}, TypeLayout{16, 16}, TypeLayout{4, 4}, TypeLayout{8, 8},
    TypeLayout{4, 4}, TypeLayout{8, 8}, TypeLayout{16, 8},
};

bool sameLayout(TypeLayout left, TypeLayout right)
{
    return left.size == right.size && left.alignment == right.alignment;
}

BuiltinLayoutKind unknownKind(std::underlying_type_t<BuiltinLayoutKind> value)
{
    return std::bit_cast<BuiltinLayoutKind>(value);
}

void addBuiltinLayoutCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-005",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "builtin-layout"},
        std::move(function),
    });
}

void registerMinimumCase(TestRegistry& registry)
{
    addBuiltinLayoutCase(registry, "TF-TGT-033", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(sameLayout(layout.layoutOf(BuiltinLayoutKind::Bool), {1, 1}), "TF-TGT-033 Bool must have layout 1/1");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addBuiltinLayoutCase(registry, "TF-TGT-034", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(sameLayout(layout.layoutOf(BuiltinLayoutKind::Int), {4, 4}), "TF-TGT-034 Int must have layout 4/4");
        context.expect(sameLayout(layout.layoutOf(BuiltinLayoutKind::Pointer), {8, 8}), "TF-TGT-034 Pointer must have layout 8/8");
        context.expect(sameLayout(layout.layoutOf(BuiltinLayoutKind::Int128), {16, 16}), "TF-TGT-034 Int128 must have layout 16/16");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addBuiltinLayoutCase(registry, "TF-TGT-035", "PR", [](TestContext& context) {
        const TargetDataLayout fresh = TargetDataLayout::WindowsX64();
        context.expect(sameLayout(fresh.layoutOf(BuiltinLayoutKind::Byte), {1, 1}), "TF-TGT-035 a fresh layout value must answer without external state");
    });
}

void registerInvalidCase(TestRegistry& registry)
{
    addBuiltinLayoutCase(registry, "TF-TGT-036", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (const int value : std::array{-1, 19, 255})
        {
            context.expect(sameLayout(layout.layoutOf(unknownKind(value)), {0, 1}), "TF-TGT-036 unknown enum values must use the documented 0/1 fallback");
        }
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addBuiltinLayoutCase(registry, "TF-TGT-037", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        context.expect(sameLayout(layout.layoutOf(kBuiltinKinds.front()), kExpectedLayouts.front()), "TF-TGT-037 first known kind must be exact");
        context.expect(sameLayout(layout.layoutOf(kBuiltinKinds.back()), kExpectedLayouts.back()), "TF-TGT-037 last known kind must be exact");
        context.expect(sameLayout(layout.layoutOf(unknownKind(std::numeric_limits<int>::min())), {0, 1}), "TF-TGT-037 minimum underlying value must use fallback");
        context.expect(sameLayout(layout.layoutOf(unknownKind(std::numeric_limits<int>::max())), {0, 1}), "TF-TGT-037 maximum underlying value must use fallback");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addBuiltinLayoutCase(registry, "TF-TGT-038", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        for (std::size_t index = 0; index < kBuiltinKinds.size(); ++index)
        {
            context.expect(sameLayout(layout.layoutOf(kBuiltinKinds[index]), kExpectedLayouts[index]), "TF-TGT-038 every known builtin kind must match the exact table");
        }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addBuiltinLayoutCase(registry, "TF-TGT-039", "PR", [](TestContext& context) {
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        std::array<TypeLayout, kBuiltinKinds.size()> forward{};
        std::array<TypeLayout, kBuiltinKinds.size()> reverse{};
        for (std::size_t index = 0; index < kBuiltinKinds.size(); ++index)
        {
            forward[index] = layout.layoutOf(kBuiltinKinds[index]);
        }
        for (std::size_t reverseIndex = kBuiltinKinds.size(); reverseIndex > 0; --reverseIndex)
        {
            const std::size_t index = reverseIndex - 1;
            reverse[index] = layout.layoutOf(kBuiltinKinds[index]);
        }
        for (std::size_t index = 0; index < kBuiltinKinds.size(); ++index)
        {
            context.expect(sameLayout(forward[index], reverse[index]), "TF-TGT-039 query order must not affect results");
            context.expect(sameLayout(forward[index], layout.layoutOf(kBuiltinKinds[index])), "TF-TGT-039 repeated queries must be exact");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addBuiltinLayoutCase(registry, "TF-TGT-040", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        const TargetDataLayout layout = TargetDataLayout::WindowsX64();
        std::uint64_t checksum = 0;
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            const TypeLayout result = layout.layoutOf(kBuiltinKinds[index % kBuiltinKinds.size()]);
            checksum += (result.size * 31U) + result.alignment;
        }
        context.expect(checksum == 13216760ULL, "TF-TGT-040 scale checksum must remain exact across 65,536 queries");
    });
}
}

void registerTargetBuiltinLayoutModule(TestRegistry& registry)
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
