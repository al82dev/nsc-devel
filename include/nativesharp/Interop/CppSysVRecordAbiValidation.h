#pragma once

#include "nativesharp/Interop/CppAbiPlanQueries.h"
#include "nativesharp/Interop/CppType.h"

namespace nativesharp::interop::abi_plan
{
// Physical and declared identity only; callers must also establish provenance,
// observed layout and C++ trivial-value eligibility.
inline bool isSysVInt32IndirectRecordRoundTrip(const Plan& plan, std::uint64_t sizeBits) noexcept
{
    const bool unary = isSysVIndirectRecordRoundTrip(plan, "__nsc_pointer_abi_probe");
    if (sizeBits < 160 || sizeBits % 32 != 0 || plan.logicalReturn.alignment != 4 ||
        (!unary && !isSysVIndirectRecordWithArguments(plan, "__nsc_pointer_abi_probe")) ||
        plan.logicalParameters.empty() || plan.logicalParameters[0].passing != Passing::Indirect ||
        plan.logicalParameters[0].offset != 0)
        return false;
    for (std::size_t i = 1; i < plan.logicalParameters.size(); ++i)
    {
        const auto& logical = plan.logicalParameters[i];
        if (logical.passing != Passing::Direct || logical.physicalCount != 1 ||
            logical.firstPhysical >= plan.physicalParameters.size() ||
            plan.physicalParameters[logical.firstPhysical].type.kind != TypeKind::Integer ||
            plan.physicalParameters[logical.firstPhysical].type.widthOrCount != 32) return false;
    }
    const auto integerFields = [sizeBits](const Type& type) {
        if (type.kind != TypeKind::Structure || type.packed || type.widthOrCount ||
            type.elements.size() != sizeBits / 32) return false;
        for (const auto& field : type.elements)
            if (field.kind != TypeKind::Integer || field.widthOrCount != 32 || field.packed || !field.elements.empty()) return false;
        return true;
    };
    return integerFields(*plan.physicalParameters[0].pointee) && integerFields(*plan.physicalParameters[plan.logicalParameters[0].firstPhysical].pointee);
}
inline bool isSysVCppRecordPointerRoundTrip(const Plan& plan, const CppFunctionType& signature) noexcept
{
    const auto& result = signature.returnType;
    if (!signature.hasPrototype || signature.variadic || signature.callingConvention != "sysv_abi" ||
        signature.parameterTypes.empty() || result.kind != CppTypeKind::Record ||
        result.isConst || result.isVolatile || result.isRestrict || !result.recordIdentity ||
        !result.recordIdentity->clangUsr || result.recordIdentity->clangUsr->empty() ||
        result.canonicalSpelling.empty() || (!result.sizeBits || (*result.sizeBits != 64 && *result.sizeBits != 96 && *result.sizeBits != 128 && (*result.sizeBits < 160 || *result.sizeBits % 32 != 0))) ||
        result.alignmentBits != 32) return false;
    const auto& parameter = signature.parameterTypes[0];
    for (std::size_t i = 1; i < signature.parameterTypes.size(); ++i)
    {
        const auto& scalar = signature.parameterTypes[i];
        if (scalar.kind != CppTypeKind::SignedInteger || scalar.canonicalSpelling != "int" ||
            scalar.sizeBits != 32 || scalar.alignmentBits != 32 ||
            scalar.isConst || scalar.isVolatile || scalar.isRestrict) return false;
    }
    if (plan.logicalParameters.size() != signature.parameterTypes.size()) return false;

    return parameter.kind == CppTypeKind::Record && !parameter.isConst && !parameter.isVolatile &&
        !parameter.isRestrict && parameter.recordIdentity &&
        parameter.recordIdentity->clangUsr == result.recordIdentity->clangUsr &&
        parameter.canonicalSpelling == result.canonicalSpelling && parameter.sizeBits == result.sizeBits &&
        parameter.alignmentBits == result.alignmentBits &&
        ((signature.parameterTypes.size() == 1 && result.sizeBits == 64 && plan.physicalReturn.widthOrCount == 64 &&
          isSysVSmallRecordRoundTrip(plan, "__nsc_pointer_abi_probe")) ||
         (signature.parameterTypes.size() == 1 && (result.sizeBits == 96 || result.sizeBits == 128) &&
          isSysVTwoIntegerRecordRoundTrip(plan, "__nsc_pointer_abi_probe") &&
          plan.physicalReturn.elements[1].widthOrCount == (result.sizeBits == 96 ? 32 : 64)) ||
         isSysVInt32IndirectRecordRoundTrip(plan, *result.sizeBits));
}
}
