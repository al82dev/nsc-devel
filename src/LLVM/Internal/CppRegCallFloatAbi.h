#pragma once

#include "../LlvmCApi.h"
#include "nativesharp/Interop/CppAbiPlan.h"
#include <vector>

namespace nativesharp::llvm_backend
{
// Call only after the Clang plan has passed isRegCallWin64FloatRecordRoundTrip.
inline llvm_api::LLVMTypeRef regCallFloatRecordType(llvm_api::LLVMContextRef context,
                                                   const interop::abi_plan::Type& type, unsigned depth = 0)
{
    using interop::abi_plan::TypeKind;
    if (depth > 16) return nullptr;
    if (type.kind == TypeKind::Float) return llvm_api::LLVMFloatTypeInContext(context);
    if (type.kind == TypeKind::Array && type.elements.size() == 1)
    {
        auto* element = regCallFloatRecordType(context, type.elements[0], depth + 1);
        return element ? llvm_api::LLVMArrayType2(element, type.widthOrCount) : nullptr;
    }
    if (type.kind != TypeKind::Structure) return nullptr;
    std::vector<llvm_api::LLVMTypeRef> members;
    for (const auto& item : type.elements)
    {
        auto* member = regCallFloatRecordType(context, item, depth + 1);
        if (!member) return nullptr;
        members.push_back(member);
    }
    return llvm_api::LLVMStructTypeInContext(context, members.data(), static_cast<unsigned>(members.size()), 0);
}
}
