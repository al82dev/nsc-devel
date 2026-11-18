#pragma once

#include "../LlvmCApi.h"
#include "nativesharp/Interop/CppAbiPlan.h"

#include <algorithm>
#include <vector>

namespace nativesharp::llvm_backend
{
// EN: T19-028. A swiftcall or swiftasynccall import whose Clang plan passes or returns a record by value: its
//     declaration and calls are built from the plan's physical types (isSwiftRecordPlan checked them in HIR).
// ES: T19-028. Una importacion swiftcall o swiftasynccall cuyo plan de Clang pasa o devuelve un record por valor: su
//     declaracion y sus llamadas se construyen con los tipos fisicos del plan (isSwiftRecordPlan los comprobo en HIR).
inline bool usesSwiftRecordPlan(const interop::abi_plan::Plan& plan) noexcept
{
    using interop::abi_plan::Passing;
    if (plan.variadic || (plan.callingConvention != 16U && plan.callingConvention != 20U)) return false;
    return plan.logicalReturn.passing == Passing::Expand || plan.logicalReturn.passing == Passing::Indirect ||
        plan.physicalReturn.kind == interop::abi_plan::TypeKind::Structure ||
        std::any_of(plan.logicalParameters.begin(), plan.logicalParameters.end(), [](const auto& value) {
            return value.passing == Passing::Expand || value.passing == Passing::Indirect;
        });
}

inline llvm_api::LLVMTypeRef swiftPlanLlvmType(llvm_api::LLVMContextRef context,
                                               const interop::abi_plan::Type& type, unsigned depth = 0)
{
    using namespace llvm_api;
    using interop::abi_plan::TypeKind;
    if (depth > 8) return nullptr;
    switch (type.kind)
    {
        case TypeKind::Void: return LLVMVoidTypeInContext(context);
        case TypeKind::Integer:
            return type.widthOrCount ? LLVMIntTypeInContext(context, static_cast<unsigned>(type.widthOrCount)) : nullptr;
        case TypeKind::Float: return LLVMFloatTypeInContext(context);
        case TypeKind::Double: return LLVMDoubleTypeInContext(context);
        case TypeKind::Pointer: return LLVMPointerTypeInContext(context, 0);
        case TypeKind::Structure:
        {
            std::vector<LLVMTypeRef> elements{};
            for (const auto& element : type.elements)
            {
                auto* converted = swiftPlanLlvmType(context, element, depth + 1);
                if (!converted) return nullptr;
                elements.push_back(converted);
            }
            return LLVMStructTypeInContext(context, elements.empty() ? nullptr : elements.data(),
                                           static_cast<unsigned>(elements.size()), type.packed ? 1 : 0);
        }
        default: return nullptr;
    }
}

inline llvm_api::LLVMTypeRef swiftRecordFunctionType(llvm_api::LLVMContextRef context,
                                                     const interop::abi_plan::Plan& plan)
{
    using namespace llvm_api;
    auto* result = swiftPlanLlvmType(context, plan.physicalReturn);
    if (!result) return nullptr;
    std::vector<LLVMTypeRef> parameters;
    for (const auto& parameter : plan.physicalParameters)
    {
        auto* converted = swiftPlanLlvmType(context, parameter.type);
        if (!converted || converted == LLVMVoidTypeInContext(context)) return nullptr;
        parameters.push_back(converted);
    }
    return LLVMFunctionType(result, parameters.empty() ? nullptr : parameters.data(),
                            static_cast<unsigned>(parameters.size()), 0);
}
}
