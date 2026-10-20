#pragma once

namespace nativesharp::llvm_api
{
enum LLVMFastMathFlags
{
    LLVMFastMathNone = 0,
    LLVMFastMathAllowReassoc = 1 << 0,
    LLVMFastMathNoNaNs = 1 << 1,
    LLVMFastMathNoInfs = 1 << 2,
    LLVMFastMathNoSignedZeros = 1 << 3,
    LLVMFastMathAllowReciprocal = 1 << 4,
    LLVMFastMathAllowContract = 1 << 5,
    LLVMFastMathApproxFunc = 1 << 6,
    LLVMFastMathFast = LLVMFastMathAllowReassoc | LLVMFastMathNoNaNs | LLVMFastMathNoInfs |
        LLVMFastMathNoSignedZeros | LLVMFastMathAllowReciprocal | LLVMFastMathAllowContract |
        LLVMFastMathApproxFunc
};
}
