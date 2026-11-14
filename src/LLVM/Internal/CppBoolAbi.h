#pragma once

#include "../LlvmCApi.h"
#include "nativesharp/Frontend/ProjectOopMirFunction.h"
#include <string_view>

namespace nativesharp::llvm_backend
{
inline unsigned cppPhysicalAttributeIndex(const frontend::ProjectOopMirFunction& function, std::size_t parameter)
{
    std::size_t hidden = 0;
    for (std::size_t i = 0; i < parameter && i < function.cppObjectSizeModes.size(); ++i)
        if (function.cppObjectSizeModes[i]) ++hidden;
    return static_cast<unsigned>(function.abi.parameterPhysicalOrdinals[parameter] + hidden + 1);
}

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
        addAttribute(cppPhysicalAttributeIndex(*function, index), attribute);
    }
}

// SysV x86-64 requires the signedness-specific extension on narrow C integers.
template <typename AddAttribute>
void applySysVIntegerExtension(llvm_api::LLVMContextRef context,
                               const frontend::ProjectOopMirFunction* function,
                               AddAttribute&& addAttribute)
{
    if (function == nullptr || !function->cppSysVCall) return;
    const auto apply = [&](std::string_view type, unsigned index)
    {
        const std::string_view name = type == "builtin:sbyte" || type == "builtin:short" ? "signext" :
            type == "builtin:byte" || type == "builtin:ushort" ? "zeroext" : "";
        if (name.empty()) return;
        const unsigned kind = llvm_api::LLVMGetEnumAttributeKindForName(name.data(), name.size());
        if (kind == 0) return;
        const auto attribute = llvm_api::LLVMCreateEnumAttribute(context, kind, 0);
        if (attribute != nullptr) addAttribute(index, attribute);
    };
    if (!function->referenceReturn && !function->constructor) apply(function->returnTypeKey, 0);
    for (std::size_t index = 0; index < function->parameterTypeKeys.size(); ++index)
    {
        if ((index < function->parameterPassKinds.size() &&
             function->parameterPassKinds[index] != frontend::ParameterPassKind::Value) ||
            index >= function->abi.parameterPhysicalOrdinals.size()) continue;
        apply(function->parameterTypeKeys[index], cppPhysicalAttributeIndex(*function, index));
    }
}
}
