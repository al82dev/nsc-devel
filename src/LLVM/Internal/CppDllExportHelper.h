#pragma once

#include "../LlvmCApi.h"

namespace nativesharp::llvm_backend
{
// Returns an internal IR helper that loads the named sibling DLL and resolves an export.
llvm_api::LLVMValueRef cppDllTlsExportHelper(
    llvm_api::LLVMContextRef context, llvm_api::LLVMModuleRef module);
}
