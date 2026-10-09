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

struct FeatureFixture
{
    std::string_view name;
    unsigned minimumRank;
};

constexpr unsigned kUnsupported = 99;
constexpr std::array kX64Features{
    FeatureFixture{"", 0}, FeatureFixture{"x87", 0}, FeatureFixture{"mmx", 0},
    FeatureFixture{"sse", 0}, FeatureFixture{"sse2", 0},
    FeatureFixture{"sse3", 1}, FeatureFixture{"ssse3", 1}, FeatureFixture{"sse4.1", 1},
    FeatureFixture{"sse4.2", 1}, FeatureFixture{"popcnt", 1},
    FeatureFixture{"avx", 2}, FeatureFixture{"avx2", 2}, FeatureFixture{"bmi", 2},
    FeatureFixture{"bmi2", 2}, FeatureFixture{"f16c", 2}, FeatureFixture{"fma", 2},
    FeatureFixture{"lzcnt", 2}, FeatureFixture{"movbe", 2}, FeatureFixture{"xsave", 2},
    FeatureFixture{"avx512f", 3}, FeatureFixture{"avx512bw", 3},
    FeatureFixture{"avx512cd", 3}, FeatureFixture{"avx512dq", 3},
    FeatureFixture{"avx512vl", 3}, FeatureFixture{"sha", kUnsupported},
    FeatureFixture{"SSE2", kUnsupported},
};

constexpr std::array kLevels{
    CpuFeatureLevel::Baseline,
    CpuFeatureLevel::X86_64_V2,
    CpuFeatureLevel::X86_64_V3,
    CpuFeatureLevel::X86_64_V4,
};

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-027", "Host.Target", "unit",
        std::move(profile), {"target", "cpu-feature-support"}, std::move(function)});
}

bool expectedX64(CpuFeatureLevel level, const FeatureFixture& feature)
{
    return feature.minimumRank != kUnsupported &&
           static_cast<unsigned>(level) >= feature.minimumRank;
}

bool expectedArm64(std::string_view feature)
{
    return feature.empty() || feature == "neon" || feature == "fp-armv8";
}

TargetInfo x64At(CpuFeatureLevel level)
{
    return TargetInfo::WindowsX64(level, level);
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-209", "PR", [](TestContext& context) {
        const TargetInfo baseline = x64At(CpuFeatureLevel::Baseline);
        context.expect(baseline.supportsCpuFeature(""), "TF-TGT-209 empty feature must be supported");
        context.expect(baseline.supportsCpuFeature("sse2"), "TF-TGT-209 baseline must support sse2");
    });
    addCase(registry, "TF-TGT-210", "PR", [](TestContext& context) {
        const TargetInfo v3 = x64At(CpuFeatureLevel::X86_64_V3);
        context.expect(v3.supportsCpuFeature("sse4.2") && v3.supportsCpuFeature("avx2"),
            "TF-TGT-210 x86-64-v3 must include earlier and current features");
        context.expect(!v3.supportsCpuFeature("avx512f"),
            "TF-TGT-210 x86-64-v3 must not include v4 features");
    });
    addCase(registry, "TF-TGT-211", "PR", [](TestContext& context) {
        const TargetInfo fresh = TargetInfo::WindowsX64();
        context.expect(fresh.supportsCpuFeature("sse") && !fresh.supportsCpuFeature("sse3"),
            "TF-TGT-211 default target must expose baseline without external metadata");
    });
}

void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-212", "PR", [](TestContext& context) {
        const TargetInfo v4 = x64At(CpuFeatureLevel::X86_64_V4);
        const TargetInfo arm64 = TargetInfo::WindowsArm64();
        context.expect(!v4.supportsCpuFeature("sha") && !v4.supportsCpuFeature("SSE2"),
            "TF-TGT-212 unknown or noncanonical x64 features must be rejected");
        context.expect(!arm64.supportsCpuFeature("sse2") && !arm64.supportsCpuFeature("NEON"),
            "TF-TGT-212 unsupported or noncanonical ARM64 features must be rejected");
    });
    addCase(registry, "TF-TGT-213", "PR", [](TestContext& context) {
        const TargetInfo baseline = x64At(CpuFeatureLevel::Baseline);
        const TargetInfo v4 = x64At(CpuFeatureLevel::X86_64_V4);
        const TargetInfo arm64 = TargetInfo::WindowsArm64();
        context.expect(baseline.supportsCpuFeature("") && arm64.supportsCpuFeature(""),
            "TF-TGT-213 empty feature must be supported on both architectures");
        context.expect(baseline.supportsCpuFeature("x87") && !baseline.supportsCpuFeature("sse3"),
            "TF-TGT-213 baseline boundary must be exact");
        context.expect(v4.supportsCpuFeature("avx512vl"),
            "TF-TGT-213 final v4 feature must be supported");
        context.expect(arm64.supportsCpuFeature("neon") && arm64.supportsCpuFeature("fp-armv8") &&
            !arm64.supportsCpuFeature("avx"), "TF-TGT-213 ARM64 boundary must be exact");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-214", "PR", [](TestContext& context) {
        for (const CpuFeatureLevel level : kLevels)
        {
            const TargetInfo target = x64At(level);
            for (const FeatureFixture& feature : kX64Features)
                context.expect(target.supportsCpuFeature(feature.name) == expectedX64(level, feature),
                    "TF-TGT-214 x64 level-by-feature verdict must match the independent table");
        }
        const TargetInfo arm64 = TargetInfo::WindowsArm64();
        constexpr std::array armFeatures{"", "neon", "fp-armv8", "sse2", "NEON", "unknown"};
        for (const std::string_view feature : armFeatures)
            context.expect(arm64.supportsCpuFeature(feature) == expectedArm64(feature),
                "TF-TGT-214 ARM64 feature verdict must match the independent table");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-215", "PR", [](TestContext& context) {
        std::array<bool, kX64Features.size()> forward{};
        std::array<bool, kX64Features.size()> reverse{};
        const TargetInfo v4 = x64At(CpuFeatureLevel::X86_64_V4);
        for (std::size_t index = 0; index < kX64Features.size(); ++index)
            forward[index] = v4.supportsCpuFeature(kX64Features[index].name);
        for (std::size_t index = kX64Features.size(); index-- > 0;)
            reverse[index] = v4.supportsCpuFeature(kX64Features[index].name);
        for (std::size_t index = 0; index < kX64Features.size(); ++index)
            context.expect(forward[index] == reverse[index] &&
                forward[index] == v4.supportsCpuFeature(kX64Features[index].name),
                "TF-TGT-215 feature verdict must be independent of order and repetition");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-216", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        constexpr std::array armFeatures{"", "neon", "fp-armv8", "sse2", "NEON", "unknown"};
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            if ((index & 1U) == 0)
            {
                const CpuFeatureLevel level = kLevels[(index / 2) % kLevels.size()];
                const FeatureFixture& feature = kX64Features[(index / 2) % kX64Features.size()];
                context.expect(x64At(level).supportsCpuFeature(feature.name) == expectedX64(level, feature),
                    "TF-TGT-216 every scaled x64 verdict must match the independent table");
            }
            else
            {
                const std::string_view feature = armFeatures[(index / 2) % armFeatures.size()];
                context.expect(TargetInfo::WindowsArm64().supportsCpuFeature(feature) == expectedArm64(feature),
                    "TF-TGT-216 every scaled ARM64 verdict must match the independent table");
            }
        }
    });
}
}

void registerTargetCpuFeatureSupportModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerVariantMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
