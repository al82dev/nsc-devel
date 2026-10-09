#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Target/TargetDataLayout.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <bit>
#include <cstdint>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::target::Architecture;
using nativesharp::target::BuiltinLayoutKind;
using nativesharp::target::CpuFeatureLevel;
using nativesharp::target::ObjectFormat;
using nativesharp::target::OperatingSystem;
using nativesharp::target::TargetInfo;

constexpr std::array kArchitectures{Architecture::X86_64, Architecture::Arm64};
constexpr std::array kOperatingSystems{OperatingSystem::Windows};
constexpr std::array kObjectFormats{ObjectFormat::COFF};
constexpr std::array kCpuFeatureLevels{
    CpuFeatureLevel::Baseline,
    CpuFeatureLevel::X86_64_V2,
    CpuFeatureLevel::X86_64_V3,
    CpuFeatureLevel::X86_64_V4,
};
constexpr std::array kBuiltinLayoutKinds{
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

void addEnumCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CF-TGT-001",
        "Host.Target",
        "unit",
        std::move(profile),
        {"target", "enum"},
        std::move(function),
    });
}

template <typename Enum, std::size_t Size>
bool hasSequentialValues(const std::array<Enum, Size>& values)
{
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (static_cast<std::size_t>(values[index]) != index) return false;
    }
    return true;
}

void registerMinimumCase(TestRegistry& registry)
{
    addEnumCase(registry, "TF-TGT-001", "PR", [](TestContext& context) {
        const TargetInfo target = TargetInfo::WindowsX64();
        context.expect(static_cast<int>(Architecture::X86_64) == 0, "TF-TGT-001 X86_64 must remain zero");
        context.expect(static_cast<int>(OperatingSystem::Windows) == 0, "TF-TGT-001 Windows must remain zero");
        context.expect(static_cast<int>(ObjectFormat::COFF) == 0, "TF-TGT-001 COFF must remain zero");
        context.expect(static_cast<int>(CpuFeatureLevel::Baseline) == 0, "TF-TGT-001 baseline must remain zero");
        context.expect(static_cast<int>(BuiltinLayoutKind::Bool) == 0, "TF-TGT-001 Bool must remain zero");
        context.expect(target.architecture() == Architecture::X86_64, "TF-TGT-001 x64 factory must preserve architecture");
        context.expect(target.operatingSystem() == OperatingSystem::Windows, "TF-TGT-001 x64 factory must preserve OS");
        context.expect(target.objectFormat() == ObjectFormat::COFF, "TF-TGT-001 x64 factory must preserve format");
    });
}

void registerRepresentativeCase(TestRegistry& registry)
{
    addEnumCase(registry, "TF-TGT-002", "PR", [](TestContext& context) {
        const TargetInfo arm64 = TargetInfo::WindowsArm64();
        const TargetInfo selected = TargetInfo::WindowsX64(
            CpuFeatureLevel::X86_64_V3, CpuFeatureLevel::X86_64_V4);
        context.expect(arm64.architecture() == Architecture::Arm64, "TF-TGT-002 ARM64 identity must be exact");
        context.expect(arm64.operatingSystem() == OperatingSystem::Windows, "TF-TGT-002 ARM64 OS must be Windows");
        context.expect(arm64.objectFormat() == ObjectFormat::COFF, "TF-TGT-002 ARM64 format must be COFF");
        context.expect(selected.requestedCpuFeatureLevel() == CpuFeatureLevel::X86_64_V3, "TF-TGT-002 requested V3 must be exact");
        context.expect(selected.cpuFeatureLevel() == CpuFeatureLevel::X86_64_V3, "TF-TGT-002 selected V3 must be exact");
        context.expect(static_cast<int>(BuiltinLayoutKind::Pointer) == 17, "TF-TGT-002 Pointer must remain ordinal 17");
    });
}

void registerNoPrerequisiteCase(TestRegistry& registry)
{
    addEnumCase(registry, "TF-TGT-003", "PR", [](TestContext& context) {
        context.expect(static_cast<int>(Architecture{}) == 0, "TF-TGT-003 architecture value-init must be zero");
        context.expect(static_cast<int>(OperatingSystem{}) == 0, "TF-TGT-003 OS value-init must be zero");
        context.expect(static_cast<int>(ObjectFormat{}) == 0, "TF-TGT-003 format value-init must be zero");
        context.expect(static_cast<int>(CpuFeatureLevel{}) == 0, "TF-TGT-003 CPU value-init must be zero");
        context.expect(static_cast<int>(BuiltinLayoutKind{}) == 0, "TF-TGT-003 builtin value-init must be zero");
    });
}

