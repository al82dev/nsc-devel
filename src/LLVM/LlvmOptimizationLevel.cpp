#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

namespace nativesharp::llvm_backend
{
std::string_view llvmOptimizationLevelName(LlvmOptimizationLevel level) noexcept
{
    switch (level)
    {
        case LlvmOptimizationLevel::O0:
            return "O0";
        case LlvmOptimizationLevel::O1:
            return "O1";
        case LlvmOptimizationLevel::O2:
            return "O2";
        case LlvmOptimizationLevel::O3:
            return "O3";
    }
    return {};
}

std::string_view llvmOptimizationPipelineName(LlvmOptimizationLevel level) noexcept
{
    switch (level)
    {
        case LlvmOptimizationLevel::O0:
            return "default<O0>";
        case LlvmOptimizationLevel::O1:
            return "default<O1>";
        case LlvmOptimizationLevel::O2:
            return "default<O2>";
        case LlvmOptimizationLevel::O3:
            return "default<O3>";
    }
    return {};
}

std::uint32_t llvmOptimizationPipelineVersion(LlvmOptimizationLevel level) noexcept
{
    switch (level)
    {
        case LlvmOptimizationLevel::O0:
        case LlvmOptimizationLevel::O1:
        case LlvmOptimizationLevel::O2:
        case LlvmOptimizationLevel::O3:
            return 1;
    }
    return 0;
}

std::string llvmOptimizationContractManifest()
{
    std::string result = "NSOPTIMIZATION 1.0\n";
    result += "target=windows-x64\n";
    result += "abi-policy=stable-across-levels\n";
    result += "observable-behavior=equivalent-unless-explicit-fast-contract\n";
    result += "cache-key=pipeline-name-and-version\n";
    constexpr LlvmOptimizationLevel levels[] = {
        LlvmOptimizationLevel::O0, LlvmOptimizationLevel::O1,
        LlvmOptimizationLevel::O2, LlvmOptimizationLevel::O3};
    constexpr std::string_view intents[] = {
        "fast-debug", "cheap-local-debuggable", "balanced-release", "aggressive-budgeted"};
    for (std::size_t index = 0; index < 4; ++index)
    {
        result += std::string(llvmOptimizationLevelName(levels[index])) +
            " pipeline=" + std::string(llvmOptimizationPipelineName(levels[index])) +
            " version=" + std::to_string(llvmOptimizationPipelineVersion(levels[index])) +
            " intent=" + std::string(intents[index]) + "\n";
    }
    return result;
}
}
