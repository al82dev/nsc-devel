#pragma once

#include "OopLlvmFuncletKind.h"
#include "../LlvmCApi.h"

#include <cstddef>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

struct FuncletContext final
{
    LLVMValueRef pad = nullptr;
    FuncletKind kind = FuncletKind::Catch;
    std::size_t cleanupDepthAtEntry = 0;
    LLVMValueRef caughtException = nullptr;
    LLVMBasicBlockRef unwindDestination = nullptr;
};
}
