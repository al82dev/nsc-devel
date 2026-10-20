#pragma once

namespace nativesharp::llvm_api
{
enum LLVMCodeGenOptLevel
{
    LLVMCodeGenLevelNone = 0,
    LLVMCodeGenLevelLess = 1,
    LLVMCodeGenLevelDefault = 2,
    LLVMCodeGenLevelAggressive = 3
};
}
