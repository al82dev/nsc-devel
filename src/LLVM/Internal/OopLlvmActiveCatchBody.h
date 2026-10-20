#pragma once

#include "../LlvmCApi.h"

#include <cstddef>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

// CC1 EH-001. One entry per `catch` clause body currently being lowered, innermost last.
// Deliberately NOT a field on FuncletContext: since the B-006/B-007 fix, clause bodies are
// ordinary parent blocks and nothing ever pushes a FuncletKind::Catch entry onto funcletStack_
// any more (the only push site builds Cleanup entries), so an obligation hung there would be
// unreachable code that closes the finding on paper and measures nothing.
struct ActiveCatchBody final
{
    LLVMValueRef exception = nullptr;
    std::size_t loopDepthAtEntry = 0;
};
}
