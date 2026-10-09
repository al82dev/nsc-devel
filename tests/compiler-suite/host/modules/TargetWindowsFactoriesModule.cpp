#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Target/TargetDataLayout.h"

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::target::ArrayLayout;
using nativesharp::target::BuiltinLayoutKind;
using nativesharp::target::StructFieldLayout;
using nativesharp::target::StructFieldRequest;
using nativesharp::target::StructLayout;
using nativesharp::target::StructLayoutOptions;
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
};

constexpr std::array kExpectedLayouts{
    TypeLayout{1, 1}, TypeLayout{1, 1}, TypeLayout{1, 1}, TypeLayout{1, 1},
    TypeLayout{2, 2}, TypeLayout{2, 2}, TypeLayout{4, 4}, TypeLayout{4, 4},
    TypeLayout{8, 8}, TypeLayout{8, 8}, TypeLayout{8, 8}, TypeLayout{8, 8},
    TypeLayout{16, 16}, TypeLayout{16, 16}, TypeLayout{4, 4}, TypeLayout{8, 8},
    TypeLayout{4, 4}, TypeLayout{8, 8},
};

bool sameTypeLayout(TypeLayout left, TypeLayout right)
{
    return left.size == right.size && left.alignment == right.alignment;
}

bool sameArrayLayout(const ArrayLayout& left, const ArrayLayout& right)
{
    return left.size == right.size && left.alignment == right.alignment &&
           left.stride == right.stride && left.count == right.count;
}

bool sameFieldLayout(const StructFieldLayout& left, const StructFieldLayout& right)
{
    return left.offset == right.offset && left.size == right.size &&
           left.alignment == right.alignment && left.bitOffset == right.bitOffset &&
           left.bitWidth == right.bitWidth;
}

bool sameStructLayout(const StructLayout& left, const StructLayout& right)
{
    if (left.size != right.size || left.alignment != right.alignment ||
        left.fields.size() != right.fields.size())
        return false;
    for (std::size_t index = 0; index < left.fields.size(); ++index)
    {
        if (!sameFieldLayout(left.fields[index], right.fields[index])) return false;
    }
    return true;
}

void addWindowsFactoryCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-004",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "windows-factory"},
        std::move(function),
    });
}

