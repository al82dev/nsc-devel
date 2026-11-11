#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"

namespace nativesharp::interop::abi_plan
{
inline bool isSysVSingleFloatingRecordRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    const auto kind = plan.physicalReturn.kind;
    if ((kind != TypeKind::Float && kind != TypeKind::Double) || plan.symbol != symbol ||
        plan.callingConvention != 78 || plan.variadic || !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") ||
        !plan.dataLayout.starts_with("e-") || plan.returnSignExtend || plan.returnZeroExtend ||
        plan.logicalReturn.passing != Passing::Direct || !plan.logicalReturn.coercion ||
        plan.logicalReturn.coercion->kind != kind || plan.logicalReturn.offset != 0 ||
        plan.logicalParameters.size() != 1 || plan.physicalParameters.size() != 1) return false;
    const auto& logical = plan.logicalParameters[0];
    const auto& physical = plan.physicalParameters[0];
    return logical.passing == Passing::Direct && logical.coercion && logical.coercion->kind == kind &&
        logical.offset == 0 && logical.firstPhysical == 0 && logical.physicalCount == 1 &&
        physical.type.kind == kind && !physical.structureReturn && !physical.byValue &&
        !physical.signExtend && !physical.zeroExtend && !physical.inRegister && !physical.noAlias;
}
inline bool isSysVSingleVector2FRecordRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    const auto vector2f = [](const Type& value) {
        return value.kind == TypeKind::Vector && value.widthOrCount == 2 &&
               value.elements.size() == 1 && value.elements[0].kind == TypeKind::Float;
    };
    if (plan.symbol != symbol || plan.callingConvention != 78 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.returnSignExtend || plan.returnZeroExtend || !vector2f(plan.physicalReturn) ||
        plan.logicalReturn.passing != Passing::Direct || !plan.logicalReturn.coercion ||
        !vector2f(*plan.logicalReturn.coercion) || plan.logicalReturn.offset != 0 ||
        plan.logicalParameters.size() != 1 || plan.physicalParameters.size() != 1) return false;
    const auto& logical = plan.logicalParameters[0];
    const auto& physical = plan.physicalParameters[0];
    return logical.passing == Passing::Direct && logical.coercion && vector2f(*logical.coercion) &&
        logical.offset == 0 && logical.firstPhysical == 0 && logical.physicalCount == 1 &&
        vector2f(physical.type) && !physical.structureReturn && !physical.byValue &&
        !physical.signExtend && !physical.zeroExtend && !physical.inRegister && !physical.noAlias;
}
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
inline bool isSysVMixedRecordRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    const auto pair = [](const Type& type) {
        if (type.kind != TypeKind::Structure || type.packed || type.elements.size() != 2) return false;
        const auto& first = type.elements[0];
        const auto& second = type.elements[1];
        const auto vector2f = [](const Type& value) {
            return value.kind == TypeKind::Vector && value.widthOrCount == 2 &&
                   value.elements.size() == 1 && value.elements[0].kind == TypeKind::Float;
        };
        return (first.kind == TypeKind::Double && second.kind == TypeKind::Integer && second.widthOrCount == 32) ||
               (first.kind == TypeKind::Double && second.kind == TypeKind::Double) ||
               (first.kind == TypeKind::Double && second.kind == TypeKind::Float) ||
               (first.kind == TypeKind::Float && second.kind == TypeKind::Double) ||
               (first.kind == TypeKind::Double && vector2f(second)) ||
               (first.kind == TypeKind::Integer && first.widthOrCount == 64 &&
                (second.kind == TypeKind::Double || second.kind == TypeKind::Float || vector2f(second))) ||
               (vector2f(first) && vector2f(second)) ||
               (vector2f(first) && second.kind == TypeKind::Float) ||
               (vector2f(first) && second.kind == TypeKind::Double) ||
               (vector2f(first) && second.kind == TypeKind::Integer &&
                (second.widthOrCount == 32 || second.widthOrCount == 64));
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
    const auto& first = plan.physicalReturn.elements[0];
    const auto& second = plan.physicalReturn.elements[1];
    if (plan.physicalParameters[0].type.kind != first.kind ||
        plan.physicalParameters[0].type.widthOrCount != first.widthOrCount ||
        (first.kind == TypeKind::Vector && (plan.physicalParameters[0].type.elements.size() != 1 ||
            plan.physicalParameters[0].type.elements[0].kind != TypeKind::Float)) ||
        plan.physicalParameters[1].type.kind != second.kind ||
        plan.physicalParameters[1].type.widthOrCount != second.widthOrCount ||
        (second.kind == TypeKind::Vector && (plan.physicalParameters[1].type.elements.size() != 1 ||
            plan.physicalParameters[1].type.elements[0].kind != TypeKind::Float)) ||
        logical.coercion->elements[0].kind != first.kind || logical.coercion->elements[1].kind != second.kind ||
        plan.logicalReturn.coercion->elements[0].kind != first.kind ||
        plan.logicalReturn.coercion->elements[1].kind != second.kind) return false;
    for (const auto& physical : plan.physicalParameters)
        if (physical.structureReturn || physical.byValue || physical.signExtend || physical.zeroExtend ||
            physical.inRegister || physical.noAlias) return false;
    return true;
}
inline bool isSysVMixedDoubleI32RecordRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    return isSysVMixedRecordRoundTrip(plan, symbol) && plan.physicalReturn.elements[0].kind == TypeKind::Double &&
        plan.physicalReturn.elements[1].kind == TypeKind::Integer;
}
inline bool isSysVIndirectRecordRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    const auto alignment = plan.logicalReturn.alignment;
    if (plan.symbol != symbol || plan.callingConvention != 78 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.returnSignExtend || plan.returnZeroExtend || plan.physicalReturn.kind != TypeKind::Void ||
        plan.logicalReturn.passing != Passing::Indirect ||
        (alignment != 1 && alignment != 2 && alignment != 4 && alignment != 8 && alignment != 16 && alignment != 32) ||
        plan.logicalParameters.size() != 1 || plan.physicalParameters.size() != 2) return false;
    const auto& logical = plan.logicalParameters[0];
    const auto& result = plan.physicalParameters[0];
    const auto& argument = plan.physicalParameters[1];
    const auto argumentAlignment = alignment < 8 ? 8 : alignment;
    return logical.passing == Passing::Indirect && logical.byValue && logical.alignment == argumentAlignment &&
        logical.firstPhysical == 1 && logical.physicalCount == 1 &&
        result.type.kind == TypeKind::Pointer && result.pointee && result.structureReturn &&
        result.alignment == alignment && !result.byValue && !result.inRegister && !result.signExtend && !result.zeroExtend &&
        argument.type.kind == TypeKind::Pointer && argument.pointee && argument.byValue && argument.alignment == argumentAlignment &&
        !argument.structureReturn && !argument.noAlias && !argument.inRegister && !argument.signExtend &&
        !argument.zeroExtend;
}
}
