#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <bit>
#include <cstddef>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::target::CpuFeatureLevel;
using nativesharp::target::TargetInfo;

constexpr std::array kKnownLevels{
    CpuFeatureLevel::Baseline,
    CpuFeatureLevel::X86_64_V2,
    CpuFeatureLevel::X86_64_V3,
    CpuFeatureLevel::X86_64_V4,
};

constexpr CpuFeatureLevel kUnknownLevel = std::bit_cast<CpuFeatureLevel>(
    (std::numeric_limits<std::underlying_type_t<CpuFeatureLevel>>::max)());

struct ExpectedSelection
{
    CpuFeatureLevel requested;
    CpuFeatureLevel selected;
    bool fallback;
};

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-023", "Host.Target", "unit",
        std::move(profile), {"target", "cpu-feature-selection"}, std::move(function)});
}

CpuFeatureLevel lowerRank(CpuFeatureLevel preferred, CpuFeatureLevel available)
{
    return static_cast<unsigned>(preferred) <= static_cast<unsigned>(available)
        ? preferred : available;
}

void expectSelection(TestContext& context, const TargetInfo& target,
    ExpectedSelection expected, std::string prefix)
{
    context.expect(target.requestedCpuFeatureLevel() == expected.requested,
        prefix + " requested level must be exact");
    context.expect(target.cpuFeatureLevel() == expected.selected,
        prefix + " selected level must be exact");
    context.expect(target.usedCpuFeatureFallback() == expected.fallback,
        prefix + " fallback flag must be exact");
}

ExpectedSelection expectedFor(CpuFeatureLevel preferred, CpuFeatureLevel available)
{
    const CpuFeatureLevel selected = lowerRank(preferred, available);
    return {preferred, selected, preferred != selected};
}

bool sameSelection(const TargetInfo& left, const TargetInfo& right)
{
    return left.requestedCpuFeatureLevel() == right.requestedCpuFeatureLevel() &&
           left.cpuFeatureLevel() == right.cpuFeatureLevel() &&
           left.usedCpuFeatureFallback() == right.usedCpuFeatureFallback();
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-177", "PR", [](TestContext& context) {
        expectSelection(context,
            TargetInfo::WindowsX64(CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline),
            {CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline, false},
            "TF-TGT-177 baseline selection");
    });
    addCase(registry, "TF-TGT-178", "PR", [](TestContext& context) {
        expectSelection(context,
            TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::X86_64_V2),
            {CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::X86_64_V2, true},
            "TF-TGT-178 representative fallback");
    });
    addCase(registry, "TF-TGT-179", "PR", [](TestContext& context) {
        expectSelection(context, TargetInfo::WindowsX64(),
            {CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline, false},
            "TF-TGT-179 implicit baseline");
    });
}

void registerUnknownAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-180", "PR", [](TestContext& context) {
        expectSelection(context, TargetInfo::WindowsX64(kUnknownLevel, CpuFeatureLevel::X86_64_V3),
            {kUnknownLevel, CpuFeatureLevel::X86_64_V3, true},
            "TF-TGT-180 unknown preferred level");
        expectSelection(context, TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V3, kUnknownLevel),
            {CpuFeatureLevel::X86_64_V3, CpuFeatureLevel::X86_64_V3, false},
            "TF-TGT-180 unknown available level");
        expectSelection(context, TargetInfo::WindowsX64(kUnknownLevel, kUnknownLevel),
            {kUnknownLevel, kUnknownLevel, false}, "TF-TGT-180 matching unknown levels");
    });
    addCase(registry, "TF-TGT-181", "PR", [](TestContext& context) {
        expectSelection(context,
            TargetInfo::WindowsX64(CpuFeatureLevel::Baseline, CpuFeatureLevel::X86_64_V4),
            {CpuFeatureLevel::Baseline, CpuFeatureLevel::Baseline, false},
            "TF-TGT-181 lower preferred boundary");
        expectSelection(context,
            TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::Baseline),
            {CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::Baseline, true},
            "TF-TGT-181 lower available boundary");
        expectSelection(context,
            TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::X86_64_V4),
            {CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::X86_64_V4, false},
            "TF-TGT-181 equal upper boundary");
    });
}

void registerMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-182", "PR", [](TestContext& context) {
        for (const CpuFeatureLevel preferred : kKnownLevels)
        {
            for (const CpuFeatureLevel available : kKnownLevels)
            {
                expectSelection(context, TargetInfo::WindowsX64(preferred, available),
                    expectedFor(preferred, available), "TF-TGT-182 known-level matrix entry");
            }
        }
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-183", "PR", [](TestContext& context) {
        std::array<TargetInfo, kKnownLevels.size()> forward{
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64(),
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64()};
        std::array<TargetInfo, kKnownLevels.size()> reverse{
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64(),
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64()};
        for (std::size_t index = 0; index < kKnownLevels.size(); ++index)
            forward[index] = TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, kKnownLevels[index]);
        for (std::size_t index = kKnownLevels.size(); index-- > 0;)
            reverse[index] = TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, kKnownLevels[index]);
        for (std::size_t index = 0; index < kKnownLevels.size(); ++index)
        {
            const TargetInfo repeated =
                TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, kKnownLevels[index]);
            context.expect(sameSelection(forward[index], reverse[index]) &&
                sameSelection(forward[index], repeated),
                "TF-TGT-183 selection must be independent of order and repetition");
        }
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-184", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        constexpr std::array levels{
            CpuFeatureLevel::Baseline,
            CpuFeatureLevel::X86_64_V2,
            CpuFeatureLevel::X86_64_V3,
            CpuFeatureLevel::X86_64_V4,
            kUnknownLevel,
        };
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            const CpuFeatureLevel preferred = levels[index % levels.size()];
            const CpuFeatureLevel available = levels[(index / levels.size()) % levels.size()];
            const TargetInfo target = TargetInfo::WindowsX64(preferred, available);
            const ExpectedSelection expected = expectedFor(preferred, available);
            const bool exact = target.requestedCpuFeatureLevel() == expected.requested &&
                target.cpuFeatureLevel() == expected.selected &&
                target.usedCpuFeatureFallback() == expected.fallback;
            context.expect(exact, "TF-TGT-184 every scaled selection must match ordinal reference");
        }
    });
}
}

void registerTargetCpuFeatureSelectionModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerUnknownAndBoundaryCases(registry);
    registerMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
