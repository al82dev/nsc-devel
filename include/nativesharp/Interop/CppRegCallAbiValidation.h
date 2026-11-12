#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"
#include "nativesharp/Interop/CppType.h"
#include <span>

namespace nativesharp::interop::abi_plan
{
// This check describes a physical plan; callers must separately verify record identity.
inline bool isRegCallWin64FloatRecordRoundTrip(const Plan& plan, std::string_view symbol,
                                               std::uint64_t sizeBytes, std::uint64_t alignment,
                                               std::size_t trailingInts = 0, bool allowScalars = false,
                                               std::size_t expandedRecords = 1, bool variedRecords = false,
                                               bool interleaved = false, bool intReturn = false,
                                               bool doubleReturn = false, bool voidReturn = false,
                                               bool floatReturn = false, bool int64Return = false,
                                               bool pointerReturn = false, unsigned narrowReturn = 0) noexcept
{
    const auto scalar = [](const Type& type) {
        return type.kind == TypeKind::Float && type.widthOrCount == 0 && !type.packed && type.elements.empty();
    };
    if (sizeBytes < 8 || sizeBytes > 16 || sizeBytes % 4 || alignment != 4) return false;
    const auto count = sizeBytes / 4;
    if (trailingInts > 1024 || !expandedRecords || expandedRecords > 1024) return false;
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
        (voidReturn ? (plan.physicalReturn.kind != TypeKind::Void || plan.physicalReturn.widthOrCount != 0 ||
                       plan.physicalReturn.packed || !plan.physicalReturn.elements.empty()) :
         pointerReturn ? (plan.physicalReturn.kind != TypeKind::Pointer || plan.physicalReturn.widthOrCount != 0 ||
                          plan.physicalReturn.packed || !plan.physicalReturn.elements.empty()) :
         floatReturn ? (plan.physicalReturn.kind != TypeKind::Float || plan.physicalReturn.widthOrCount != 0 ||
                        plan.physicalReturn.packed || !plan.physicalReturn.elements.empty()) :
         doubleReturn ? (plan.physicalReturn.kind != TypeKind::Double || plan.physicalReturn.widthOrCount != 0 ||
                         plan.physicalReturn.packed || !plan.physicalReturn.elements.empty()) :
         (intReturn || int64Return || narrowReturn) ? (plan.physicalReturn.kind != TypeKind::Integer || plan.physicalReturn.widthOrCount != (narrowReturn ? narrowReturn : int64Return ? 64 : 32) ||
                      plan.physicalReturn.packed || !plan.physicalReturn.elements.empty()) :
            (plan.physicalReturn.kind != TypeKind::Structure || countFloats(countFloats, plan.physicalReturn, 0) != count)) ||
        plan.logicalParameters.size() != expandedRecords + trailingInts)
        return false;
    const auto& result = plan.logicalReturn;
    if ((voidReturn ? (result.passing != Passing::Ignore || result.coercion.has_value()) :
        (result.passing != Passing::Direct || !result.coercion ||
         !sameType(sameType, *result.coercion, plan.physicalReturn, 0))) || result.offset ||
        result.byValue || result.signExtend || result.zeroExtend || !result.expandedOffsets.empty())
        return false;
    std::size_t physicalIndex = 0;
    std::size_t seenRecords = 0;
    for (std::size_t index = 0; index < plan.logicalParameters.size(); ++index)
    {
        const auto& argument = plan.logicalParameters[index];
        const bool expanded = interleaved ? argument.passing == Passing::Expand : index < expandedRecords;
        if (expanded)
        {
            ++seenRecords;
            const auto pieces = variedRecords ? argument.physicalCount : count;
            if (pieces < 2 || pieces > 4 || physicalIndex > plan.physicalParameters.size() ||
                pieces > plan.physicalParameters.size() - physicalIndex) return false;
            if (argument.passing != Passing::Expand || argument.coercion || argument.offset || argument.byValue ||
                argument.signExtend || argument.zeroExtend || argument.firstPhysical != physicalIndex ||
                argument.physicalCount != pieces || argument.expandedOffsets.size() != pieces) return false;
            for (std::size_t i = 0; i < pieces; ++i)
            {
                if (argument.expandedOffsets[i] != i * 4) return false;
                const auto& part = plan.physicalParameters[physicalIndex + i];
                if (!scalar(part.type) || part.pointee || part.alignment || part.structureReturn || part.byValue ||
                    part.signExtend || part.zeroExtend || part.inRegister || part.noAlias) return false;
            }
            physicalIndex += pieces;
            continue;
        }
        if (physicalIndex >= plan.physicalParameters.size()) return false;
        const auto& logical = argument;
        const auto& physical = plan.physicalParameters[physicalIndex];
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
            logical.offset || logical.alignment || logical.firstPhysical != physicalIndex || logical.physicalCount != 1 ||
            logical.byValue || logical.signExtend || logical.zeroExtend != boolean || !logical.expandedOffsets.empty() ||
            !supported(physical.type) || !sameType(sameType, *logical.coercion, physical.type, 0) ||
            physical.pointee || physical.alignment || physical.structureReturn ||
            physical.byValue || physical.signExtend || physical.zeroExtend != boolean || physical.inRegister || physical.noAlias)
            return false;
        ++physicalIndex;
    }
    return seenRecords == expandedRecords && physicalIndex == plan.physicalParameters.size();
}

