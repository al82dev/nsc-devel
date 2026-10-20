#pragma once

namespace nativesharp::llvm_api
{
enum LLVMVerifierFailureAction
{
    LLVMAbortProcessAction = 0,
    LLVMPrintMessageAction = 1,
    LLVMReturnStatusAction = 2
};
}
