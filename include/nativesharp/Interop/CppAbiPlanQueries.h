#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"

namespace nativesharp::interop::abi_plan
{
inline bool isSysVSmallRecordsRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    const auto integer = [](const Type& type) {
        return type.kind == TypeKind::Integer && type.widthOrCount >= 8 &&
            type.widthOrCount <= 64 && type.widthOrCount % 8 == 0;
    };
    if (plan.symbol != symbol || plan.callingConvention != 78 ||
        plan.variadic || !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") ||
        !plan.dataLayout.starts_with("e-") || plan.returnSignExtend || plan.returnZeroExtend ||
        !integer(plan.physicalReturn) || plan.logicalReturn.passing != Passing::Direct ||
        !plan.logicalReturn.coercion || !integer(*plan.logicalReturn.coercion) ||
        plan.logicalReturn.coercion->widthOrCount != plan.physicalReturn.widthOrCount || plan.logicalReturn.offset != 0 ||
        plan.logicalParameters.size() < 2 || plan.physicalParameters.size() != plan.logicalParameters.size()) return false;
    for (std::size_t i = 0; i < plan.logicalParameters.size(); ++i)
    {
        const auto& logical = plan.logicalParameters[i];
        const auto& physical = plan.physicalParameters[i];
        const bool directFloat = logical.coercion &&
            (logical.coercion->kind == TypeKind::Float || logical.coercion->kind == TypeKind::Double) &&
            physical.type.kind == logical.coercion->kind;
        const bool directPointer = logical.coercion && logical.coercion->kind == TypeKind::Pointer &&
            logical.coercion->widthOrCount == 0 && physical.type.kind == TypeKind::Pointer && physical.type.widthOrCount == 0;
        if (logical.passing != Passing::Direct || !logical.coercion ||
            (!integer(*logical.coercion) && !directFloat && !directPointer) ||
            logical.offset != 0 || logical.firstPhysical != i || logical.physicalCount != 1 ||
            (!integer(physical.type) && !directFloat && !directPointer) || logical.coercion->widthOrCount != physical.type.widthOrCount ||
            physical.structureReturn || physical.byValue || physical.signExtend ||
            physical.zeroExtend || physical.inRegister || physical.noAlias) return false;
    }
    return true;
}
inline bool isSysVTwoSmallRecordsRoundTrip(const Plan& plan, std::string_view symbol) noexcept
{
    return plan.logicalParameters.size() == 2 && isSysVSmallRecordsRoundTrip(plan, symbol);
}
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
// Two-integer records and direct scalar arguments; physical indices come from Clang.
inline bool isSysVTwoIntegerRecordWithScalars(const Plan& plan, std::string_view symbol) noexcept
{
    const auto pair = [](const Type& type) {
        return type.kind == TypeKind::Structure && !type.packed && type.elements.size() == 2 &&
            type.elements[0].kind == TypeKind::Integer && type.elements[0].widthOrCount == 64 &&
            type.elements[1].kind == TypeKind::Integer && type.elements[1].widthOrCount >= 8 &&
            type.elements[1].widthOrCount <= 64 && type.elements[1].widthOrCount % 8 == 0;
    };
    if (plan.symbol != symbol || plan.callingConvention != 78 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.returnSignExtend || plan.returnZeroExtend || !pair(plan.physicalReturn) ||
        plan.logicalReturn.passing != Passing::Direct || !plan.logicalReturn.coercion ||
        !pair(*plan.logicalReturn.coercion) || plan.logicalReturn.offset != 0 ||
        plan.logicalParameters.size() < 2) return false;
    const auto bits = plan.physicalReturn.elements[1].widthOrCount;
    if (plan.logicalReturn.coercion->elements[1].widthOrCount != bits) return false;
    std::size_t physicalIndex = 0;
    bool hasRecord = false;
    for (std::size_t i = 0; i < plan.logicalParameters.size(); ++i)
    {
        const auto& logical = plan.logicalParameters[i];
        if (logical.passing != Passing::Direct || !logical.coercion || logical.offset != 0 ||
            logical.firstPhysical != physicalIndex) return false;
        if (pair(*logical.coercion))
        {
            hasRecord = true;
            const auto argumentBits = logical.coercion->elements[1].widthOrCount;
            if (logical.physicalCount != 2 ||
                physicalIndex + 2 > plan.physicalParameters.size()) return false;
            for (std::size_t piece = 0; piece < 2; ++piece)
            {
                const auto& p = plan.physicalParameters[physicalIndex + piece];
                if (p.type.kind != TypeKind::Integer || p.type.widthOrCount != (piece == 0 ? 64 : argumentBits) ||
                    p.structureReturn || p.byValue || p.signExtend || p.zeroExtend || p.inRegister || p.noAlias) return false;
            }
            physicalIndex += 2;
            continue;
        }
        if (logical.physicalCount != 1 || physicalIndex >= plan.physicalParameters.size()) return false;
        const auto& p = plan.physicalParameters[physicalIndex++];
        if (p.structureReturn || p.byValue || p.signExtend || p.zeroExtend || p.inRegister || p.noAlias ||
            logical.coercion->kind != p.type.kind || logical.coercion->widthOrCount != p.type.widthOrCount) return false;
        if (!((p.type.kind == TypeKind::Integer && p.type.widthOrCount >= 8 && p.type.widthOrCount <= 64 && p.type.widthOrCount % 8 == 0) ||
              p.type.kind == TypeKind::Float || p.type.kind == TypeKind::Double ||
              (p.type.kind == TypeKind::Pointer && p.type.widthOrCount == 0))) return false;
    }
    return hasRecord && physicalIndex == plan.physicalParameters.size();
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
        alignment == 0 || alignment > (1ULL << 29) || (alignment & (alignment - 1)) != 0 ||
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
inline bool isSysVDirectPairShape(const Type& type) noexcept
{
    if (type.kind != TypeKind::Structure || type.packed || type.elements.size() != 2) return false;
    const auto& a = type.elements[0];
    const auto& b = type.elements[1];
    if (((a.kind == TypeKind::Float || a.kind == TypeKind::Double) && a.widthOrCount != 0) ||
        ((b.kind == TypeKind::Float || b.kind == TypeKind::Double) && b.widthOrCount != 0)) return false;
    const auto vec = [](const Type& t) { return t.kind == TypeKind::Vector && t.widthOrCount == 2 &&
        t.elements.size() == 1 && t.elements[0].kind == TypeKind::Float && t.elements[0].widthOrCount == 0; };
    const bool ai = a.kind == TypeKind::Integer && a.widthOrCount == 64;
    const bool bi = b.kind == TypeKind::Integer && b.widthOrCount >= 8 && b.widthOrCount <= 64 && b.widthOrCount % 8 == 0;
    return (ai && bi) || (a.kind == TypeKind::Double && b.kind == TypeKind::Integer && b.widthOrCount == 32) ||
        (a.kind == TypeKind::Double && (b.kind == TypeKind::Double || b.kind == TypeKind::Float || vec(b))) ||
        (a.kind == TypeKind::Float && b.kind == TypeKind::Double) ||
        (ai && (b.kind == TypeKind::Double || b.kind == TypeKind::Float || vec(b))) ||
        (vec(a) && (vec(b) || b.kind == TypeKind::Float || b.kind == TypeKind::Double ||
                    (b.kind == TypeKind::Integer && (b.widthOrCount == 32 || b.widthOrCount == 64))));
}
inline bool isSysVDirectPairLayout(const Type& type, std::uint64_t bits, std::uint64_t alignmentBits) noexcept
{
    if (!isSysVDirectPairShape(type)) return false;
    const auto& a = type.elements[0];
    const auto& b = type.elements[1];
    if (a.kind == TypeKind::Integer && b.kind == TypeKind::Integer) return bits == 64 + b.widthOrCount;
    if (a.kind == TypeKind::Vector && b.kind == TypeKind::Integer) return bits == 64 + b.widthOrCount && alignmentBits == 32;
    if (a.kind == TypeKind::Vector && b.kind == TypeKind::Float) return bits == 96 && alignmentBits == 32;
    if (a.kind == TypeKind::Integer && b.kind == TypeKind::Float) return bits == 96 && alignmentBits == 32;
    if ((a.kind == TypeKind::Vector && b.kind == TypeKind::Double) ||
        (a.kind == TypeKind::Double && b.kind == TypeKind::Vector)) return bits == 128 && alignmentBits == 64;
    if (a.kind == TypeKind::Vector && b.kind == TypeKind::Vector) return bits == 128 && (alignmentBits == 32 || alignmentBits == 128);
    if (a.kind == TypeKind::Integer && b.kind == TypeKind::Vector) return bits == 128 && alignmentBits == 32;
    return bits == 128 && alignmentBits == 64;
}
// Indirect record result and by-value records mixed with direct scalars.
inline bool isSysVIndirectRecordWithArguments(const Plan& plan, std::string_view symbol) noexcept
{
    const auto alignment = plan.logicalReturn.alignment;
    if (plan.symbol != symbol || plan.callingConvention != 78 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.returnSignExtend || plan.returnZeroExtend || plan.physicalReturn.kind != TypeKind::Void ||
        plan.logicalReturn.passing != Passing::Indirect || alignment == 0 || alignment > (1ULL << 29) ||
        (alignment & (alignment - 1)) != 0 || plan.physicalParameters.empty() ||
        (plan.logicalParameters.size() == 1 && plan.logicalParameters[0].passing == Passing::Indirect)) return false;
    const auto& result = plan.physicalParameters[0];
    if (result.type.kind != TypeKind::Pointer || result.type.widthOrCount != 0 || !result.pointee ||
        !result.structureReturn || result.alignment != alignment || result.byValue || result.inRegister ||
        result.signExtend || result.zeroExtend) return false;
    std::size_t physicalIndex = 1;
    for (std::size_t i = 0; i < plan.logicalParameters.size(); ++i)
    {
        const auto& logical = plan.logicalParameters[i];
        if (logical.firstPhysical != physicalIndex || physicalIndex >= plan.physicalParameters.size()) return false;
        const auto& physical = plan.physicalParameters[physicalIndex];
        if (physical.structureReturn ||
            physical.noAlias || physical.inRegister || physical.signExtend || physical.zeroExtend) return false;
        if (logical.passing == Passing::Indirect)
        {
            if (logical.physicalCount != 1 || !logical.byValue || logical.alignment < 8 || logical.alignment > (1ULL << 29) ||
                (logical.alignment & (logical.alignment - 1)) != 0 || !physical.byValue ||
                physical.type.kind != TypeKind::Pointer || physical.type.widthOrCount != 0 ||
                !physical.pointee || physical.alignment != logical.alignment) return false;
            ++physicalIndex;
            continue;
        }
        if (logical.passing == Passing::Direct && (logical.byValue || logical.signExtend || logical.zeroExtend)) return false;
        if (logical.passing == Passing::Direct && logical.coercion && logical.coercion->kind == TypeKind::Structure)
        {
            const auto& type = *logical.coercion;
            if (logical.offset != 0 || logical.physicalCount != 2 || !isSysVDirectPairShape(type) ||
                physicalIndex + 2 > plan.physicalParameters.size()) return false;
            for (std::size_t piece = 0; piece < 2; ++piece)
            {
                const auto& p = plan.physicalParameters[physicalIndex + piece];
                if (p.type.kind != type.elements[piece].kind || p.type.widthOrCount != type.elements[piece].widthOrCount ||
                    (p.type.kind == TypeKind::Vector && (p.type.elements.size() != 1 || p.type.elements[0].kind != TypeKind::Float ||
                                                         p.type.elements[0].widthOrCount != 0)) ||
                    p.structureReturn || p.byValue || p.noAlias || p.inRegister || p.signExtend || p.zeroExtend) return false;
            }
            physicalIndex += 2;
            continue;
        }
        if (logical.passing != Passing::Direct || !logical.coercion || logical.offset != 0 ||
            logical.physicalCount != 1 ||
            logical.coercion->kind != physical.type.kind || logical.coercion->widthOrCount != physical.type.widthOrCount ||
            physical.byValue ||
            !((physical.type.kind == TypeKind::Integer && physical.type.widthOrCount >= 8 && physical.type.widthOrCount <= 64 && physical.type.widthOrCount % 8 == 0) ||
              physical.type.kind == TypeKind::Float || physical.type.kind == TypeKind::Double ||
              (physical.type.kind == TypeKind::Pointer && physical.type.widthOrCount == 0))) return false;
        ++physicalIndex;
    }
    return physicalIndex == plan.physicalParameters.size();
}
}