inline bool regCallFloatRecordScalarMatches(const Type& physical, const CppType& source, unsigned depth = 0) noexcept
{
    if (depth > 16) return false;
    if (source.isVolatile || source.isRestrict ||
        (source.isConst && (depth == 0 || source.kind == CppTypeKind::Pointer || source.kind == CppTypeKind::Record ||
                            source.unqualifiedCanonicalSpelling.empty()))) return false;
    if (source.isConst && (source.canonicalSpelling.size() != source.unqualifiedCanonicalSpelling.size() + 6 ||
        !source.canonicalSpelling.starts_with("const ") ||
        !source.canonicalSpelling.ends_with(source.unqualifiedCanonicalSpelling))) return false;
    const std::string_view canonical = source.isConst ? source.unqualifiedCanonicalSpelling : source.canonicalSpelling;
    if (source.kind == CppTypeKind::Boolean && (canonical == "_Bool" || canonical == "bool") &&
        source.sizeBits == 8 && source.alignmentBits == 8)
        return physical.kind == TypeKind::Integer && physical.widthOrCount == 1;
    if (((source.kind == CppTypeKind::SignedInteger && canonical == "signed char") ||
         (source.kind == CppTypeKind::UnsignedInteger && canonical == "unsigned char")) &&
        source.sizeBits == 8 && source.alignmentBits == 8)
        return physical.kind == TypeKind::Integer && physical.widthOrCount == 8;
    if (((source.kind == CppTypeKind::SignedInteger && canonical == "short") ||
         (source.kind == CppTypeKind::UnsignedInteger && canonical == "unsigned short")) &&
        source.sizeBits == 16 && source.alignmentBits == 16)
        return physical.kind == TypeKind::Integer && physical.widthOrCount == 16;
    if (((source.kind == CppTypeKind::SignedInteger && (canonical == "int" || canonical == "long")) ||
         (source.kind == CppTypeKind::UnsignedInteger && (canonical == "unsigned int" || canonical == "unsigned long"))) &&
        source.sizeBits == 32 && source.alignmentBits == 32)
        return physical.kind == TypeKind::Integer && physical.widthOrCount == 32;
    if (((source.kind == CppTypeKind::SignedInteger && canonical == "long long") ||
         (source.kind == CppTypeKind::UnsignedInteger && canonical == "unsigned long long")) &&
        source.sizeBits == 64 && source.alignmentBits == 64)
        return physical.kind == TypeKind::Integer && physical.widthOrCount == 64;
    if (source.kind == CppTypeKind::Pointer && source.sizeBits == 64 && source.alignmentBits == 64 &&
        source.element && !source.element->isVolatile && !source.element->isRestrict &&
        physical.kind == TypeKind::Pointer && physical.widthOrCount == 0)
    {
        const auto& element = *source.element;
        const std::string_view suffix = element.kind == CppTypeKind::Pointer ? "*" : " *";
        if (source.canonicalSpelling.size() != element.canonicalSpelling.size() + suffix.size() ||
            !source.canonicalSpelling.starts_with(element.canonicalSpelling) ||
            !source.canonicalSpelling.ends_with(suffix)) return false;
        if (element.kind == CppTypeKind::Void)
            return (!element.isConst && element.canonicalSpelling == "void") ||
                (element.isConst && element.canonicalSpelling == "const void" &&
                 element.unqualifiedCanonicalSpelling == "void");
        // Physical acceptance only: callers must independently reconcile the
        // record identity and its origin before using this pointer plan.
        if (element.kind == CppTypeKind::Record)
            return !element.isConst && element.recordIdentity && element.recordIdentity->clangUsr &&
                !element.recordIdentity->clangUsr->empty() && !element.recordIdentity->name.empty() &&
                !element.recordIdentity->anonymousContext && !element.recordIdentity->specializationContext &&
                element.canonicalSpelling.size() > element.recordIdentity->name.size() &&
                element.canonicalSpelling.ends_with(element.recordIdentity->name) &&
                (element.canonicalSpelling[element.canonicalSpelling.size() - element.recordIdentity->name.size() - 1] == ' ' ||
                 element.canonicalSpelling[element.canonicalSpelling.size() - element.recordIdentity->name.size() - 1] == ':');
        if (element.kind == CppTypeKind::Array) return false;
        Type candidate;
        if (element.kind == CppTypeKind::Pointer)
            candidate.kind = TypeKind::Pointer;
        else if (element.kind == CppTypeKind::Floating)
            candidate.kind = (element.isConst ? element.unqualifiedCanonicalSpelling : element.canonicalSpelling) == "float"
                ? TypeKind::Float : TypeKind::Double;
        else
        {
            candidate.kind = TypeKind::Integer;
            candidate.widthOrCount = element.kind == CppTypeKind::Boolean ? 1 : element.sizeBits.value_or(0);
        }
        return regCallFloatRecordScalarMatches(candidate, element, depth + 1);
    }
    if (source.kind == CppTypeKind::Floating && canonical == "float" &&
        source.sizeBits == 32 && source.alignmentBits == 32) return physical.kind == TypeKind::Float;
    if (source.kind == CppTypeKind::Floating && canonical == "double" &&
        source.sizeBits == 64 && source.alignmentBits == 64) return physical.kind == TypeKind::Double;
    return false;
}

