#pragma once

#include "../LlvmCApi.h"

#include <cstddef>
#include <string>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

// One frame per construct that `break` or `continue` can name. A loop pushes a frame whose two
// targets are its own; a SWITCH pushes one whose break target is the switch end and whose
// CONTINUE TARGET IS INHERITED from the enclosing loop, so `continue` inside a case continues
// that loop. That inheritance is why the funclet depth is stored twice: the break target sits
// at the depth of the construct that owns it, while the continue target sits at the depth of a
// loop that may be several funclets further out.
struct LoopTargets final
{
    LLVMBasicBlockRef breakTarget = nullptr;
    LLVMBasicBlockRef continueTarget = nullptr;
    std::size_t funcletDepth = 0;
    std::size_t continueFuncletDepth = 0;
    std::string label;
    LLVMValueRef resultStorage = nullptr;
};
}
