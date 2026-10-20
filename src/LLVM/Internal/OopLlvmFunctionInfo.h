#pragma once

#include "../LlvmCApi.h"

#include "nativesharp/Language/FrameStorage.h"

#include <cstdint>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

struct FunctionInfo final
{
    LLVMValueRef function = nullptr;
    LLVMTypeRef functionType = nullptr;
    LLVMMetadataRef subprogram = nullptr;
    std::uint64_t asyncFrameSize = 0;
    language::FrameStorage asyncFrameStorage = language::FrameStorage::CallerStack;
};
}