inline std::size_t regCallExpandedRecordCount(const Plan& plan) noexcept
{
    std::size_t count = 0;
    while (count < plan.logicalParameters.size() && plan.logicalParameters[count].passing == Passing::Expand)
        ++count;
    return count;
}

inline bool regCallSameRecord(const CppType& result, const CppType& parameter) noexcept
{
    return result.kind == CppTypeKind::Record && parameter.kind == CppTypeKind::Record &&
        result.recordIdentity && result.recordIdentity->clangUsr && !result.recordIdentity->clangUsr->empty() &&
        parameter.recordIdentity && parameter.recordIdentity->clangUsr == result.recordIdentity->clangUsr &&
        result.canonicalSpelling == parameter.canonicalSpelling && result.sizeBits && result.alignmentBits &&
        parameter.sizeBits == result.sizeBits && parameter.alignmentBits == result.alignmentBits;
}

inline bool regCallCompatibleRecordLayout(const CppType& result, const CppType& parameter) noexcept
{
    return result.kind == CppTypeKind::Record && parameter.kind == CppTypeKind::Record &&
        result.recordIdentity && result.recordIdentity->clangUsr && !result.recordIdentity->clangUsr->empty() &&
        parameter.recordIdentity && parameter.recordIdentity->clangUsr && !parameter.recordIdentity->clangUsr->empty() &&
        !parameter.canonicalSpelling.empty() && result.sizeBits && result.alignmentBits &&
        (parameter.canonicalSpelling == result.canonicalSpelling) ==
            (parameter.recordIdentity->clangUsr == result.recordIdentity->clangUsr) &&
        parameter.sizeBits == result.sizeBits && parameter.alignmentBits == result.alignmentBits;
}

