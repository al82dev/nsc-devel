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
        plan.logicalParameters.empty()) return false;
    const auto integerFields = [sizeBits](const Type& type) {
        if (type.kind != TypeKind::Structure || type.packed || type.widthOrCount ||
            type.elements.size() != sizeBits / 32) return false;
        for (const auto& field : type.elements)
            if (field.kind != TypeKind::Integer || field.widthOrCount != 32 || field.packed || !field.elements.empty()) return false;
        return true;
    };
    if (!integerFields(*plan.physicalParameters[0].pointee)) return false;
    std::size_t recordCount = 0;
    for (const auto& logical : plan.logicalParameters)
    {
        if (logical.passing == Passing::Indirect)
        {
            if (logical.offset != 0 || logical.firstPhysical >= plan.physicalParameters.size() ||
                !plan.physicalParameters[logical.firstPhysical].pointee ||
                !integerFields(*plan.physicalParameters[logical.firstPhysical].pointee)) return false;
            ++recordCount;
            continue;
        }
        if (logical.passing != Passing::Direct || logical.physicalCount != 1 ||
            logical.firstPhysical >= plan.physicalParameters.size() ||
            plan.physicalParameters[logical.firstPhysical].type.kind != TypeKind::Integer ||
            plan.physicalParameters[logical.firstPhysical].type.widthOrCount != 32) return false;
    }
    return recordCount != 0;
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
    if (plan.logicalParameters.size() != signature.parameterTypes.size()) return false;
    std::size_t recordCount = 0;
    for (std::size_t i = 0; i < signature.parameterTypes.size(); ++i)
    {
        const auto& type = signature.parameterTypes[i];
        if (type.kind == CppTypeKind::Record)
        {
            if (type.isConst || type.isVolatile || type.isRestrict || !type.recordIdentity ||
                type.recordIdentity->clangUsr != result.recordIdentity->clangUsr ||
                type.canonicalSpelling != result.canonicalSpelling || type.sizeBits != result.sizeBits ||
                type.alignmentBits != result.alignmentBits ||
                (*result.sizeBits >= 160 && plan.logicalParameters[i].passing != Passing::Indirect)) return false;
            ++recordCount;
            continue;
        }
        if (plan.logicalParameters[i].passing != Passing::Direct || type.kind != CppTypeKind::SignedInteger || type.canonicalSpelling != "int" ||
            type.sizeBits != 32 || type.alignmentBits != 32 ||
            type.isConst || type.isVolatile || type.isRestrict) return false;
    }
    if (recordCount == 0) return false;
    return ((signature.parameterTypes.size() == 1 && result.sizeBits == 64 && plan.physicalReturn.widthOrCount == 64 &&
          isSysVSmallRecordRoundTrip(plan, "__nsc_pointer_abi_probe")) ||
         (signature.parameterTypes.size() == 1 && (result.sizeBits == 96 || result.sizeBits == 128) &&
          isSysVTwoIntegerRecordRoundTrip(plan, "__nsc_pointer_abi_probe") &&
          plan.physicalReturn.elements[1].widthOrCount == (result.sizeBits == 96 ? 32 : 64)) ||
         isSysVInt32IndirectRecordRoundTrip(plan, *result.sizeBits));
}
}
