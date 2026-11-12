#pragma once

namespace nativesharp::llvm_api
{
// LLVMGetTypeKind returns this C enum; only the fixed-vector kind is used here.
enum LLVMTypeKind : int
{
    LLVMVectorTypeKind = 13
};
}
