#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Target/TargetInfo.h"

#include <array>
#include <bit>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::target::CpuFeatureLevel;
using nativesharp::target::TargetInfo;

struct ExpectedMachineStrings
{
    std::string_view baseline;
    std::string_view cpu;
    std::string_view features;
};

struct X64Fixture
{
    CpuFeatureLevel level;
    ExpectedMachineStrings expected;
};

constexpr std::array kX64Fixtures{
    X64Fixture{CpuFeatureLevel::Baseline, {"x86-64-v1", "x86-64", "+sse,+sse2"}},
    X64Fixture{CpuFeatureLevel::X86_64_V2,
        {"x86-64-v2", "x86-64-v2", "+sse3,+ssse3,+sse4.1,+sse4.2,+popcnt"}},
    X64Fixture{CpuFeatureLevel::X86_64_V3,
        {"x86-64-v3", "x86-64-v3", "+avx,+avx2,+bmi,+bmi2,+f16c,+fma,+lzcnt,+movbe,+xsave"}},
    X64Fixture{CpuFeatureLevel::X86_64_V4,
        {"x86-64-v4", "x86-64-v4", "+avx512f,+avx512bw,+avx512cd,+avx512dq,+avx512vl"}},
};

constexpr CpuFeatureLevel kUnknownLevel = std::bit_cast<CpuFeatureLevel>(
    (std::numeric_limits<std::underlying_type_t<CpuFeatureLevel>>::max)());

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-026", "Host.Target", "unit",
        std::move(profile), {"target", "machine-strings"}, std::move(function)});
}

void expectMachineStrings(TestContext& context, const TargetInfo& target,
    ExpectedMachineStrings expected, std::string prefix)
{
    context.expect(target.cpuBaselineName() == expected.baseline,
        prefix + " baseline name must be exact");
    context.expect(target.llvmCpuName() == expected.cpu, prefix + " LLVM CPU name must be exact");
    context.expect(target.llvmFeatureString() == expected.features,
        prefix + " LLVM feature string must be exact");
}

bool sameMachineStrings(const TargetInfo& left, const TargetInfo& right)
{
    return left.cpuBaselineName() == right.cpuBaselineName() &&
           left.llvmCpuName() == right.llvmCpuName() &&
           left.llvmFeatureString() == right.llvmFeatureString();
}

TargetInfo x64At(CpuFeatureLevel level)
{
    return TargetInfo::WindowsX64(level, level);
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-201", "PR", [](TestContext& context) {
        expectMachineStrings(context, x64At(CpuFeatureLevel::Baseline), kX64Fixtures[0].expected,
            "TF-TGT-201 baseline x64");
    });
    addCase(registry, "TF-TGT-202", "PR", [](TestContext& context) {
        expectMachineStrings(context, x64At(CpuFeatureLevel::X86_64_V3), kX64Fixtures[2].expected,
            "TF-TGT-202 x64-v3");
    });
    addCase(registry, "TF-TGT-203", "PR", [](TestContext& context) {
        expectMachineStrings(context, TargetInfo::WindowsX64(), kX64Fixtures[0].expected,
            "TF-TGT-203 default x64");
    });
}

void registerInvalidAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-204", "PR", [](TestContext& context) {
        expectMachineStrings(context, x64At(kUnknownLevel), {"", "", ""},
            "TF-TGT-204 unknown x64 level");
    });
    addCase(registry, "TF-TGT-205", "PR", [](TestContext& context) {
        expectMachineStrings(context, x64At(CpuFeatureLevel::X86_64_V4), kX64Fixtures[3].expected,
            "TF-TGT-205 upper x64 boundary");
        expectMachineStrings(context, TargetInfo::WindowsArm64(),
            {"armv8-a", "generic", "+neon,+fp-armv8"}, "TF-TGT-205 ARM64 boundary");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-206", "PR", [](TestContext& context) {
        for (const X64Fixture& fixture : kX64Fixtures)
            expectMachineStrings(context, x64At(fixture.level), fixture.expected,
                "TF-TGT-206 x64 table entry");
        expectMachineStrings(context, TargetInfo::WindowsArm64(),
            {"armv8-a", "generic", "+neon,+fp-armv8"}, "TF-TGT-206 ARM64 entry");
        expectMachineStrings(context, x64At(kUnknownLevel), {"", "", ""},
            "TF-TGT-206 unknown entry");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-207", "PR", [](TestContext& context) {
        std::array<TargetInfo, kX64Fixtures.size()> forward{
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64(),
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64()};
        std::array<TargetInfo, kX64Fixtures.size()> reverse{
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64(),
            TargetInfo::WindowsX64(), TargetInfo::WindowsX64()};
        for (std::size_t index = 0; index < kX64Fixtures.size(); ++index)
            forward[index] = x64At(kX64Fixtures[index].level);
        for (std::size_t index = kX64Fixtures.size(); index-- > 0;)
            reverse[index] = x64At(kX64Fixtures[index].level);
        for (std::size_t index = 0; index < kX64Fixtures.size(); ++index)
        {
            context.expect(sameMachineStrings(forward[index], reverse[index]) &&
                sameMachineStrings(forward[index], x64At(kX64Fixtures[index].level)),
                "TF-TGT-207 machine strings must be independent of order and repetition");
        }
        context.expect(sameMachineStrings(TargetInfo::WindowsArm64(), TargetInfo::WindowsArm64()),
            "TF-TGT-207 ARM64 machine strings must be deterministic");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-208", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            const std::size_t selector = index % (kX64Fixtures.size() + 2);
            if (selector < kX64Fixtures.size())
            {
                const X64Fixture& fixture = kX64Fixtures[selector];
                const TargetInfo target = x64At(fixture.level);
                const bool exact = target.cpuBaselineName() == fixture.expected.baseline &&
                    target.llvmCpuName() == fixture.expected.cpu &&
                    target.llvmFeatureString() == fixture.expected.features;
                context.expect(exact, "TF-TGT-208 every scaled x64 table lookup must be exact");
            }
            else if (selector == kX64Fixtures.size())
            {
                const TargetInfo target = TargetInfo::WindowsArm64();
                const bool exact = target.cpuBaselineName() == "armv8-a" &&
                    target.llvmCpuName() == "generic" && target.llvmFeatureString() == "+neon,+fp-armv8";
                context.expect(exact, "TF-TGT-208 every scaled ARM64 lookup must be exact");
            }
            else
            {
                const TargetInfo target = x64At(kUnknownLevel);
                context.expect(target.cpuBaselineName().empty() && target.llvmCpuName().empty() &&
                    target.llvmFeatureString().empty(),
                    "TF-TGT-208 every scaled unknown lookup must remain empty");
            }
        }
    });
}
}

void registerTargetMachineStringsModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvalidAndBoundaryCases(registry);
    registerVariantMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
