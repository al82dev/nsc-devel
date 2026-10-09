#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::target::CpuFeatureLevel;
using nativesharp::target::TargetInfo;

constexpr std::array kLevels{
    CpuFeatureLevel::Baseline,
    CpuFeatureLevel::X86_64_V2,
    CpuFeatureLevel::X86_64_V3,
    CpuFeatureLevel::X86_64_V4,
};

struct ExpectedNames
{
    std::string_view architecture;
    std::string_view operatingSystem;
    std::string_view objectFormat;
};

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-024", "Host.Target", "unit",
        std::move(profile), {"target", "display-names"}, std::move(function)});
}

void expectNames(TestContext& context, const TargetInfo& target,
    ExpectedNames expected, std::string prefix)
{
    context.expect(target.architectureName() == expected.architecture,
        prefix + " architecture spelling must be exact");
    context.expect(target.operatingSystemName() == expected.operatingSystem,
        prefix + " operating-system spelling must be exact");
    context.expect(target.objectFormatName() == expected.objectFormat,
        prefix + " object-format spelling must be exact");
}

bool sameNames(const TargetInfo& left, const TargetInfo& right)
{
    return left.architectureName() == right.architectureName() &&
           left.operatingSystemName() == right.operatingSystemName() &&
           left.objectFormatName() == right.objectFormatName();
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-185", "PR", [](TestContext& context) {
        expectNames(context, TargetInfo::WindowsX64(), {"x86-64", "windows", "COFF"},
            "TF-TGT-185 x64");
    });
    addCase(registry, "TF-TGT-186", "PR", [](TestContext& context) {
        expectNames(context, TargetInfo::WindowsArm64(), {"arm64", "windows", "COFF"},
            "TF-TGT-186 ARM64");
    });
    addCase(registry, "TF-TGT-187", "PR", [](TestContext& context) {
        const TargetInfo fresh = TargetInfo::WindowsX64();
        expectNames(context, fresh, {"x86-64", "windows", "COFF"}, "TF-TGT-187 fresh target");
    });
}

void registerInvariantAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-188", "PR", [](TestContext& context) {
        const TargetInfo constrained =
            TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::Baseline);
        expectNames(context, constrained, {"x86-64", "windows", "COFF"},
            "TF-TGT-188 CPU-constrained x64");
    });
    addCase(registry, "TF-TGT-189", "PR", [](TestContext& context) {
        const TargetInfo x64 = TargetInfo::WindowsX64();
        const TargetInfo arm64 = TargetInfo::WindowsArm64();
        context.expect(x64.architectureName().size() == 6 && arm64.architectureName().size() == 5,
            "TF-TGT-189 architecture lengths must be exact");
        context.expect(x64.operatingSystemName().size() == 7 && arm64.operatingSystemName().size() == 7,
            "TF-TGT-189 operating-system lengths must be exact");
        context.expect(x64.objectFormatName().size() == 4 && arm64.objectFormatName().size() == 4,
            "TF-TGT-189 object-format lengths must be exact");
        context.expect(!x64.architectureName().empty() && !arm64.architectureName().empty(),
            "TF-TGT-189 architecture names must not be empty");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-190", "PR", [](TestContext& context) {
        for (const CpuFeatureLevel preferred : kLevels)
            for (const CpuFeatureLevel available : kLevels)
                expectNames(context, TargetInfo::WindowsX64(preferred, available),
                    {"x86-64", "windows", "COFF"}, "TF-TGT-190 x64 matrix entry");
        expectNames(context, TargetInfo::WindowsArm64(), {"arm64", "windows", "COFF"},
            "TF-TGT-190 ARM64 variant");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-191", "PR", [](TestContext& context) {
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
            context.expect(sameNames(forward[index], reverse[index]) &&
                sameNames(forward[index], TargetInfo::WindowsX64(kLevels[index], CpuFeatureLevel::X86_64_V4)),
                "TF-TGT-191 names must be independent of order and repetition");
        }
        context.expect(sameNames(TargetInfo::WindowsArm64(), TargetInfo::WindowsArm64()),
            "TF-TGT-191 ARM64 names must be deterministic");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-192", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            if ((index & 1U) == 0)
            {
                const TargetInfo target = TargetInfo::WindowsX64(
                    kLevels[index % kLevels.size()], kLevels[(index / kLevels.size()) % kLevels.size()]);
                const bool exact = target.architectureName() == "x86-64" &&
                    target.operatingSystemName() == "windows" && target.objectFormatName() == "COFF";
                context.expect(exact, "TF-TGT-192 every scaled x64 spelling must be exact");
            }
            else
            {
                const TargetInfo target = TargetInfo::WindowsArm64();
                const bool exact = target.architectureName() == "arm64" &&
                    target.operatingSystemName() == "windows" && target.objectFormatName() == "COFF";
                context.expect(exact, "TF-TGT-192 every scaled ARM64 spelling must be exact");
            }
        }
    });
}
}

void registerTargetDisplayNamesModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvariantAndBoundaryCases(registry);
    registerVariantMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
