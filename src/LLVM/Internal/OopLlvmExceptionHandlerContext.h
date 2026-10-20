#pragma once

#include "../LlvmCApi.h"

#include <cstddef>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

struct ExceptionHandlerContext final
{
    std::size_t regionOrdinal = 0;
    LLVMBasicBlockRef dispatchBlock = nullptr;
    std::size_t retainedCleanupDepth = 0;
    std::size_t funcletDepthAtEntry = 0;
};
}
