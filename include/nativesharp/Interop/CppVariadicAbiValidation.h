#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"

namespace nativesharp::interop
{
namespace variadic_validation_detail
{
inline bool validVariadicType(const abi_plan::Type& type, unsigned depth) noexcept
{
    using abi_plan::TypeKind;
    if (depth > 32) return false;
    switch (type.kind)
    {
        case TypeKind::Void:
        case TypeKind::Float:
        case TypeKind::Double:
        case TypeKind::Pointer:
            return type.widthOrCount == 0 && !type.packed && type.elements.empty();
        case TypeKind::Integer:
            return type.widthOrCount > 0 && type.widthOrCount <= 128 && !type.packed && type.elements.empty();
        case TypeKind::Structure:
            if (type.widthOrCount != 0 || type.elements.size() > 1024) return false;
            break;
        case TypeKind::Array:
            if (type.packed || type.widthOrCount > (1U << 24) || type.elements.size() != 1) return false;
            break;
        case TypeKind::Vector:
            if (type.packed || type.widthOrCount != 2 || type.elements.size() != 1 ||
                type.elements[0].kind != TypeKind::Float) return false;
            break;
        default: return false;
    }
    for (const auto& element : type.elements)
        if (element.kind == TypeKind::Void || !validVariadicType(element, depth + 1)) return false;
    return true;
}

inline bool equalVariadicType(const abi_plan::Type& a, const abi_plan::Type& b) noexcept
{
    if (a.kind != b.kind || a.widthOrCount != b.widthOrCount || a.packed != b.packed ||
        a.elements.size() != b.elements.size()) return false;
    for (std::size_t i = 0; i < a.elements.size(); ++i)
        if (!equalVariadicType(a.elements[i], b.elements[i])) return false;
    return true;
}

inline bool validVariadicAlignment(std::uint64_t value) noexcept
{
    return value != 0 && value <= (1ULL << 29) && (value & (value - 1)) == 0;
}

}

inline bool validateCppVariadicSysVCallPlan(const abi_plan::VariadicCallPlan& plan,
                                     std::string_view expectedSymbol,
                                     std::size_t expectedRequiredArguments) noexcept
{
    using namespace variadic_validation_detail;
    using abi_plan::Passing;
    using abi_plan::TypeKind;
    if (expectedSymbol.empty() || plan.symbol != expectedSymbol || plan.callingConvention != 78 ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.requiredArguments != expectedRequiredArguments || plan.requiredArguments > plan.arguments.size() ||
        plan.arguments.size() > 1024 ||
        plan.physicalParameters.size() > 2049 || !validVariadicType(plan.physicalReturn, 0) ||
        (plan.returnSignExtend && plan.returnZeroExtend)) return false;
    const auto& result = plan.result;
    if (result.offset != 0) return false;
    std::size_t next = 0;
    if (result.passing == Passing::Indirect)
    {
        if (plan.physicalReturn.kind != TypeKind::Void || plan.returnSignExtend || plan.returnZeroExtend ||
            result.coercion || result.signExtend || result.zeroExtend ||
            !validVariadicAlignment(result.alignment) || plan.physicalParameters.empty()) return false;
        const auto& p = plan.physicalParameters[0];
        if (!p.structureReturn || p.byValue || p.inRegister || p.signExtend || p.zeroExtend ||
            p.type.kind != TypeKind::Pointer || !validVariadicType(p.type, 0) || !p.pointee ||
            p.pointee->kind == TypeKind::Void || !validVariadicType(*p.pointee, 0) ||
            p.alignment != result.alignment) return false;
        next = 1;
    }
    else if (result.passing == Passing::Ignore)
    {
        if (plan.physicalReturn.kind != TypeKind::Void || result.coercion || result.byValue ||
            result.signExtend || result.zeroExtend ||
            plan.returnSignExtend || plan.returnZeroExtend) return false;
    }
    else if (result.passing == Passing::Direct || result.passing == Passing::Extend)
    {
        if (!result.coercion || result.byValue || plan.physicalReturn.kind == TypeKind::Void ||
            !validVariadicType(*result.coercion, 0) ||
            !equalVariadicType(*result.coercion, plan.physicalReturn)) return false;
        if (result.passing == Passing::Extend)
        {
            if (result.signExtend == result.zeroExtend || result.signExtend != plan.returnSignExtend ||
                result.zeroExtend != plan.returnZeroExtend || plan.physicalReturn.kind != TypeKind::Integer) return false;
        }
        else if (plan.returnSignExtend || plan.returnZeroExtend || result.signExtend || result.zeroExtend) return false;
    }
    else return false;
    for (std::size_t i = 0; i < plan.arguments.size(); ++i)
    {
        const auto& argument = plan.arguments[i];
        if (argument.firstPhysical != next || argument.offset != 0 ||
            argument.physicalCount > plan.physicalParameters.size() - next) return false;
        if (argument.passing == Passing::Ignore)
        {
            if (argument.physicalCount != 0 || argument.coercion || argument.byValue ||
                argument.signExtend || argument.zeroExtend) return false;
            continue;
        }
        if (argument.passing == Passing::Indirect)
        {
            if (argument.physicalCount != 1 || !argument.byValue || argument.coercion || argument.flatten ||
                argument.signExtend || argument.zeroExtend || !validVariadicAlignment(argument.alignment)) return false;
            const auto& p = plan.physicalParameters[next++];
            if (!p.byValue || p.structureReturn || p.inRegister || p.noAlias || p.signExtend || p.zeroExtend ||
                p.type.kind != TypeKind::Pointer || !validVariadicType(p.type, 0) || !p.pointee ||
                p.pointee->kind == TypeKind::Void || !validVariadicType(*p.pointee, 0) ||
                p.alignment != argument.alignment) return false;
            continue;
        }
        if ((argument.passing != Passing::Direct && argument.passing != Passing::Extend) ||
            !argument.coercion || argument.coercion->kind == TypeKind::Void ||
            !validVariadicType(*argument.coercion, 0) || argument.byValue ||
            (argument.alignment != 0 && !validVariadicAlignment(argument.alignment))) return false;
        const auto& coercion = *argument.coercion;
        const bool flattened = argument.passing == Passing::Direct && argument.flatten && coercion.kind == TypeKind::Structure;
        const auto count = flattened ? coercion.elements.size() : 1U;
        if (argument.physicalCount != count) return false;
        if (argument.passing == Passing::Extend && (argument.signExtend == argument.zeroExtend ||
            coercion.kind != TypeKind::Integer)) return false;
        if (argument.passing == Passing::Direct && (argument.signExtend || argument.zeroExtend)) return false;
        for (std::size_t part = 0; part < count; ++part)
        {
            const auto& p = plan.physicalParameters[next++];
            const auto& expected = flattened ? coercion.elements[part] : coercion;
            if (!validVariadicType(p.type, 0) || !equalVariadicType(p.type, expected) || p.structureReturn ||
                p.byValue || p.inRegister || p.pointee || (p.alignment != 0 &&
                (p.type.kind != TypeKind::Pointer || !validVariadicAlignment(p.alignment))) ||
                (p.noAlias && (i >= plan.requiredArguments ||
                p.type.kind != TypeKind::Pointer)) || p.signExtend != argument.signExtend ||
                p.zeroExtend != argument.zeroExtend) return false;
        }
    }
    return next == plan.physicalParameters.size();
}

}
