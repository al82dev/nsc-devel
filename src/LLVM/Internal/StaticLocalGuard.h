#pragma once

#include "../LlvmCApi.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

struct StaticLocalGuard final
{
    LLVMValueRef state = nullptr;
    LLVMBasicBlockRef done = nullptr;
    LLVMBasicBlockRef failed = nullptr;
    LLVMBasicBlockRef failureUnwind = nullptr;
};
}
