#pragma once

namespace nativesharp::llvm_api
{
enum LLVMTailCallKind
{
    LLVMTailCallKindNone = 0,
    LLVMTailCallKindTail = 1,
    LLVMTailCallKindMustTail = 2,
    LLVMTailCallKindNoTail = 3,
};
}
