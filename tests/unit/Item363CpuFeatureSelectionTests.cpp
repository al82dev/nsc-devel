#include "nativesharp/LLVM/LlvmOptimizationLevel.h"
#include "nativesharp/LLVM/LlvmTargetMachine.h"
#include "nativesharp/Target/TargetInfo.h"

#include "../support/TestContext.h"

#include <memory>
#include <string>

int main()
{
    using nativesharp::llvm_backend::LlvmOptimizationLevel;
    using nativesharp::llvm_backend::LlvmTargetMachine;
    using nativesharp::target::CpuFeatureLevel;
    using nativesharp::target::TargetInfo;
    using nativesharp::tests::TestContext;

    TestContext tests;
    const TargetInfo avx2 = TargetInfo::WindowsX64(
        CpuFeatureLevel::X86_64_V3, CpuFeatureLevel::X86_64_V3);
    tests.expect(avx2.cpuBaselineName() == "x86-64-v3" &&
            avx2.supportsCpuFeature("avx2") && avx2.supportsCpuFeature("fma") &&
            !avx2.supportsCpuFeature("avx512f") && !avx2.usedCpuFeatureFallback(),
        "x86-64-v3 selects the AVX2/FMA feature set without claiming AVX-512");

    const TargetInfo fallback = TargetInfo::WindowsX64(
        CpuFeatureLevel::X86_64_V4, CpuFeatureLevel::X86_64_V2);
    tests.expect(fallback.requestedCpuFeatureLevel() == CpuFeatureLevel::X86_64_V4 &&
            fallback.cpuFeatureLevel() == CpuFeatureLevel::X86_64_V2 &&
            fallback.cpuBaselineName() == "x86-64-v2" &&
            fallback.supportsCpuFeature("sse4.2") && !fallback.supportsCpuFeature("avx") &&
            fallback.usedCpuFeatureFallback(),
        "selection falls back to the highest explicitly available CPU level");

    std::string error;
    std::unique_ptr<LlvmTargetMachine> machine = LlvmTargetMachine::createWindowsX64(
        LlvmOptimizationLevel::O2,
        CpuFeatureLevel::X86_64_V4,
        CpuFeatureLevel::X86_64_V3,
        error);
    tests.expect(machine != nullptr && error.empty() && machine->cpuName() == "x86-64-v3" &&
            machine->featureString().find("+avx2") != std::string::npos &&
            machine->usedCpuFeatureFallback(),
        "LLVM TargetMachine receives the selected fallback CPU and feature string (" + error + ")");

    const TargetInfo baseline = TargetInfo::WindowsX64();
    tests.expect(baseline.cpuBaselineName() == "x86-64-v1" &&
            baseline.llvmCpuName() == "x86-64" && !baseline.supportsCpuFeature("avx"),
        "the default remains the reproducible x86-64-v1 baseline");
    return tests.result();
}
