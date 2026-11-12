#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"

namespace nativesharp::interop::abi_plan
{
// This check describes a physical plan; callers must separately verify record identity.
inline bool isRegCallWin64FloatRecordRoundTrip(const Plan& plan, std::string_view symbol,
                                               std::uint64_t sizeBytes, std::uint64_t alignment,
                                               std::size_t trailingInts = 0) noexcept
{
    const auto scalar = [](const Type& type) {
        return type.kind == TypeKind::Float && type.widthOrCount == 0 && !type.packed && type.elements.empty();
    };
    if (sizeBytes < 8 || sizeBytes > 16 || sizeBytes % 4 || alignment != 4) return false;
    const auto count = sizeBytes / 4;
    if (trailingInts > 1024) return false;
    const auto countFloats = [&](const auto& self, const Type& type, unsigned depth) -> std::uint64_t {
        if (depth > 16 || type.packed) return 0;
        if (scalar(type)) return 1;
        if (type.kind == TypeKind::Array)
        {
            if (type.elements.size() != 1 || !type.widthOrCount || type.widthOrCount > count) return 0;
            const auto element = self(self, type.elements[0], depth + 1);
            return element && element <= count / type.widthOrCount ? element * type.widthOrCount : 0;
        }
        if (type.kind != TypeKind::Structure || type.widthOrCount || type.elements.empty()) return 0;
        std::uint64_t total = 0;
        for (const auto& element : type.elements)
        {
            const auto part = self(self, element, depth + 1);
            if (!part || part > count - total) return 0;
            total += part;
        }
        return total;
    };
    const auto sameType = [&](const auto& self, const Type& left, const Type& right, unsigned depth) -> bool {
        if (depth > 16 || left.kind != right.kind || left.widthOrCount != right.widthOrCount ||
            left.packed != right.packed || left.elements.size() != right.elements.size()) return false;
        for (std::size_t i = 0; i < left.elements.size(); ++i)
            if (!self(self, left.elements[i], right.elements[i], depth + 1)) return false;
        return true;
    };
    if (symbol.empty() || plan.symbol != symbol || plan.callingConvention != 92 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.returnSignExtend || plan.returnZeroExtend ||
        plan.physicalReturn.kind != TypeKind::Structure || countFloats(countFloats, plan.physicalReturn, 0) != count ||
        plan.logicalParameters.size() != 1 + trailingInts ||
        plan.physicalParameters.size() != count + trailingInts)
        return false;
    const auto& result = plan.logicalReturn;
    const auto& argument = plan.logicalParameters[0];
    if (result.passing != Passing::Direct || !result.coercion ||
        !sameType(sameType, *result.coercion, plan.physicalReturn, 0) || result.offset ||
        result.byValue || result.signExtend || result.zeroExtend || !result.expandedOffsets.empty() ||
        argument.passing != Passing::Expand || argument.coercion || argument.offset || argument.byValue ||
        argument.signExtend || argument.zeroExtend || argument.firstPhysical != 0 || argument.physicalCount != count ||
        argument.expandedOffsets.size() != count)
        return false;
    for (std::size_t i = 0; i < count; ++i)
        if (argument.expandedOffsets[i] != i * 4) return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& part = plan.physicalParameters[i];
        if (!scalar(part.type) || part.pointee || part.alignment || part.structureReturn || part.byValue ||
            part.signExtend || part.zeroExtend || part.inRegister || part.noAlias) return false;
    }
    for (std::size_t index = 0; index < trailingInts; ++index)
    {
        const auto& logical = plan.logicalParameters[index + 1];
        const auto& physical = plan.physicalParameters[count + index];
        const auto integer = [](const Type& type) {
            return type.kind == TypeKind::Integer && type.widthOrCount == 32 && !type.packed && type.elements.empty();
        };
        if (logical.passing != Passing::Direct || !logical.coercion || !integer(*logical.coercion) ||
            logical.offset || logical.alignment || logical.firstPhysical != count + index || logical.physicalCount != 1 ||
            logical.byValue || logical.signExtend || logical.zeroExtend || !logical.expandedOffsets.empty() ||
            !integer(physical.type) || physical.pointee || physical.alignment || physical.structureReturn ||
            physical.byValue || physical.signExtend || physical.zeroExtend || physical.inRegister || physical.noAlias)
            return false;
    }
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
