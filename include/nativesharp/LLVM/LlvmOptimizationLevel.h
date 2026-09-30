#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
enum class LlvmOptimizationLevel
{
    O0,
    O1,
    O2,
    O3
};

std::string_view llvmOptimizationLevelName(LlvmOptimizationLevel level) noexcept;
std::string_view llvmOptimizationPipelineName(LlvmOptimizationLevel level) noexcept;
std::uint32_t llvmOptimizationPipelineVersion(LlvmOptimizationLevel level) noexcept;
std::string llvmOptimizationContractManifest();
}
