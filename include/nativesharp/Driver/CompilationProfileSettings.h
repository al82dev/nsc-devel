#pragma once

#include "nativesharp/LLVM/DebugInformationKind.h"
#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

namespace nativesharp::driver
{
struct CompilationProfileSettings final
{
    llvm_backend::LlvmOptimizationLevel optimizationLevel = llvm_backend::LlvmOptimizationLevel::O0;
    llvm_backend::DebugInformationKind debugInformation = llvm_backend::DebugInformationKind::None;
};
} // namespace nativesharp::driver
