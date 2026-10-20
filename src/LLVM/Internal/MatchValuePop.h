#pragma once

#include "../LlvmCApi.h"

#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

struct MatchValuePop final
{
    std::vector<std::pair<LLVMValueRef, LLVMValueRef>>& stack;
    ~MatchValuePop();
};
}
