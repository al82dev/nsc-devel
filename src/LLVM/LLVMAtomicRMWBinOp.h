#pragma once

namespace nativesharp::llvm_api
{
enum LLVMAtomicRMWBinOp
{
    LLVMAtomicRMWBinOpXchg = 0,
    LLVMAtomicRMWBinOpAdd = 1,
    LLVMAtomicRMWBinOpAnd = 3,
    LLVMAtomicRMWBinOpOr = 5,
    LLVMAtomicRMWBinOpXor = 6
};
}
