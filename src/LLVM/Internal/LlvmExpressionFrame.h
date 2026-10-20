#pragma once

#include "../LlvmCApi.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace nativesharp::frontend
{
struct ProjectOopMirNode;
}

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

struct LlvmExpressionFrame final
{
    const frontend::ProjectOopMirNode* node = nullptr;
    std::uint8_t phase = 0;     // 0 = evaluate the operand, 1 = compute.
    LLVMValueRef lhs = nullptr; // the left operand (binary) / the retained depth (cleanup).
    std::size_t retainedDepth = 0; // the cleanup depth before the wrapped child.
    std::string overflowPolicy = {}; // the saved overflow policy (OverflowPolicy).
    LLVMValueRef auxiliary = nullptr;
    LLVMTypeRef resultType = nullptr;
    LLVMBasicBlockRef nullBlock = nullptr;
    LLVMBasicBlockRef endBlock = nullptr;
};
}
