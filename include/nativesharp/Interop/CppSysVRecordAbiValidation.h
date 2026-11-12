#pragma once

#include "nativesharp/Interop/CppAbiPlanQueries.h"
#include "nativesharp/Interop/CppType.h"

namespace nativesharp::interop::abi_plan
{
// Physical and declared identity only; callers must also establish provenance,
// observed layout and C++ trivial-value eligibility.
inline bool isSysVCppRecordPointerRoundTrip(const Plan& plan, const CppFunctionType& signature) noexcept
{
    const auto& result = signature.returnType;
    if (!signature.hasPrototype || signature.variadic || signature.callingConvention != "sysv_abi" ||
        signature.parameterTypes.size() != 1 || result.kind != CppTypeKind::Record ||
        result.isConst || result.isVolatile || result.isRestrict || !result.recordIdentity ||
        !result.recordIdentity->clangUsr || result.recordIdentity->clangUsr->empty() ||
        result.canonicalSpelling.empty() || (result.sizeBits != 64 && result.sizeBits != 96 && result.sizeBits != 128) ||
        result.alignmentBits != 32) return false;
    const auto& parameter = signature.parameterTypes[0];
    return parameter.kind == CppTypeKind::Record && !parameter.isConst && !parameter.isVolatile &&
        !parameter.isRestrict && parameter.recordIdentity &&
        parameter.recordIdentity->clangUsr == result.recordIdentity->clangUsr &&
        parameter.canonicalSpelling == result.canonicalSpelling && parameter.sizeBits == result.sizeBits &&
        parameter.alignmentBits == result.alignmentBits &&
        ((result.sizeBits == 64 && plan.physicalReturn.widthOrCount == 64 &&
          isSysVSmallRecordRoundTrip(plan, "__nsc_pointer_abi_probe")) ||
         ((result.sizeBits == 96 || result.sizeBits == 128) &&
          isSysVTwoIntegerRecordRoundTrip(plan, "__nsc_pointer_abi_probe") &&
          plan.physicalReturn.elements[1].widthOrCount == (result.sizeBits == 96 ? 32 : 64)));
}
}