inline bool regCallRecordMatchesExpansion(const CppType& result, const CppType& parameter,
                                          const LogicalValue& logical) noexcept
{
    return parameter.kind == CppTypeKind::Record &&
        parameter.recordIdentity && parameter.recordIdentity->clangUsr && !parameter.recordIdentity->clangUsr->empty() &&
        !parameter.canonicalSpelling.empty() &&
        (result.kind != CppTypeKind::Record ||
            (result.recordIdentity && result.recordIdentity->clangUsr && !result.recordIdentity->clangUsr->empty() &&
             (parameter.canonicalSpelling == result.canonicalSpelling) ==
                (parameter.recordIdentity->clangUsr == result.recordIdentity->clangUsr))) &&
        logical.passing == Passing::Expand && logical.physicalCount >= 2 && logical.physicalCount <= 4 &&
        parameter.sizeBits == logical.physicalCount * 32 && parameter.alignmentBits == 32;
}

inline bool isRegCallWin64FloatRecordSequence(const Plan& plan, std::string_view symbol,
                                              std::uint64_t size, std::uint64_t alignment) noexcept
{
    const auto count = regCallExpandedRecordCount(plan);
    return count && isRegCallWin64FloatRecordRoundTrip(plan, symbol, size, alignment,
        plan.logicalParameters.size() - count, true, count, true);
}

inline bool isRegCallWin64FloatRecordInterleaved(const Plan& plan, std::string_view symbol,
                                                 std::uint64_t size, std::uint64_t alignment) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, size, alignment,
        plan.logicalParameters.size() - records, true, records, true, true);
}

inline bool isRegCallWin64FloatRecordsReturningInt(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, true);
}

inline bool isRegCallWin64FloatRecordsReturningDouble(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, true);
}

inline bool isRegCallWin64FloatRecordsReturningVoid(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, true);
}

inline bool isRegCallWin64FloatRecordsReturningFloat(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, false, true);
}

inline bool isRegCallWin64FloatRecordsReturningInt64(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, false, false, true);
}

inline bool isRegCallWin64FloatRecordsReturningPointer(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, false, false, false, true);
}

inline bool isRegCallWin64FloatRecordsReturningNarrowInteger(const Plan& plan, std::string_view symbol) noexcept
{
    const auto bits = plan.physicalReturn.widthOrCount;
    if (bits != 8 && bits != 16) return false;
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, false, false, false, false,
        static_cast<unsigned>(bits));
}

inline bool regCallFloatRecordArgumentsMatch(const Plan& plan, const CppType& result,
                                             std::span<const CppType> parameters) noexcept
{
    if (parameters.size() != plan.logicalParameters.size()) return false;
    for (std::size_t i = 0; i < parameters.size(); ++i)
    {
        const auto& logical = plan.logicalParameters[i];
        if (logical.passing == Passing::Expand)
        {
            if (!regCallRecordMatchesExpansion(result, parameters[i], logical)) return false;
        }
        else if (logical.firstPhysical >= plan.physicalParameters.size() ||
                 !regCallFloatRecordScalarMatches(plan.physicalParameters[logical.firstPhysical].type, parameters[i]))
            return false;
    }
    return true;
}

inline bool regCallFloatRecordTailMatches(const Plan& plan, std::span<const CppType> parameters,
                                         std::size_t firstScalar = 1) noexcept
{
    if (parameters.empty() || firstScalar > parameters.size() || plan.logicalParameters.size() != parameters.size()) return false;
    for (std::size_t i = firstScalar; i < parameters.size(); ++i)
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

// Pointer returns to records require separate identity and lifetime reconciliation.
inline bool regCallFloatRecordScalarPointerReturnSupported(const CppType& source) noexcept
{
    if (source.kind != CppTypeKind::Pointer || !regCallFloatRecordScalarSupported(source)) return false;
    const CppType* element = &source;
    unsigned depth = 0;
    while (element->kind == CppTypeKind::Pointer)
    {
        if (++depth > 16 || !element->element) return false;
        element = element->element.get();
    }
    return element->kind != CppTypeKind::Record;
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
