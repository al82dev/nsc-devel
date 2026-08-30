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
}
