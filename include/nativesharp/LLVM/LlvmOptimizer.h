#pragma once

#include "nativesharp/LLVM/LlvmOptimizationLevel.h"

#include <string>

namespace nativesharp::llvm_backend
{
class LlvmModule;
class LlvmTargetMachine;

class LlvmOptimizer final
{
public:
    bool optimize(
        LlvmModule& module,
        const LlvmTargetMachine& targetMachine,
        LlvmOptimizationLevel level,
        std::string& error) const;
};
}