void registerMinimumCase(TestRegistry& registry)
{
    addWindowsFactoryCase(registry, "TF-TGT-025", "PR", [](TestContext& context) {
        const TargetDataLayout x64 = TargetDataLayout::WindowsX64();
        const TargetDataLayout arm64 = TargetDataLayout::WindowsArm64();
        context.expect(x64.isLittleEndian() && arm64.isLittleEndian(), "TF-TGT-025 both Windows factories must be little-endian");
        context.expect(x64.maximumNaturalAlignment() == 8 && arm64.maximumNaturalAlignment() == 8, "TF-TGT-025 both Windows factories must publish maximum natural alignment 8");
        context.expect(sameTypeLayout(x64.layoutOf(BuiltinLayoutKind::Bool), {1, 1}) && sameTypeLayout(arm64.layoutOf(BuiltinLayoutKind::Bool), {1, 1}), "TF-TGT-025 minimum scalar layout must be exact");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addWindowsFactoryCase(registry, "TF-TGT-026", "PR", [](TestContext& context) {
        const TargetDataLayout x64 = TargetDataLayout::WindowsX64();
        const TargetDataLayout arm64 = TargetDataLayout::WindowsArm64();
        const TypeLayout pointerX64 = x64.layoutOf(BuiltinLayoutKind::Pointer);
        const TypeLayout pointerArm64 = arm64.layoutOf(BuiltinLayoutKind::Pointer);
        const TypeLayout wideX64 = x64.layoutOf(BuiltinLayoutKind::Int128);
        const TypeLayout wideArm64 = arm64.layoutOf(BuiltinLayoutKind::Int128);
        context.expect(sameTypeLayout(pointerX64, {8, 8}) && sameTypeLayout(pointerX64, pointerArm64), "TF-TGT-026 pointer layout must be LLP64-identical");
        context.expect(sameTypeLayout(wideX64, {16, 16}) && sameTypeLayout(wideX64, wideArm64), "TF-TGT-026 Int128 layout must remain exact");
        const auto arrayX64 = x64.layoutArray({4, 4}, 3);
        const auto arrayArm64 = arm64.layoutArray({4, 4}, 3);
        context.expect(arrayX64 && arrayArm64 && sameArrayLayout(*arrayX64, *arrayArm64) && arrayX64->size == 12, "TF-TGT-026 representative arrays must match");
        const std::array fields{TypeLayout{1, 1}, TypeLayout{8, 8}};
        const auto structX64 = x64.layoutStruct(fields);
        const auto structArm64 = arm64.layoutStruct(fields);
        context.expect(structX64 && structArm64 && sameStructLayout(*structX64, *structArm64) && structX64->size == 16, "TF-TGT-026 representative structs must match");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addWindowsFactoryCase(registry, "TF-TGT-027", "PR", [](TestContext& context) {
        const TargetDataLayout x64 = TargetDataLayout::WindowsX64();
        const TargetDataLayout arm64 = TargetDataLayout::WindowsArm64();
        const std::span<const TypeLayout> empty;
        const auto structX64 = x64.layoutStruct(empty);
        const auto structArm64 = arm64.layoutStruct(empty);
        context.expect(structX64 && structArm64, "TF-TGT-027 factories must be usable without external state");
        context.expect(structX64 && structArm64 && sameStructLayout(*structX64, *structArm64), "TF-TGT-027 empty struct results must match");
        context.expect(structX64 && structX64->size == 1 && structX64->alignment == 1 && structX64->fields.empty(), "TF-TGT-027 empty struct contract must be exact");
    });
}

void registerInvalidCase(TestRegistry& registry)
{
    addWindowsFactoryCase(registry, "TF-TGT-028", "PR", [](TestContext& context) {
        const TargetDataLayout x64 = TargetDataLayout::WindowsX64();
        const TargetDataLayout arm64 = TargetDataLayout::WindowsArm64();
        context.expect(!x64.strideOf({0, 1}) && !arm64.strideOf({0, 1}), "TF-TGT-028 zero-size stride must be rejected identically");
        context.expect(!x64.layoutArray({1, 1}, 0) && !arm64.layoutArray({1, 1}, 0), "TF-TGT-028 zero-count arrays must be rejected identically");
        const std::array invalidFields{TypeLayout{1, 0}};
        context.expect(!x64.layoutStruct(invalidFields) && !arm64.layoutStruct(invalidFields), "TF-TGT-028 invalid struct fields must be rejected identically");
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addWindowsFactoryCase(registry, "TF-TGT-029", "PR", [](TestContext& context) {
        constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
        const TargetDataLayout x64 = TargetDataLayout::WindowsX64();
        const TargetDataLayout arm64 = TargetDataLayout::WindowsArm64();
        const auto oneX64 = x64.layoutArray({1, 1}, 1);
        const auto oneArm64 = arm64.layoutArray({1, 1}, 1);
        const auto maximumX64 = x64.layoutArray({1, 1}, maximum);
        const auto maximumArm64 = arm64.layoutArray({1, 1}, maximum);
        context.expect(oneX64 && oneArm64 && sameArrayLayout(*oneX64, *oneArm64) && oneX64->size == 1, "TF-TGT-029 one-element boundary must be exact");
        context.expect(maximumX64 && maximumArm64 && sameArrayLayout(*maximumX64, *maximumArm64) && maximumX64->size == maximum, "TF-TGT-029 maximum non-overflow array must be exact");
        context.expect(!x64.layoutArray({2, 2}, maximum) && !arm64.layoutArray({2, 2}, maximum), "TF-TGT-029 overflowing arrays must be rejected identically");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addWindowsFactoryCase(registry, "TF-TGT-030", "PR", [](TestContext& context) {
        const TargetDataLayout x64 = TargetDataLayout::WindowsX64();
        const TargetDataLayout arm64 = TargetDataLayout::WindowsArm64();
        for (std::size_t index = 0; index < kBuiltinKinds.size(); ++index)
        {
            const TypeLayout x64Layout = x64.layoutOf(kBuiltinKinds[index]);
            const TypeLayout arm64Layout = arm64.layoutOf(kBuiltinKinds[index]);
            context.expect(sameTypeLayout(x64Layout, kExpectedLayouts[index]), "TF-TGT-030 x64 builtin table entry must be exact");
            context.expect(sameTypeLayout(arm64Layout, kExpectedLayouts[index]), "TF-TGT-030 ARM64 builtin table entry must be exact");
        }
        const auto x64Stride = x64.strideOf({3, 4});
        const auto arm64Stride = arm64.strideOf({3, 4});
        context.expect(x64Stride == 4 && arm64Stride == 4, "TF-TGT-030 stride query must match");
        const auto x64Array = x64.layoutArray({3, 4}, 5);
        const auto arm64Array = arm64.layoutArray({3, 4}, 5);
        context.expect(x64Array && arm64Array && sameArrayLayout(*x64Array, *arm64Array), "TF-TGT-030 array query must match");
        const std::array simpleFields{TypeLayout{1, 1}, TypeLayout{4, 4}, TypeLayout{8, 8}};
        const auto simpleX64 = x64.layoutStruct(simpleFields);
        const auto simpleArm64 = arm64.layoutStruct(simpleFields);
        context.expect(simpleX64 && simpleArm64 && sameStructLayout(*simpleX64, *simpleArm64), "TF-TGT-030 simple struct query must match");
        const std::array generalFields{
            StructFieldRequest{{1, 1}, std::nullopt, std::nullopt, false, std::nullopt, false},
            StructFieldRequest{{4, 4}, std::nullopt, std::nullopt, false, std::nullopt, false},
        };
        const StructLayoutOptions options{false, 2, 8};
        const auto generalX64 = x64.layoutStruct(generalFields, options);
        const auto generalArm64 = arm64.layoutStruct(generalFields, options);
        context.expect(generalX64 && generalArm64 && sameStructLayout(*generalX64, *generalArm64), "TF-TGT-030 general struct query must match");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addWindowsFactoryCase(registry, "TF-TGT-031", "PR", [](TestContext& context) {
        const TargetDataLayout firstX64 = TargetDataLayout::WindowsX64();
        const TargetDataLayout secondX64 = TargetDataLayout::WindowsX64();
        const TargetDataLayout firstArm64 = TargetDataLayout::WindowsArm64();
        const TargetDataLayout secondArm64 = TargetDataLayout::WindowsArm64();
        const std::array fields{TypeLayout{2, 2}, TypeLayout{8, 8}, TypeLayout{1, 1}};
        const auto firstStruct = firstX64.layoutStruct(fields);
        const auto secondStruct = secondX64.layoutStruct(fields);
        const auto armStruct = firstArm64.layoutStruct(fields);
        context.expect(firstStruct && secondStruct && armStruct && sameStructLayout(*firstStruct, *secondStruct) && sameStructLayout(*firstStruct, *armStruct), "TF-TGT-031 repeated factory struct results must be exact");
        context.expect(sameTypeLayout(firstX64.layoutOf(BuiltinLayoutKind::Pointer), secondX64.layoutOf(BuiltinLayoutKind::Pointer)) && sameTypeLayout(firstArm64.layoutOf(BuiltinLayoutKind::Pointer), secondArm64.layoutOf(BuiltinLayoutKind::Pointer)), "TF-TGT-031 repeated scalar results must be exact");
        context.expect(firstX64.isLittleEndian() == secondX64.isLittleEndian() && firstArm64.maximumNaturalAlignment() == secondArm64.maximumNaturalAlignment(), "TF-TGT-031 repeated properties must be exact");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addWindowsFactoryCase(registry, "TF-TGT-032", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        const TargetDataLayout x64 = TargetDataLayout::WindowsX64();
        const TargetDataLayout arm64 = TargetDataLayout::WindowsArm64();
        std::uint64_t x64Checksum = 0;
        std::uint64_t arm64Checksum = 0;
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            const BuiltinLayoutKind kind = kBuiltinKinds[index % kBuiltinKinds.size()];
            const std::uint64_t count = static_cast<std::uint64_t>(index) + 1;
            const auto x64Array = x64.layoutArray(x64.layoutOf(kind), count);
            const auto arm64Array = arm64.layoutArray(arm64.layoutOf(kind), count);
            context.expect(x64Array && arm64Array && sameArrayLayout(*x64Array, *arm64Array), "TF-TGT-032 every scaled query must preserve factory parity");
            if (x64Array) x64Checksum += x64Array->size + x64Array->alignment + x64Array->stride + x64Array->count;
            if (arm64Array) arm64Checksum += arm64Array->size + arm64Array->alignment + arm64Array->stride + arm64Array->count;
        }
        context.expect(x64Checksum == 14557107568ULL, "TF-TGT-032 x64 scale checksum must be exact");
        context.expect(arm64Checksum == x64Checksum, "TF-TGT-032 ARM64 scale checksum must match x64");
    });
}
}

void registerTargetWindowsFactoriesModule(TestRegistry& registry)
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