void registerUnknownValueCase(TestRegistry& registry)
{
    addEnumCase(registry, "TF-TGT-004", "PR", [](TestContext& context) {
        constexpr int unknownValue = 255;
        const BuiltinLayoutKind builtin = std::bit_cast<BuiltinLayoutKind>(unknownValue);
        const nativesharp::target::TypeLayout layout =
            nativesharp::target::TargetDataLayout::WindowsX64().layoutOf(builtin);
        context.expect(layout.size == 0, "TF-TGT-004 unknown builtin kind must use zero size fallback");
        context.expect(layout.alignment == 1, "TF-TGT-004 unknown builtin kind must use alignment-one fallback");
    });
}

void registerBoundaryCase(TestRegistry& registry)
{
    addEnumCase(registry, "TF-TGT-005", "PR", [](TestContext& context) {
        context.expect(static_cast<int>(kArchitectures.front()) == 0 && static_cast<int>(kArchitectures.back()) == 1, "TF-TGT-005 architecture boundaries must be 0..1");
        context.expect(static_cast<int>(kCpuFeatureLevels.front()) == 0 && static_cast<int>(kCpuFeatureLevels.back()) == 3, "TF-TGT-005 CPU boundaries must be 0..3");
        context.expect(static_cast<int>(kBuiltinLayoutKinds.front()) == 0 && static_cast<int>(kBuiltinLayoutKinds.back()) == 17, "TF-TGT-005 builtin boundaries must be 0..17");
        context.expect(static_cast<int>(kOperatingSystems.front()) == 0 && static_cast<int>(kOperatingSystems.back()) == 0, "TF-TGT-005 OS boundary must be zero");
        context.expect(static_cast<int>(kObjectFormats.front()) == 0 && static_cast<int>(kObjectFormats.back()) == 0, "TF-TGT-005 format boundary must be zero");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addEnumCase(registry, "TF-TGT-006", "PR", [](TestContext& context) {
        context.expect(kArchitectures.size() == 2 && hasSequentialValues(kArchitectures), "TF-TGT-006 architecture table must be exact");
        context.expect(kOperatingSystems.size() == 1 && hasSequentialValues(kOperatingSystems), "TF-TGT-006 OS table must be exact");
        context.expect(kObjectFormats.size() == 1 && hasSequentialValues(kObjectFormats), "TF-TGT-006 format table must be exact");
        context.expect(kCpuFeatureLevels.size() == 4 && hasSequentialValues(kCpuFeatureLevels), "TF-TGT-006 CPU table must be exact");
        context.expect(kBuiltinLayoutKinds.size() == 18 && hasSequentialValues(kBuiltinLayoutKinds), "TF-TGT-006 builtin table must be exact");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addEnumCase(registry, "TF-TGT-007", "PR", [](TestContext& context) {
        const auto firstKinds = kBuiltinLayoutKinds;
        const auto secondKinds = kBuiltinLayoutKinds;
        const TargetInfo first = TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::X86_64_V2);
        const TargetInfo second = TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::X86_64_V2);
        context.expect(firstKinds == secondKinds, "TF-TGT-007 repeated enum tables must be identical");
        context.expect(first.architecture() == second.architecture(), "TF-TGT-007 repeated architecture must be identical");
        context.expect(first.operatingSystem() == second.operatingSystem(), "TF-TGT-007 repeated OS must be identical");
        context.expect(first.objectFormat() == second.objectFormat(), "TF-TGT-007 repeated format must be identical");
        context.expect(first.requestedCpuFeatureLevel() == second.requestedCpuFeatureLevel(), "TF-TGT-007 repeated request must be identical");
        context.expect(first.cpuFeatureLevel() == second.cpuFeatureLevel(), "TF-TGT-007 repeated selection must be identical");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addEnumCase(registry, "TF-TGT-008", "NIGHTLY", [](TestContext& context) {
        constexpr std::uint64_t iterations = 32768;
        std::uint64_t checksum = 0;
        for (std::uint64_t iteration = 0; iteration < iterations; ++iteration)
        {
            for (const Architecture value : kArchitectures) checksum += static_cast<std::uint64_t>(value);
            for (const OperatingSystem value : kOperatingSystems) checksum += static_cast<std::uint64_t>(value);
            for (const ObjectFormat value : kObjectFormats) checksum += static_cast<std::uint64_t>(value);
            for (const CpuFeatureLevel value : kCpuFeatureLevels) checksum += static_cast<std::uint64_t>(value);
            for (const BuiltinLayoutKind value : kBuiltinLayoutKinds) checksum += static_cast<std::uint64_t>(value);
        }
        context.expect(checksum == 5242880, "TF-TGT-008 exhaustive repeated enum checksum must remain exact");
    });
}
}

void registerTargetDomainEnumsModule(TestRegistry& registry)
{
    registerMinimumCase(registry);
    registerRepresentativeCase(registry);
    registerNoPrerequisiteCase(registry);
    registerUnknownValueCase(registry);
    registerBoundaryCase(registry);
    registerVariantMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
