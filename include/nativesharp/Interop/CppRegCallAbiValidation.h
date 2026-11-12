#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"

namespace nativesharp::interop::abi_plan
{
// This check describes a physical plan; callers must separately verify record identity.
inline bool isRegCallWin64TwoFloatRecordRoundTrip(const Plan& plan, std::string_view symbol,
                                                  std::uint64_t sizeBytes, std::uint64_t alignment) noexcept
{
    const auto scalar = [](const Type& type) {
        return type.kind == TypeKind::Float && type.widthOrCount == 0 && !type.packed && type.elements.empty();
    };
    const auto pair = [&](const Type& type) {
        return type.kind == TypeKind::Structure && !type.packed && type.widthOrCount == 0 &&
            type.elements.size() == 2 && scalar(type.elements[0]) && scalar(type.elements[1]);
    };
    if (symbol.empty() || plan.symbol != symbol || plan.callingConvention != 92 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        sizeBytes != 8 || alignment != 4 || plan.returnSignExtend || plan.returnZeroExtend ||
        !pair(plan.physicalReturn) || plan.logicalParameters.size() != 1 || plan.physicalParameters.size() != 2)
        return false;
    const auto& result = plan.logicalReturn;
    const auto& argument = plan.logicalParameters[0];
    if (result.passing != Passing::Direct || !result.coercion || !pair(*result.coercion) || result.offset ||
        result.byValue || result.signExtend || result.zeroExtend || !result.expandedOffsets.empty() ||
        argument.passing != Passing::Expand || argument.coercion || argument.offset || argument.byValue ||
        argument.signExtend || argument.zeroExtend || argument.firstPhysical != 0 || argument.physicalCount != 2 ||
        argument.expandedOffsets.size() != 2 || argument.expandedOffsets[0] != 0 || argument.expandedOffsets[1] != 4)
        return false;
    for (const auto& part : plan.physicalParameters)
        if (!scalar(part.type) || part.pointee || part.alignment || part.structureReturn || part.byValue ||
            part.signExtend || part.zeroExtend || part.inRegister || part.noAlias) return false;
    return true;
}

// A single C record, passed and returned with exactly the normal Win64
// carrier. This is a physical-plan check, not a record identity check.
inline bool isRegCallWin64RecordRoundTrip(const Plan& plan, std::string_view symbol,
                                          std::uint64_t sizeBytes, std::uint64_t alignment) noexcept
{
    if (symbol.empty() || plan.symbol != symbol || plan.callingConvention != 92 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        !sizeBytes || !alignment || (alignment & (alignment - 1)) || sizeBytes % alignment ||
        plan.returnSignExtend || plan.returnZeroExtend || plan.logicalParameters.size() != 1) return false;
    const auto& result = plan.logicalReturn;
    const auto& argument = plan.logicalParameters[0];
    if (result.offset || argument.offset || result.signExtend || result.zeroExtend ||
        argument.signExtend || argument.zeroExtend || argument.byValue || argument.physicalCount != 1) return false;
    const auto plain = [](const PhysicalParameter& value) {
        return !value.byValue && !value.signExtend && !value.zeroExtend && !value.inRegister;
    };
    if (sizeBytes == 1 || sizeBytes == 2 || sizeBytes == 4 || sizeBytes == 8)
    {
        const auto bits = sizeBytes * 8;
        const auto integer = [bits](const Type& type) { return type.kind == TypeKind::Integer && type.widthOrCount == bits; };
        return plan.physicalParameters.size() == 1 && integer(plan.physicalReturn) &&
            result.passing == Passing::Direct && result.coercion && integer(*result.coercion) &&
            argument.passing == Passing::Direct && argument.coercion && integer(*argument.coercion) &&
            argument.firstPhysical == 0 && integer(plan.physicalParameters[0].type) &&
            !plan.physicalParameters[0].structureReturn && plain(plan.physicalParameters[0]);
    }
    return plan.physicalReturn.kind == TypeKind::Void && plan.physicalParameters.size() == 2 &&
        result.passing == Passing::Indirect && result.alignment == alignment &&
        argument.passing == Passing::Indirect && argument.alignment == alignment && argument.firstPhysical == 1 &&
        plan.physicalParameters[0].type.kind == TypeKind::Pointer && plan.physicalParameters[0].type.widthOrCount == 0 &&
        plan.physicalParameters[0].structureReturn &&
        plan.physicalParameters[0].alignment == alignment && plain(plan.physicalParameters[0]) &&
        plan.physicalParameters[1].type.kind == TypeKind::Pointer && plan.physicalParameters[1].type.widthOrCount == 0 &&
        !plan.physicalParameters[1].structureReturn &&
        plain(plan.physicalParameters[1]);
}
}
