#pragma once

namespace nativesharp::llvm_api
{
// LLVMGetTypeKind returns this C enum; only the fixed-vector kind is used here.
enum LLVMTypeKind : int
{
    // EN: Struct and array kinds select memset zeroing of aggregate locals (LKF-293).
    // ES: Los tipos struct y arreglo eligen el memset que pone en cero agregados locales (LKF-293).
    LLVMStructTypeKind = 10,
    LLVMArrayTypeKind = 11,
    LLVMVectorTypeKind = 13
};
}
