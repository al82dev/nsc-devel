#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"
#include "nativesharp/Interop/CppType.h"
#include <span>

namespace nativesharp::interop::abi_plan
{
// This check describes a physical plan; callers must separately verify record identity.
inline bool isRegCallWin64FloatRecordRoundTrip(const Plan& plan, std::string_view symbol,
                                               std::uint64_t sizeBytes, std::uint64_t alignment,
                                               std::size_t trailingInts = 0, bool allowScalars = false) noexcept
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
        const auto supported = [allowScalars](const Type& type) {
            return !type.packed && type.elements.empty() &&
                ((type.kind == TypeKind::Integer && (type.widthOrCount == 32 ||
                    (allowScalars && (type.widthOrCount == 1 || type.widthOrCount == 8 ||
                                      type.widthOrCount == 16 || type.widthOrCount == 64)))) ||
                 (allowScalars && (type.kind == TypeKind::Float || type.kind == TypeKind::Double || type.kind == TypeKind::Pointer) &&
                  type.widthOrCount == 0));
        };
        const bool boolean = physical.type.kind == TypeKind::Integer && physical.type.widthOrCount == 1;
        if (logical.passing != (boolean ? Passing::Extend : Passing::Direct) || !logical.coercion || !supported(*logical.coercion) ||
            logical.offset || logical.alignment || logical.firstPhysical != count + index || logical.physicalCount != 1 ||
            logical.byValue || logical.signExtend || logical.zeroExtend != boolean || !logical.expandedOffsets.empty() ||
            !supported(physical.type) || !sameType(sameType, *logical.coercion, physical.type, 0) ||
            physical.pointee || physical.alignment || physical.structureReturn ||
            physical.byValue || physical.signExtend || physical.zeroExtend != boolean || physical.inRegister || physical.noAlias)
            return false;
    }
    return true;
}

inline bool regCallFloatRecordScalarMatches(const Type& physical, const CppType& source) noexcept
{
    if (source.isConst || source.isVolatile || source.isRestrict) return false;
    if (source.kind == CppTypeKind::Boolean && (source.canonicalSpelling == "_Bool" || source.canonicalSpelling == "bool") &&
        source.sizeBits == 8 && source.alignmentBits == 8)
        return physical.kind == TypeKind::Integer && physical.widthOrCount == 1;
    if (((source.kind == CppTypeKind::SignedInteger && source.canonicalSpelling == "signed char") ||
         (source.kind == CppTypeKind::UnsignedInteger && source.canonicalSpelling == "unsigned char")) &&
        source.sizeBits == 8 && source.alignmentBits == 8)
        return physical.kind == TypeKind::Integer && physical.widthOrCount == 8;
    if (((source.kind == CppTypeKind::SignedInteger && source.canonicalSpelling == "short") ||
         (source.kind == CppTypeKind::UnsignedInteger && source.canonicalSpelling == "unsigned short")) &&
        source.sizeBits == 16 && source.alignmentBits == 16)
        return physical.kind == TypeKind::Integer && physical.widthOrCount == 16;
    if (((source.kind == CppTypeKind::SignedInteger && (source.canonicalSpelling == "int" || source.canonicalSpelling == "long")) ||
         (source.kind == CppTypeKind::UnsignedInteger && (source.canonicalSpelling == "unsigned int" || source.canonicalSpelling == "unsigned long"))) &&
        source.sizeBits == 32 && source.alignmentBits == 32)
        return physical.kind == TypeKind::Integer && physical.widthOrCount == 32;
    if (((source.kind == CppTypeKind::SignedInteger && source.canonicalSpelling == "long long") ||
         (source.kind == CppTypeKind::UnsignedInteger && source.canonicalSpelling == "unsigned long long")) &&
        source.sizeBits == 64 && source.alignmentBits == 64)
        return physical.kind == TypeKind::Integer && physical.widthOrCount == 64;
    if (source.kind == CppTypeKind::Pointer && source.canonicalSpelling == "int *" &&
        source.sizeBits == 64 && source.alignmentBits == 64 && source.element &&
        source.element->kind == CppTypeKind::SignedInteger && source.element->canonicalSpelling == "int" &&
        source.element->sizeBits == 32 && source.element->alignmentBits == 32 &&
        !source.element->isConst && !source.element->isVolatile && !source.element->isRestrict)
        return physical.kind == TypeKind::Pointer && physical.widthOrCount == 0;
    if (source.kind == CppTypeKind::Floating && source.canonicalSpelling == "float" &&
        source.sizeBits == 32 && source.alignmentBits == 32) return physical.kind == TypeKind::Float;
    if (source.kind == CppTypeKind::Floating && source.canonicalSpelling == "double" &&
        source.sizeBits == 64 && source.alignmentBits == 64) return physical.kind == TypeKind::Double;
    return false;
}

inline bool regCallFloatRecordTailMatches(const Plan& plan, std::span<const CppType> parameters) noexcept
{
    if (parameters.empty() || plan.logicalParameters.size() != parameters.size()) return false;
    for (std::size_t i = 1; i < parameters.size(); ++i)
    {
        const auto physical = plan.logicalParameters[i].firstPhysical;
        if (physical >= plan.physicalParameters.size() ||
            !regCallFloatRecordScalarMatches(plan.physicalParameters[physical].type, parameters[i])) return false;
    }
    return true;
}

inline bool regCallFloatRecordScalarSupported(const CppType& source) noexcept
{
    Type integer; integer.kind = TypeKind::Integer; integer.widthOrCount = 32;
    Type integer64; integer64.kind = TypeKind::Integer; integer64.widthOrCount = 64;
    Type boolean; boolean.kind = TypeKind::Integer; boolean.widthOrCount = 1;
    Type byte; byte.kind = TypeKind::Integer; byte.widthOrCount = 8;
    Type shortValue; shortValue.kind = TypeKind::Integer; shortValue.widthOrCount = 16;
    Type real; real.kind = TypeKind::Float;
    Type wide; wide.kind = TypeKind::Double;
    Type pointer; pointer.kind = TypeKind::Pointer;
    return regCallFloatRecordScalarMatches(integer, source) ||
        regCallFloatRecordScalarMatches(integer64, source) || regCallFloatRecordScalarMatches(pointer, source) ||
        regCallFloatRecordScalarMatches(boolean, source) || regCallFloatRecordScalarMatches(byte, source) ||
        regCallFloatRecordScalarMatches(shortValue, source) ||
        regCallFloatRecordScalarMatches(real, source) || regCallFloatRecordScalarMatches(wide, source);
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
