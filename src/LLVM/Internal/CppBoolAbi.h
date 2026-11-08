#pragma once

#include "../LlvmCApi.h"
#include "nativesharp/Frontend/ProjectOopMirFunction.h"

namespace nativesharp::llvm_backend
{
template <typename AddAttribute>
void applyBoolZeroExtension(llvm_api::LLVMContextRef context, unsigned index,
                            AddAttribute&& addAttribute)
{
    const unsigned kind = llvm_api::LLVMGetEnumAttributeKindForName("zeroext", 7);
    if (kind == 0) return;
    const auto attribute = llvm_api::LLVMCreateEnumAttribute(context, kind, 0);
    if (attribute != nullptr) addAttribute(index, attribute);
}

// The Win64 Clang ABI attaches zeroext to C/C++ bool values, including call sites.
// LLVM attribute indices use zero for the return and physical parameter index + 1.
template <typename AddAttribute>
void applyExternalBoolAbi(llvm_api::LLVMContextRef context,
                          const frontend::ProjectOopMirFunction* function,
                          AddAttribute&& addAttribute)
{
    if (function == nullptr || !(function->isImportedC || function->isCppExternal)) return;
    const unsigned kind = llvm_api::LLVMGetEnumAttributeKindForName("zeroext", 7);
    if (kind == 0) return;
    const auto attribute = llvm_api::LLVMCreateEnumAttribute(context, kind, 0);
    if (attribute == nullptr) return;
    if (!function->referenceReturn && !function->constructor &&
        function->returnTypeKey == "builtin:bool")
        addAttribute(0, attribute);
    for (std::size_t index = 0; index < function->parameterTypeKeys.size(); ++index)
    {
        if (function->parameterTypeKeys[index] != "builtin:bool" ||
            (index < function->parameterPassKinds.size() &&
             function->parameterPassKinds[index] != frontend::ParameterPassKind::Value) ||
            index >= function->abi.parameterPhysicalOrdinals.size())
            continue;
        addAttribute(static_cast<unsigned>(function->abi.parameterPhysicalOrdinals[index] + 1), attribute);
    }
}
}
