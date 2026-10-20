#pragma once

#include "../LlvmCApi.h"

namespace nativesharp::frontend
{
struct ProjectOopMirNode;
}

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

struct StaticLocalStorage final
{
    const frontend::ProjectOopMirNode* declaration = nullptr;
    LLVMValueRef storage = nullptr;
    LLVMValueRef state = nullptr;
    LLVMValueRef owner = nullptr;
};
}
