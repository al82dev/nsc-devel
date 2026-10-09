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

struct ExpectedLlvmIdentity
{
    std::string_view triple;
    std::string_view abi;
};

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CF-TGT-025", "Host.Target", "unit",
        std::move(profile), {"target", "llvm-identity"}, std::move(function)});
}

void expectLlvmIdentity(TestContext& context, const TargetInfo& target,
    ExpectedLlvmIdentity expected, std::string prefix)
{
    context.expect(target.llvmTriple() == expected.triple, prefix + " LLVM triple must be exact");
    context.expect(target.abiName() == expected.abi, prefix + " ABI name must be exact");
}

bool sameLlvmIdentity(const TargetInfo& left, const TargetInfo& right)
{
    return left.llvmTriple() == right.llvmTriple() && left.abiName() == right.abiName();
}

void registerNominalCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-193", "PR", [](TestContext& context) {
        expectLlvmIdentity(context, TargetInfo::WindowsX64(),
            {"x86_64-pc-windows-msvc", "Microsoft Windows x64 ABI"}, "TF-TGT-193 x64");
    });
    addCase(registry, "TF-TGT-194", "PR", [](TestContext& context) {
        expectLlvmIdentity(context, TargetInfo::WindowsArm64(),
            {"aarch64-pc-windows-msvc", "Microsoft Windows ARM64 ABI"}, "TF-TGT-194 ARM64");
    });
    addCase(registry, "TF-TGT-195", "PR", [](TestContext& context) {
        const TargetInfo fresh = TargetInfo::WindowsX64();
        expectLlvmIdentity(context, fresh,
            {"x86_64-pc-windows-msvc", "Microsoft Windows x64 ABI"}, "TF-TGT-195 fresh target");
    });
}

void registerInvariantAndBoundaryCases(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-196", "PR", [](TestContext& context) {
        const TargetInfo constrained =
            TargetInfo::WindowsX64(CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::Baseline);
        expectLlvmIdentity(context, constrained,
            {"x86_64-pc-windows-msvc", "Microsoft Windows x64 ABI"},
            "TF-TGT-196 CPU-constrained x64");
    });
    addCase(registry, "TF-TGT-197", "PR", [](TestContext& context) {
        const TargetInfo x64 = TargetInfo::WindowsX64();
        const TargetInfo arm64 = TargetInfo::WindowsArm64();
        context.expect(x64.llvmTriple().size() == 22 && arm64.llvmTriple().size() == 23,
            "TF-TGT-197 LLVM triple lengths must be exact");
        context.expect(x64.abiName().size() == 25 && arm64.abiName().size() == 27,
            "TF-TGT-197 ABI name lengths must be exact");
        context.expect(!x64.llvmTriple().empty() && !arm64.llvmTriple().empty() &&
            !x64.abiName().empty() && !arm64.abiName().empty(),
            "TF-TGT-197 target-machine identity strings must not be empty");
    });
}

void registerVariantMatrixCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-198", "PR", [](TestContext& context) {
        for (const CpuFeatureLevel preferred : kLevels)
            for (const CpuFeatureLevel available : kLevels)
                expectLlvmIdentity(context, TargetInfo::WindowsX64(preferred, available),
                    {"x86_64-pc-windows-msvc", "Microsoft Windows x64 ABI"},
                    "TF-TGT-198 x64 matrix entry");
        expectLlvmIdentity(context, TargetInfo::WindowsArm64(),
            {"aarch64-pc-windows-msvc", "Microsoft Windows ARM64 ABI"},
            "TF-TGT-198 ARM64 variant");
    });
}

void registerDeterminismCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-199", "PR", [](TestContext& context) {
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
            context.expect(sameLlvmIdentity(forward[index], reverse[index]) &&
                sameLlvmIdentity(forward[index],
                    TargetInfo::WindowsX64(kLevels[index], CpuFeatureLevel::X86_64_V4)),
                "TF-TGT-199 LLVM identity must be independent of order and repetition");
        }
        context.expect(sameLlvmIdentity(TargetInfo::WindowsArm64(), TargetInfo::WindowsArm64()),
            "TF-TGT-199 ARM64 LLVM identity must be deterministic");
    });
}

void registerScaleCase(TestRegistry& registry)
{
    addCase(registry, "TF-TGT-200", "NIGHTLY", [](TestContext& context) {
        constexpr std::size_t iterationCount = 65536;
        for (std::size_t index = 0; index < iterationCount; ++index)
        {
            if ((index & 1U) == 0)
            {
                const TargetInfo target = TargetInfo::WindowsX64(
                    kLevels[index % kLevels.size()], kLevels[(index / kLevels.size()) % kLevels.size()]);
                context.expect(target.llvmTriple() == "x86_64-pc-windows-msvc" &&
                    target.abiName() == "Microsoft Windows x64 ABI",
                    "TF-TGT-200 every scaled x64 LLVM identity must be exact");
            }
            else
            {
                const TargetInfo target = TargetInfo::WindowsArm64();
                context.expect(target.llvmTriple() == "aarch64-pc-windows-msvc" &&
                    target.abiName() == "Microsoft Windows ARM64 ABI",
                    "TF-TGT-200 every scaled ARM64 LLVM identity must be exact");
            }
        }
    });
}
}

void registerTargetLlvmIdentityModule(TestRegistry& registry)
{
    registerNominalCases(registry);
    registerInvariantAndBoundaryCases(registry);
    registerVariantMatrixCase(registry);
    registerDeterminismCase(registry);
    registerScaleCase(registry);
}
}
