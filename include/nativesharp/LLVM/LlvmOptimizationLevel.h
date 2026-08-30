#pragma once

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
}
