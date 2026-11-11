#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"

namespace nativesharp::interop::abi_plan
{
// Only the observed two-integer SysV round trip; mixed register classes stay unsupported.
inline bool isSysVTwoIntegerRecordRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    const auto i64 = [](const Type& type) {
        return type.kind == TypeKind::Integer && type.widthOrCount == 64;
    };
    if (plan.physicalReturn.elements.size() != 2) return false;
    const auto secondBits = plan.physicalReturn.elements[1].widthOrCount;
    if (secondBits < 8 || secondBits > 64 || secondBits % 8 != 0) return false;
    const auto secondInteger = [secondBits](const Type& type) {
        return type.kind == TypeKind::Integer && type.widthOrCount == secondBits;
    };
    const auto pair = [&](const Type& type) {
        return type.kind == TypeKind::Structure && !type.packed && type.elements.size() == 2 &&
            i64(type.elements[0]) && secondInteger(type.elements[1]);
    };
    if (plan.symbol != symbol || plan.callingConvention != 78 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.returnSignExtend || plan.returnZeroExtend || !pair(plan.physicalReturn) ||
        plan.logicalReturn.passing != Passing::Direct || !plan.logicalReturn.coercion ||
        !pair(*plan.logicalReturn.coercion) || plan.logicalReturn.offset != 0 ||
        plan.logicalParameters.size() != 1 || plan.physicalParameters.size() != 2) return false;
    const auto& logical = plan.logicalParameters[0];
    if (logical.passing != Passing::Direct || !logical.coercion || !pair(*logical.coercion) ||
        logical.offset != 0 || logical.firstPhysical != 0 || logical.physicalCount != 2) return false;
    for (std::size_t i = 0; i < plan.physicalParameters.size(); ++i)
    {
        const auto& physical = plan.physicalParameters[i];
        if (!(i == 0 ? i64(physical.type) : secondInteger(physical.type)) || physical.structureReturn ||
            physical.byValue || physical.signExtend || physical.zeroExtend || physical.inRegister ||
            physical.noAlias) return false;
    }
    return true;
}
inline bool isSysVTwoI64RecordRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    return isSysVTwoIntegerRecordRoundTrip(plan, symbol) &&
        plan.physicalReturn.elements[1].widthOrCount == 64;
}
inline bool isSysVIndirectRecordRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    const auto alignment = plan.logicalReturn.alignment;
    if (plan.symbol != symbol || plan.callingConvention != 78 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.returnSignExtend || plan.returnZeroExtend || plan.physicalReturn.kind != TypeKind::Void ||
        plan.logicalReturn.passing != Passing::Indirect ||
        (alignment != 1 && alignment != 4 && alignment != 8) ||
        plan.logicalParameters.size() != 1 || plan.physicalParameters.size() != 2) return false;
    const auto& logical = plan.logicalParameters[0];
    const auto& result = plan.physicalParameters[0];
    const auto& argument = plan.physicalParameters[1];
    return logical.passing == Passing::Indirect && logical.byValue && logical.alignment == 8 &&
        logical.firstPhysical == 1 && logical.physicalCount == 1 &&
        result.type.kind == TypeKind::Pointer && result.pointee && result.structureReturn &&
        result.alignment == alignment && !result.byValue && !result.inRegister && !result.signExtend && !result.zeroExtend &&
        argument.type.kind == TypeKind::Pointer && argument.pointee && argument.byValue && argument.alignment == 8 &&
        !argument.structureReturn && !argument.noAlias && !argument.inRegister && !argument.signExtend &&
        !argument.zeroExtend;
}
}
