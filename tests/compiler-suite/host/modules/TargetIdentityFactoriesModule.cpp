#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::target::Architecture;
using nativesharp::target::CpuFeatureLevel;
using nativesharp::target::ObjectFormat;
using nativesharp::target::OperatingSystem;
using nativesharp::target::TargetInfo;

constexpr std::array kLevels{
    CpuFeatureLevel::Baseline,
    CpuFeatureLevel::X86_64_V2,
    CpuFeatureLevel::X86_64_V3,
    CpuFeatureLevel::X86_64_V4,
};

struct ExpectedIdentity
{
    Architecture architecture;
    CpuFeatureLevel requested;
    CpuFeatureLevel selected;
};

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-022", "Host.Target", "unit",
        std::move(profile), {"target", "identity-factory"}, std::move(function)});
}

void expectIdentity(TestContext& context, const TargetInfo& target,
    ExpectedIdentity expected, std::string prefix)
{
    context.expect(target.architecture() == expected.architecture,
        prefix + " architecture must be exact");
    context.expect(target.operatingSystem() == OperatingSystem::Windows,
        prefix + " operating system must be Windows");
    context.expect(target.objectFormat() == ObjectFormat::COFF,
        prefix + " object format must be COFF");
    context.expect(target.requestedCpuFeatureLevel() == expected.requested,
        prefix + " requested CPU level must be exact");
    context.expect(target.cpuFeatureLevel() == expected.selected,
        prefix + " selected CPU level must be exact");
}

CpuFeatureLevel minimumLevel(CpuFeatureLevel left, CpuFeatureLevel right)
{
    return static_cast<unsigned>(left) <= static_cast<unsigned>(right) ? left : right;
}

bool sameIdentity(const TargetInfo& left, const TargetInfo& right)
{
    return left.architecture() == right.architecture() &&
           left.operatingSystem() == right.operatingSystem() &&
           left.objectFormat() == right.objectFormat() &&
           left.requestedCpuFeatureLevel() == right.requestedCpuFeatureLevel() &&
           left.cpuFeatureLevel() == right.cpuFeatureLevel();
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-169", "PR", [](TestContext& context) {
        expectIdentity(context, TargetInfo::WindowsX64(), {Architecture::X86_64,
            CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline}, "TF-TGT-169 default x64");
    });
    addCase(registry, "TF-TGT-170", "PR", [](TestContext& context) {
        expectIdentity(context,
            TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V3, CpuFeatureLevel::X86_64_V4),
            {Architecture::X86_64, CpuFeatureLevel::X86_64_V3, CpuFeatureLevel::X86_64_V3},
            "TF-TGT-170 custom x64");
    });
    addCase(registry, "TF-TGT-171", "PR", [](TestContext& context) {
        const TargetInfo x64 = TargetInfo::WindowsX64();
        const TargetInfo arm64 = TargetInfo::WindowsArm64();
        expectIdentity(context, x64, {Architecture::X86_64,
            CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline}, "TF-TGT-171 fresh x64");
        expectIdentity(context, arm64, {Architecture::Arm64,
            CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline}, "TF-TGT-171 fresh ARM64");
    });
}

void registerInvariantAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-172", "PR", [](TestContext& context) {
        const TargetInfo constrained =
            TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::Baseline);
        expectIdentity(context, constrained, {Architecture::X86_64,
            CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::Baseline},
            "TF-TGT-172 constrained x64");
    });
    addCase(registry, "TF-TGT-173", "PR", [](TestContext& context) {
        expectIdentity(context,
            TargetInfo::WindowsX64(CpuFeatureLevel::Baseline, CpuFeatureLevel::X86_64_V4),
            {Architecture::X86_64, CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline},
            "TF-TGT-173 lower boundary");
        expectIdentity(context,
            TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::X86_64_V4),
            {Architecture::X86_64, CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::X86_64_V4},
            "TF-TGT-173 upper boundary");
        expectIdentity(context, TargetInfo::WindowsArm64(), {Architecture::Arm64,
            CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline}, "TF-TGT-173 ARM64 boundary");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-174", "PR", [](TestContext& context) {
        for (const CpuFeatureLevel preferred : kLevels)
        {
            for (const CpuFeatureLevel available : kLevels)
            {
                expectIdentity(context, TargetInfo::WindowsX64(preferred, available),
                    {Architecture::X86_64, preferred, minimumLevel(preferred, available)},
                    "TF-TGT-174 x64 matrix entry");
            }
        }
        expectIdentity(context, TargetInfo::WindowsArm64(), {Architecture::Arm64,
            CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline}, "TF-TGT-174 ARM64 variant");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-175", "PR", [](TestContext& context) {
        std::array<TargetInfo, kLevels.size()> forward{
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64(),
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64()};
        std::array<TargetInfo, kLevels.size()> reverse{
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64(),
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64()};
        for (std::size_t index = 0; index < kLevels.size(); ++index)
            forward[index] = TargetInfo::WindowsX64(kLevels[index], CpuFeatureLevel::X86_64_V4);
        for (std::size_t index = kLevels.size(); index-- > 0;)
            reverse[index] = TargetInfo::WindowsX64(kLevels[index], CpuFeatureLevel::X86_64_V4);
        for (std::size_t index = 0; index < kLevels.size(); ++index)
        {
            const TargetInfo repeated =
                TargetInfo::WindowsX64(kLevels[index], CpuFeatureLevel::X86_64_V4);
            context.expect(sameIdentity(forward[index], reverse[index]) &&
                sameIdentity(forward[index], repeated),
                "TF-TGT-175 factory identity must be independent of order and repetition");
        }
        context.expect(sameIdentity(TargetInfo::WindowsArm64(), TargetInfo::WindowsArm64()),
            "TF-TGT-175 ARM64 factory must be deterministic");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-176", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            const CpuFeatureLevel preferred = kLevels[index % kLevels.size()];
            const CpuFeatureLevel available = kLevels[(index / kLevels.size()) % kLevels.size()];
            const TargetInfo target = TargetInfo::WindowsX64(preferred, available);
            const bool exact = target.architecture() == Architecture::X86_64 &&
                target.operatingSystem() == OperatingSystem::Windows &&
                target.objectFormat() == ObjectFormat::COFF &&
                target.requestedCpuFeatureLevel() == preferred &&
                target.cpuFeatureLevel() == minimumLevel(preferred, available);
            context.expect(exact, "TF-TGT-176 every scaled factory result must preserve identity");
        }
        expectIdentity(context, TargetInfo::WindowsArm64(), {Architecture::Arm64,
            CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline}, "TF-TGT-176 ARM64 after scale");
    });
}
}

void registerTargetIdentityFactoriesModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvariantAndBoundaryCases(registry);
    registerVariantMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
