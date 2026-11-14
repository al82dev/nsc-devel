#pragma once

#include "nativesharp/Interop/CppAbiPlan.h"
#include "nativesharp/Interop/CppType.h"
#include <algorithm>
#include <span>
#include <utility>

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
                                               bool pointerReturn = false, unsigned narrowReturn = 0,
                                               bool boolReturn = false, std::uint64_t indirectAlignment = 0,
                                               bool allowDoublePieces = false, bool allowNoExpandedRecords = false,
                                               bool doubleRecordReturn = false) noexcept
{
    const auto scalar = [](const Type& type) {
        return type.kind == TypeKind::Float && type.widthOrCount == 0 && !type.packed && type.elements.empty();
    };
    if (doubleRecordReturn ? (sizeBytes < 16 || sizeBytes > 32 || sizeBytes % 8 || alignment != 8) :
        (sizeBytes < 8 || sizeBytes > 16 || sizeBytes % 4 || alignment != 4)) return false;
    const auto count = sizeBytes / (doubleRecordReturn ? 8 : 4);
    if (trailingInts > 1024 || (!expandedRecords && !allowNoExpandedRecords) || expandedRecords > 1024) return false;
    const auto countFloats = [&](const auto& self, const Type& type, unsigned depth) -> std::uint64_t {
        if (depth > 16 || type.packed) return 0;
        if (doubleRecordReturn ? (type.kind == TypeKind::Double && !type.widthOrCount && !type.packed && type.elements.empty()) : scalar(type)) return 1;
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
        plan.returnSignExtend || plan.returnZeroExtend != boolReturn ||
        ((voidReturn || indirectAlignment) ? (plan.physicalReturn.kind != TypeKind::Void || plan.physicalReturn.widthOrCount != 0 ||
                       plan.physicalReturn.packed || !plan.physicalReturn.elements.empty()) :
         pointerReturn ? (plan.physicalReturn.kind != TypeKind::Pointer || plan.physicalReturn.widthOrCount != 0 ||
                          plan.physicalReturn.packed || !plan.physicalReturn.elements.empty()) :
         floatReturn ? (plan.physicalReturn.kind != TypeKind::Float || plan.physicalReturn.widthOrCount != 0 ||
                        plan.physicalReturn.packed || !plan.physicalReturn.elements.empty()) :
         doubleReturn ? (plan.physicalReturn.kind != TypeKind::Double || plan.physicalReturn.widthOrCount != 0 ||
                         plan.physicalReturn.packed || !plan.physicalReturn.elements.empty()) :
         (intReturn || int64Return || narrowReturn || boolReturn) ? (plan.physicalReturn.kind != TypeKind::Integer || plan.physicalReturn.widthOrCount != (boolReturn ? 1 : narrowReturn ? narrowReturn : int64Return ? 64 : 32) ||
                      plan.physicalReturn.packed || !plan.physicalReturn.elements.empty()) :
            (plan.physicalReturn.kind != TypeKind::Structure || countFloats(countFloats, plan.physicalReturn, 0) != count)) ||
        plan.logicalParameters.size() != expandedRecords + trailingInts)
        return false;
    const auto& result = plan.logicalReturn;
    if ((indirectAlignment ? (result.passing != Passing::Indirect || result.coercion.has_value() || result.alignment != indirectAlignment) :
        voidReturn ? (result.passing != Passing::Ignore || result.coercion.has_value()) :
        (result.passing != (boolReturn ? Passing::Extend : Passing::Direct) || !result.coercion ||
         !sameType(sameType, *result.coercion, plan.physicalReturn, 0))) || result.offset ||
        result.byValue || result.signExtend || result.zeroExtend != boolReturn || !result.expandedOffsets.empty())
        return false;
    std::size_t physicalIndex = 0;
    if (indirectAlignment)
    {
        if (plan.physicalParameters.empty()) return false;
        const auto& hidden = plan.physicalParameters[0];
        if (hidden.type.kind != TypeKind::Pointer || hidden.type.widthOrCount || hidden.type.packed ||
            !hidden.type.elements.empty() || !hidden.pointee || hidden.pointee->kind != TypeKind::Structure ||
            hidden.alignment != indirectAlignment || !hidden.structureReturn || hidden.byValue ||
            hidden.signExtend || hidden.zeroExtend || hidden.inRegister) return false;
        physicalIndex = 1;
    }
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
            const auto& firstType = plan.physicalParameters[physicalIndex].type;
            const bool doublePieces = allowDoublePieces && interleaved && firstType.kind == TypeKind::Double &&
                !firstType.widthOrCount && !firstType.packed && firstType.elements.empty();
            const std::uint64_t pieceSize = doublePieces ? 8 : 4;
            for (std::size_t i = 0; i < pieces; ++i)
            {
                if (argument.expandedOffsets[i] != i * pieceSize) return false;
                const auto& part = plan.physicalParameters[physicalIndex + i];
                const bool matchingPiece = doublePieces ? part.type.kind == TypeKind::Double && !part.type.widthOrCount &&
                    !part.type.packed && part.type.elements.empty() : scalar(part.type);
                if (!matchingPiece || part.pointee || part.alignment || part.structureReturn || part.byValue ||
                    part.signExtend || part.zeroExtend || part.inRegister || part.noAlias) return false;
            }
            physicalIndex += pieces;
            continue;
        }
        if (physicalIndex >= plan.physicalParameters.size()) return false;
        const auto& logical = argument;
        const auto& physical = plan.physicalParameters[physicalIndex];
        if (logical.passing == Passing::Indirect)
        {
            if (logical.coercion || logical.offset || !logical.alignment || logical.firstPhysical != physicalIndex ||
                logical.physicalCount != 1 || logical.byValue || logical.signExtend || logical.zeroExtend ||
                !logical.expandedOffsets.empty() || physical.type.kind != TypeKind::Pointer ||
                physical.type.widthOrCount || physical.type.packed || !physical.type.elements.empty() ||
                physical.pointee || physical.alignment != logical.alignment || physical.structureReturn ||
                physical.byValue || physical.signExtend || physical.zeroExtend || physical.inRegister || physical.noAlias) return false;
            ++physicalIndex;
            continue;
        }
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

inline bool regCallRecordMatchesDoubleExpansion(const Plan& plan, const CppType& result,
                                                const CppType& parameter, const LogicalValue& logical) noexcept
{
    return parameter.kind == CppTypeKind::Record && !parameter.isConst && !parameter.isVolatile &&
        !parameter.isRestrict && parameter.recordIdentity && parameter.recordIdentity->clangUsr &&
        !parameter.recordIdentity->clangUsr->empty() && !parameter.canonicalSpelling.empty() &&
        (result.kind != CppTypeKind::Record ||
            (result.recordIdentity && result.recordIdentity->clangUsr && !result.recordIdentity->clangUsr->empty() &&
             ((parameter.canonicalSpelling == result.canonicalSpelling) ==
              (parameter.recordIdentity->clangUsr == result.recordIdentity->clangUsr)))) &&
        logical.passing == Passing::Expand && logical.physicalCount >= 2 && logical.physicalCount <= 4 &&
        parameter.sizeBits == logical.physicalCount * 64 && parameter.alignmentBits == 64 &&
        logical.firstPhysical < plan.physicalParameters.size() &&
        plan.physicalParameters[logical.firstPhysical].type.kind == TypeKind::Double;
}

// Source identity is checked here; the imported value layout must also be
// reconciled by the caller before the physical integer carrier is used.
inline bool regCallDirectIntegerRecordMatches(const CppType& parameter, const LogicalValue& logical,
                                                const PhysicalParameter& physical) noexcept
{
    return parameter.kind == CppTypeKind::Record && !parameter.isConst && !parameter.isVolatile &&
        !parameter.isRestrict && parameter.sizeBits &&
        (*parameter.sizeBits == 8 || *parameter.sizeBits == 16 || *parameter.sizeBits == 32 || *parameter.sizeBits == 64) &&
        parameter.alignmentBits && *parameter.alignmentBits >= 8 && *parameter.alignmentBits <= *parameter.sizeBits &&
        (*parameter.alignmentBits & (*parameter.alignmentBits - 1)) == 0 &&
        parameter.recordIdentity && parameter.recordIdentity->clangUsr &&
        !parameter.recordIdentity->clangUsr->empty() && !parameter.canonicalSpelling.empty() &&
        logical.passing == Passing::Direct && logical.physicalCount == 1 && logical.coercion &&
        logical.coercion->kind == TypeKind::Integer && logical.coercion->widthOrCount == *parameter.sizeBits &&
        physical.type.kind == TypeKind::Integer && physical.type.widthOrCount == *parameter.sizeBits;
}

inline bool regCallIndirectRecordMatches(const CppType& parameter, const LogicalValue& logical,
                                         const PhysicalParameter& physical) noexcept
{
    return parameter.kind == CppTypeKind::Record && !parameter.isConst && !parameter.isVolatile &&
        !parameter.isRestrict && parameter.sizeBits && *parameter.sizeBits && *parameter.sizeBits % 8 == 0 &&
        parameter.alignmentBits && *parameter.alignmentBits >= 8 && *parameter.alignmentBits % 8 == 0 &&
        *parameter.sizeBits % *parameter.alignmentBits == 0 && parameter.recordIdentity &&
        parameter.recordIdentity->clangUsr && !parameter.recordIdentity->clangUsr->empty() &&
        !parameter.canonicalSpelling.empty() && logical.passing == Passing::Indirect &&
        logical.physicalCount == 1 && !logical.byValue && logical.alignment == *parameter.alignmentBits / 8 &&
        physical.type.kind == TypeKind::Pointer && physical.alignment == logical.alignment && !physical.byValue;
}

inline bool regCallRecordResultIdentityConsistent(const CppType& result, const CppType& parameter) noexcept
{
    if (result.kind != CppTypeKind::Record) return true;
    return result.recordIdentity && result.recordIdentity->clangUsr && !result.recordIdentity->clangUsr->empty() &&
        parameter.recordIdentity && parameter.recordIdentity->clangUsr && !parameter.recordIdentity->clangUsr->empty() &&
        ((parameter.canonicalSpelling == result.canonicalSpelling) ==
         (parameter.recordIdentity->clangUsr == result.recordIdentity->clangUsr));
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

inline bool isRegCallWin64DoubleRecordInterleaved(const Plan& plan, std::string_view symbol,
                                                  std::uint64_t size, std::uint64_t alignment) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return isRegCallWin64FloatRecordRoundTrip(plan, symbol, size, alignment,
        plan.logicalParameters.size() - records, true, records, true, true,
        false, false, false, false, false, false, 0, false, 0, true, true, true);
}

inline bool isRegCallWin64FloatRecordsReturningInt(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, true,
        false, false, false, false, false, 0, false, 0, true);
}

inline bool isRegCallWin64FloatRecordsReturningDouble(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true,
        false, true, false, false, false, false, 0, false, 0, true);
}

inline bool isRegCallWin64DoubleRecordsReturningDouble(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true,
        false, true, false, false, false, false, 0, false, 0, true);
}

inline bool isRegCallWin64IndirectRecordsReturningDouble(const Plan& plan, std::string_view symbol) noexcept
{
    bool indirect = false;
    for (const auto& logical : plan.logicalParameters)
    {
        if (logical.passing == Passing::Expand) return false;
        if (logical.passing == Passing::Indirect) indirect = true;
    }
    return indirect && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size(), true, 0, true, true,
        false, true, false, false, false, false, 0, false, 0, false, true);
}

inline bool isRegCallWin64IndirectRecordsReturningInt(const Plan& plan, std::string_view symbol) noexcept
{
    bool indirect = false;
    for (const auto& logical : plan.logicalParameters)
    {
        if (logical.passing == Passing::Expand) return false;
        if (logical.passing == Passing::Indirect) indirect = true;
    }
    return indirect && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size(), true, 0, true, true,
        true, false, false, false, false, false, 0, false, 0, false, true);
}

inline bool isRegCallWin64IndirectRecordsReturningVoid(const Plan& plan, std::string_view symbol) noexcept
{
    bool indirect = false;
    for (const auto& logical : plan.logicalParameters)
    {
        if (logical.passing == Passing::Expand) return false;
        if (logical.passing == Passing::Indirect) indirect = true;
    }
    return indirect && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size(), true, 0, true, true,
        false, false, true, false, false, false, 0, false, 0, false, true);
}

inline bool isRegCallWin64FloatRecordsReturningVoid(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, true,
        false, false, false, 0, false, 0, true);
}

inline bool isRegCallWin64FloatRecordsReturningFloat(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    bool indirect = false;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
        else if (logical.passing == Passing::Indirect) indirect = true;
    return (records || indirect) && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, false, true,
        false, false, 0, false, 0, true, !records);
}

inline bool isRegCallWin64FloatRecordsReturningInt64(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    bool indirect = false;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
        else if (logical.passing == Passing::Indirect) indirect = true;
    return (records || indirect) && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, false, false, true,
        false, 0, false, 0, true, !records);
}

inline bool isRegCallWin64FloatRecordsReturningPointer(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    bool indirect = false;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
        else if (logical.passing == Passing::Indirect) indirect = true;
    return (records || indirect) && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, false, false, false, true,
        0, false, 0, true, !records);
}

inline bool isRegCallWin64FloatRecordsReturningNarrowInteger(const Plan& plan, std::string_view symbol) noexcept
{
    const auto bits = plan.physicalReturn.widthOrCount;
    if (bits != 8 && bits != 16) return false;
    std::size_t records = 0;
    bool indirect = false;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
        else if (logical.passing == Passing::Indirect) indirect = true;
    return (records || indirect) && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, false, false, false, false,
        static_cast<unsigned>(bits), false, 0, true, !records);
}

inline bool isRegCallWin64FloatRecordsReturningBool(const Plan& plan, std::string_view symbol) noexcept
{
    std::size_t records = 0;
    bool indirect = false;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
        else if (logical.passing == Passing::Indirect) indirect = true;
    return (records || indirect) && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true, false, false, false, false, false, false,
        0, true, 0, true, !records);
}

// Physical validation only. The caller must also reconcile the returned
// record's identity, layout and C++ triviality before using this carrier.
inline bool isRegCallWin64FloatRecordsReturningCompactRecord(const Plan& plan, std::string_view symbol,
                                                              std::uint64_t size, std::uint64_t alignment) noexcept
{
    if ((size != 1 && size != 2 && size != 4 && size != 8) || !alignment || alignment > size ||
        (alignment & (alignment - 1)) || size % alignment || plan.physicalReturn.widthOrCount != size * 8)
        return false;
    if (size == 4) return isRegCallWin64FloatRecordsReturningInt(plan, symbol);
    if (size == 8) return isRegCallWin64FloatRecordsReturningInt64(plan, symbol);
    return isRegCallWin64FloatRecordsReturningNarrowInteger(plan, symbol);
}

// Checks only the hidden return and argument plan. The caller reconciles the
// record identity, imported layout and (for C++) its triviality separately.
inline bool isRegCallWin64FloatRecordsReturningIndirectRecord(const Plan& plan, std::string_view symbol,
                                                               std::uint64_t size, std::uint64_t alignment) noexcept
{
    if (!size || !alignment || (alignment & (alignment - 1)) || size % alignment ||
        size == 1 || size == 2 || size == 4 || size == 8) return false;
    std::size_t records = 0;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) ++records;
    return records && isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4,
        plan.logicalParameters.size() - records, true, records, true, true,
        false, false, false, false, false, false, 0, false, alignment);
}

inline bool regCallFloatRecordArgumentsMatch(const Plan& plan, const CppType& result,
                                             std::span<const CppType> parameters,
                                             bool allowDirectRecords = false,
                                             bool allowIndirectRecords = false) noexcept
{
    if (parameters.size() != plan.logicalParameters.size()) return false;
    for (std::size_t i = 0; i < parameters.size(); ++i)
    {
        const auto& logical = plan.logicalParameters[i];
        if (logical.passing == Passing::Expand)
        {
            if (!regCallRecordMatchesExpansion(result, parameters[i], logical) &&
                !regCallRecordMatchesDoubleExpansion(plan, result, parameters[i], logical)) return false;
        }
        else if (parameters[i].kind == CppTypeKind::Record && (allowDirectRecords || allowIndirectRecords))
        {
            if (!regCallRecordResultIdentityConsistent(result, parameters[i]) || logical.firstPhysical >= plan.physicalParameters.size() ||
                !((allowDirectRecords && regCallDirectIntegerRecordMatches(parameters[i], logical,
                    plan.physicalParameters[logical.firstPhysical])) ||
                  (allowIndirectRecords && regCallIndirectRecordMatches(parameters[i], logical,
                    plan.physicalParameters[logical.firstPhysical])))) return false;
        }
        else if (logical.passing == Passing::Indirect || logical.firstPhysical >= plan.physicalParameters.size() ||
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
        if (plan.logicalParameters[i].passing == Passing::Indirect) return false;
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

// Scalar or void return; every logical parameter is a scalar, a record passed directly as one
// integer, or a record passed indirectly; nothing is expanded. A plan cannot tell an integer scalar
// from a record carried as an integer, so the caller states whether the source signature has a
// record by value and must reconcile every parameter with its source type.
inline bool isRegCallWin64NonExpandedRecordsReturningScalar(const Plan& plan, std::string_view symbol,
                                                            bool sourceHasRecordValue) noexcept
{
    if (!sourceHasRecordValue) return false;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) return false;
    const auto& result = plan.physicalReturn;
    if (result.packed || !result.elements.empty()) return false;
    const bool isVoid = result.kind == TypeKind::Void, isDouble = result.kind == TypeKind::Double,
               isFloat = result.kind == TypeKind::Float, isPointer = result.kind == TypeKind::Pointer,
               isInteger = result.kind == TypeKind::Integer;
    if ((!isVoid && !isDouble && !isFloat && !isPointer && !isInteger) || (!isInteger && result.widthOrCount)) return false;
    const auto width = isInteger ? result.widthOrCount : 0;
    if (isInteger && width != 1 && width != 8 && width != 16 && width != 32 && width != 64) return false;
    return isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4, plan.logicalParameters.size(), true, 0, true, true,
        width == 32, isDouble, isVoid, isFloat, width == 64, isPointer,
        (width == 8 || width == 16) ? static_cast<unsigned>(width) : 0U, width == 1, 0, false, true);
}

// Record result of a non-expanded regcall plan: a compact record returned directly as one integer of
// its own size, or a record returned through the hidden sret pointer. Parameters as above.
inline bool isRegCallWin64NonExpandedRecordsReturningRecord(const Plan& plan, std::string_view symbol,
                                                            std::uint64_t sizeBytes, std::uint64_t alignment) noexcept
{
    if (!sizeBytes || !alignment || (alignment & (alignment - 1)) || sizeBytes % alignment) return false;
    for (const auto& logical : plan.logicalParameters)
        if (logical.passing == Passing::Expand) return false;
    const auto& result = plan.physicalReturn;
    if (result.packed || !result.elements.empty()) return false;
    if (result.kind == TypeKind::Integer)
    {
        const auto width = result.widthOrCount;
        if ((sizeBytes != 1 && sizeBytes != 2 && sizeBytes != 4 && sizeBytes != 8) || width != sizeBytes * 8) return false;
        return isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4, plan.logicalParameters.size(), true, 0, true, true,
            width == 32, false, false, false, width == 64, false,
            (width == 8 || width == 16) ? static_cast<unsigned>(width) : 0U, false, 0, false, true);
    }
    if (result.kind != TypeKind::Void || result.widthOrCount || plan.physicalParameters.empty() ||
        !plan.physicalParameters.front().pointee) return false;
    const auto& pointee = *plan.physicalParameters.front().pointee;
    if (pointee.kind != TypeKind::Structure && pointee.kind != TypeKind::Array) return false;
    return isRegCallWin64FloatRecordRoundTrip(plan, symbol, 8, 4, plan.logicalParameters.size(), true, 0, true, true,
        false, false, false, false, false, false, 0U, false, alignment, false, true);
}

// The physical return of a non-expanded regcall plan agrees with a NativeSharp builtin result key.
inline bool regCallPhysicalReturnMatchesKey(const Type& result, std::string_view key) noexcept
{
    if (result.packed || !result.elements.empty()) return false;
    if (key == "builtin:void") return result.kind == TypeKind::Void;
    if (key == "builtin:double") return result.kind == TypeKind::Double && !result.widthOrCount;
    if (key == "builtin:float") return result.kind == TypeKind::Float && !result.widthOrCount;
    if (key.ends_with('*')) return result.kind == TypeKind::Pointer && !result.widthOrCount;
    if (result.kind != TypeKind::Integer) return false;
    if (key == "builtin:bool") return result.widthOrCount == 1;
    if (key == "builtin:sbyte" || key == "builtin:byte") return result.widthOrCount == 8;
    if (key == "builtin:short" || key == "builtin:ushort") return result.widthOrCount == 16;
    if (key == "builtin:int" || key == "builtin:uint") return result.widthOrCount == 32;
    if (key == "builtin:long" || key == "builtin:ulong") return result.widthOrCount == 64;
    return false;
}

// Source-checked form of the plan above: at least one record by value, a scalar or void result
// whose physical carrier matches the source, and every argument reconciled with the plan.
inline bool isRegCallWin64RecordsWithScalarReturn(const Plan& plan, const CppFunctionType& signature,
                                                  std::string_view symbol) noexcept
{
    const auto& result = signature.returnType;
    const bool hasRecord = std::any_of(signature.parameterTypes.begin(), signature.parameterTypes.end(),
        [](const CppType& type) { return type.kind == CppTypeKind::Record; });
    if (!hasRecord || signature.callingConvention != "regcall" || !signature.hasPrototype || signature.variadic ||
        result.kind == CppTypeKind::Record || plan.logicalParameters.size() != signature.parameterTypes.size() ||
        !isRegCallWin64NonExpandedRecordsReturningScalar(plan, symbol, true)) return false;
    if (result.kind == CppTypeKind::Void)
    {
        if (result.canonicalSpelling != "void" || result.isConst || result.isVolatile || result.isRestrict) return false;
    }
    else if (!regCallFloatRecordScalarMatches(plan.physicalReturn, result) ||
             (result.kind == CppTypeKind::Pointer && !regCallFloatRecordScalarPointerReturnSupported(result)))
        return false;
    return regCallFloatRecordArgumentsMatch(plan, result, signature.parameterTypes, true, true);
}

// Source-checked form with a record result (compact or indirect) and any mix of scalars and records
// passed directly as one integer or indirectly.
inline bool isRegCallWin64RecordsWithRecordReturn(const Plan& plan, const CppFunctionType& signature,
                                                  std::string_view symbol) noexcept
{
    const auto& result = signature.returnType;
    if (result.kind != CppTypeKind::Record || result.isConst || result.isVolatile || result.isRestrict ||
        !result.recordIdentity || !result.recordIdentity->clangUsr || result.recordIdentity->clangUsr->empty() ||
        result.recordIdentity->anonymousContext || result.recordIdentity->specializationContext ||
        !result.sizeBits || *result.sizeBits % 8 || !result.alignmentBits || *result.alignmentBits % 8 ||
        signature.callingConvention != "regcall" || !signature.hasPrototype || signature.variadic ||
        plan.logicalParameters.size() != signature.parameterTypes.size() ||
        !isRegCallWin64NonExpandedRecordsReturningRecord(plan, symbol, *result.sizeBits / 8, *result.alignmentBits / 8))
        return false;
    return regCallFloatRecordArgumentsMatch(plan, result, signature.parameterTypes, true, true);
}

inline bool isRegCallWin64NonExpandedRecordsPlan(const Plan& plan, const CppFunctionType& signature,
                                                 std::string_view symbol) noexcept
{
    return isRegCallWin64RecordsWithScalarReturn(plan, signature, symbol) ||
        isRegCallWin64RecordsWithRecordReturn(plan, signature, symbol);
}

// A single C record, passed and returned with exactly the normal Win64
// carrier. This is a physical-plan check, not a record identity check.
inline bool isWin64RecordRoundTripForConvention(const Plan& plan, std::string_view symbol,
                                                 std::uint64_t sizeBytes, std::uint64_t alignment,
                                                 unsigned expectedConvention) noexcept
{
    if (symbol.empty() || plan.symbol != symbol || plan.callingConvention != expectedConvention || plan.variadic ||
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
inline bool isRegCallWin64RecordRoundTrip(const Plan& plan, std::string_view symbol,
                                          std::uint64_t sizeBytes, std::uint64_t alignment) noexcept
{
    return isWin64RecordRoundTripForConvention(plan, symbol, sizeBytes, alignment, 92);
}
inline bool isPreserveNoneWin64RecordRoundTrip(const Plan& plan, std::string_view symbol,
                                               std::uint64_t sizeBytes, std::uint64_t alignment) noexcept
{
    return isWin64RecordRoundTripForConvention(plan, symbol, sizeBytes, alignment, 21);
}
inline bool isPreserveNoneCppRecordRoundTrip(const Plan& plan, const CppFunctionType& signature,
                                              std::string_view symbol) noexcept
{
    if (!signature.hasPrototype || signature.variadic || signature.callingConvention != "preserve_none" ||
        signature.parameterTypes.size() != 1) return false;
    const auto& result = signature.returnType;
    const auto& argument = signature.parameterTypes[0];
    if (result.kind != CppTypeKind::Record || argument.kind != CppTypeKind::Record ||
        result.isConst || result.isVolatile || result.isRestrict || argument.isConst || argument.isVolatile || argument.isRestrict ||
        !result.recordIdentity || !argument.recordIdentity || result.recordIdentity->anonymousContext ||
        argument.recordIdentity->anonymousContext || result.recordIdentity->specializationContext ||
        argument.recordIdentity->specializationContext || !result.recordIdentity->clangUsr ||
        result.recordIdentity->clangUsr->empty() || result.recordIdentity->clangUsr != argument.recordIdentity->clangUsr ||
        result.recordIdentity->name.empty() || result.recordIdentity->name != argument.recordIdentity->name ||
        result.recordIdentity->namespaceSegments != argument.recordIdentity->namespaceSegments ||
        result.recordIdentity->containingRecords != argument.recordIdentity->containingRecords ||
        !result.sizeBits || *result.sizeBits % 8 || !result.alignmentBits || *result.alignmentBits % 8 ||
        result.sizeBits != argument.sizeBits || result.alignmentBits != argument.alignmentBits) return false;
    return isPreserveNoneWin64RecordRoundTrip(plan, symbol, *result.sizeBits / 8, *result.alignmentBits / 8);
}
inline bool isPreserveNoneCppRecordPointerRoundTrip(const Plan& plan, const CppFunctionType& signature) noexcept
{
    return isPreserveNoneCppRecordRoundTrip(plan, signature, "__nsc_pointer_abi_probe");
}
inline bool isCppScalarPlanForConvention(const Plan& plan, const CppFunctionType& signature, std::string_view symbol,
                                         unsigned convention, std::string_view conventionName) noexcept
{
    if (symbol.empty() || plan.symbol != symbol || plan.callingConvention != convention || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        !signature.hasPrototype || signature.variadic || signature.callingConvention != conventionName ||
        signature.parameterTypes.size() != plan.logicalParameters.size() ||
        signature.parameterTypes.size() != plan.physicalParameters.size() || signature.parameterTypes.size() > 32)
        return false;
    const auto scalar = [](const Type& physical, const CppType& observed) {
        return !physical.packed && physical.elements.empty() &&
            ((physical.kind != TypeKind::Float && physical.kind != TypeKind::Double) || physical.widthOrCount == 0) &&
            observed.kind != CppTypeKind::Record &&
            regCallFloatRecordScalarMatches(physical, observed);
    };
    const auto& result = signature.returnType;
    if (result.kind == CppTypeKind::Void)
    {
        if (result.canonicalSpelling != "void" || result.isConst || result.isVolatile || result.isRestrict ||
            plan.physicalReturn.kind != TypeKind::Void || plan.physicalReturn.widthOrCount ||
            plan.physicalReturn.packed || !plan.physicalReturn.elements.empty() ||
            plan.returnSignExtend || plan.returnZeroExtend || plan.logicalReturn.passing != Passing::Ignore ||
            plan.logicalReturn.coercion || plan.logicalReturn.offset || plan.logicalReturn.byValue ||
            plan.logicalReturn.physicalCount || plan.logicalReturn.signExtend || plan.logicalReturn.zeroExtend ||
            !plan.logicalReturn.expandedOffsets.empty()) return false;
    }
    else if (!scalar(plan.physicalReturn, result) || plan.returnSignExtend ||
        plan.returnZeroExtend != (result.kind == CppTypeKind::Boolean) ||
        (plan.logicalReturn.passing != Passing::Direct && plan.logicalReturn.passing != Passing::Extend) ||
        plan.logicalReturn.offset || plan.logicalReturn.byValue ||
        !plan.logicalReturn.expandedOffsets.empty() ||
        (plan.logicalReturn.coercion && !scalar(*plan.logicalReturn.coercion, result)) ||
        plan.logicalReturn.signExtend != plan.returnSignExtend ||
        plan.logicalReturn.zeroExtend != plan.returnZeroExtend) return false;
    for (std::size_t i = 0; i < signature.parameterTypes.size(); ++i)
    {
        const auto& observed = signature.parameterTypes[i];
        const auto& logical = plan.logicalParameters[i];
        const auto& physical = plan.physicalParameters[i];
        if ((logical.passing != Passing::Direct && logical.passing != Passing::Extend) || logical.offset ||
            logical.byValue || !logical.expandedOffsets.empty() ||
            (logical.coercion && !scalar(*logical.coercion, observed)) ||
            logical.firstPhysical != i || logical.physicalCount != 1 ||
            physical.structureReturn || physical.byValue || physical.inRegister || physical.noAlias || physical.pointee ||
            physical.signExtend || physical.zeroExtend != (observed.kind == CppTypeKind::Boolean) ||
            logical.signExtend != physical.signExtend || logical.zeroExtend != physical.zeroExtend ||
            !scalar(physical.type, observed)) return false;
    }
    return true;
}
inline bool isPreserveNoneCppScalarPlan(const Plan& plan, const CppFunctionType& signature,
                                         std::string_view symbol) noexcept
{
    return isCppScalarPlanForConvention(plan, signature, symbol, 21, "preserve_none");
}
// LLVM calling conventions that Clang emits on Windows x64 and NativeSharp lowers only for scalar
// and pointer signatures checked by the plan above: swiftcc, swifttailcc and intel_ocl_bicc.
inline unsigned cppPlannedScalarConventionId(std::string_view conventionName) noexcept
{
    return conventionName == "swiftcall" ? 16U : conventionName == "swiftasynccall" ? 20U :
        conventionName == "intel_ocl_bicc" ? 77U : 0U;
}
// LLVM calling convention to stamp on a declaration or call whose plan came from that path, or 0.
inline unsigned cppPlannedScalarConventionOfPlan(const Plan& plan) noexcept
{
    return plan.callingConvention == 16U || plan.callingConvention == 20U || plan.callingConvention == 77U ?
        plan.callingConvention : 0U;
}
inline bool isCppPlannedScalarConventionPlan(const Plan& plan, const CppFunctionType& signature,
                                             std::string_view symbol) noexcept
{
    const auto convention = cppPlannedScalarConventionId(signature.callingConvention);
    return convention && isCppScalarPlanForConvention(plan, signature, symbol, convention, signature.callingConvention);
}
// Compute the layout of the bounded scalar/aggregate LLVM types represented by
// this plan on the Win64 data layout. Unsupported types are not inferred.
inline std::optional<std::pair<std::uint64_t, std::uint64_t>>
preserveNonePhysicalLayout(const Type& type, unsigned depth = 0)
{
    constexpr std::uint64_t limit = 1U << 20;
    if (depth > 16) return std::nullopt;
    if (type.kind == TypeKind::Integer)
    {
        if (type.packed || !type.elements.empty() ||
            (type.widthOrCount != 8 && type.widthOrCount != 16 && type.widthOrCount != 32 && type.widthOrCount != 64))
            return std::nullopt;
        const auto bytes = type.widthOrCount / 8;
        return std::pair{bytes, bytes};
    }
    if (type.kind == TypeKind::Float || type.kind == TypeKind::Double || type.kind == TypeKind::Pointer)
    {
        if (type.packed || type.widthOrCount || !type.elements.empty()) return std::nullopt;
        const std::uint64_t bytes = type.kind == TypeKind::Float ? 4 : 8;
        return std::pair{bytes, bytes};
    }
    if (type.kind == TypeKind::Array)
    {
        if (type.packed || type.elements.size() != 1 || !type.widthOrCount || type.widthOrCount > limit) return std::nullopt;
        const auto element = preserveNonePhysicalLayout(type.elements.front(), depth + 1);
        if (!element || element->first > limit / type.widthOrCount) return std::nullopt;
        return std::pair{element->first * type.widthOrCount, element->second};
    }
    if (type.kind == TypeKind::Vector)
    {
        if (type.packed || type.elements.size() != 1 || !type.widthOrCount || type.widthOrCount > limit)
            return std::nullopt;
        const auto& lane = type.elements.front();
        if (lane.kind != TypeKind::Integer && lane.kind != TypeKind::Float && lane.kind != TypeKind::Double)
            return std::nullopt;
        const auto element = preserveNonePhysicalLayout(lane, depth + 1);
        if (!element || element->first > limit / type.widthOrCount) return std::nullopt;
        const auto storeSize = element->first * type.widthOrCount;
        std::uint64_t alignment = 1;
        while (alignment < storeSize)
        {
            if (alignment > limit / 2) return std::nullopt;
            alignment *= 2;
        }
        // The Win64 data layout gives fixed vectors a power-of-two ABI alignment;
        // their allocation size is rounded up to that alignment.
        return std::pair{alignment, alignment};
    }
    if (type.kind != TypeKind::Structure || type.widthOrCount || type.elements.empty() || type.elements.size() > 1024)
        return std::nullopt;
    std::uint64_t size = 0, alignment = 1;
    for (const auto& elementType : type.elements)
    {
        const auto element = preserveNonePhysicalLayout(elementType, depth + 1);
        if (!element) return std::nullopt;
        const auto fieldAlignment = type.packed ? 1 : element->second;
        alignment = std::max(alignment, fieldAlignment);
        const auto padding = (fieldAlignment - size % fieldAlignment) % fieldAlignment;
        if (padding > limit - size || element->first > limit - size - padding) return std::nullopt;
        size += padding + element->first;
    }
    const auto padding = (alignment - size % alignment) % alignment;
    if (padding > limit - size) return std::nullopt;
    return std::pair{size + padding, alignment};
}
// Validate and remove the hidden result after reconciling its physical layout.
inline bool normalizePreserveNoneIndirectResult(Plan& plan, std::uint64_t sizeBytes, std::uint64_t alignment)
{
    if (!sizeBytes || !alignment || (alignment & (alignment - 1)) || sizeBytes % alignment ||
        plan.dataLayout != "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128" ||
        plan.physicalReturn.kind != TypeKind::Void || plan.physicalReturn.widthOrCount ||
        plan.physicalReturn.packed || !plan.physicalReturn.elements.empty() ||
        plan.returnSignExtend || plan.returnZeroExtend || plan.physicalParameters.empty()) return false;
    const auto& logical = plan.logicalReturn;
    const auto& physical = plan.physicalParameters.front();
    if (logical.passing != Passing::Indirect || logical.alignment != alignment || logical.offset ||
        logical.coercion || logical.byValue || logical.signExtend || logical.zeroExtend ||
        logical.firstPhysical || logical.physicalCount || !logical.expandedOffsets.empty() ||
        physical.type.kind != TypeKind::Pointer || physical.type.widthOrCount || physical.type.packed ||
        !physical.type.elements.empty() || !physical.structureReturn || physical.alignment != alignment ||
        physical.byValue || physical.signExtend || physical.zeroExtend || physical.inRegister || physical.noAlias ||
        !physical.pointee) return false;
    const auto pointeeLayout = preserveNonePhysicalLayout(*physical.pointee);
    if (!pointeeLayout || pointeeLayout->first != sizeBytes || pointeeLayout->second > alignment) return false;
    for (auto& parameter : plan.logicalParameters)
    {
        if (!parameter.firstPhysical) return false;
        --parameter.firstPhysical;
    }
    plan.physicalParameters.erase(plan.physicalParameters.begin());
    plan.logicalReturn = LogicalValue{};
    return true;
}
// Scalar or compact-record return with C++ records by value. Record identity and
// eligibility must be independently reconciled against the importing session.
inline bool isPreserveNoneCppScalarWithRecord(const Plan& plan, const CppFunctionType& signature,
                                              std::string_view symbol)
{
    CppFunctionType physicalSignature = signature;
    Plan normalized = plan;
    const auto consistentRecord = [](const CppType& first, const CppType& second) {
        if (first.kind != CppTypeKind::Record || second.kind != CppTypeKind::Record) return true;
        if (!regCallRecordResultIdentityConsistent(first, second)) return false;
        return first.recordIdentity->clangUsr != second.recordIdentity->clangUsr ||
            (first.sizeBits == second.sizeBits && first.alignmentBits == second.alignmentBits);
    };
    for (std::size_t i = 0; i < signature.parameterTypes.size(); ++i)
    {
        if (!consistentRecord(signature.returnType, signature.parameterTypes[i])) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (!consistentRecord(signature.parameterTypes[j], signature.parameterTypes[i])) return false;
    }
    const auto compactRecordReturn = [](CppType& result) {
        if (result.kind != CppTypeKind::Record) return false;
        if (result.isConst || result.isVolatile || result.isRestrict || !result.recordIdentity ||
            !result.recordIdentity->clangUsr || result.recordIdentity->clangUsr->empty() ||
            result.recordIdentity->anonymousContext || result.recordIdentity->specializationContext ||
            !result.sizeBits || !result.alignmentBits ||
            (*result.sizeBits != 8 && *result.sizeBits != 16 && *result.sizeBits != 32 && *result.sizeBits != 64) ||
            (*result.alignmentBits != 8 && *result.alignmentBits != 16 && *result.alignmentBits != 32 && *result.alignmentBits != 64) ||
            *result.sizeBits % *result.alignmentBits != 0) return false;
        const auto bits = *result.sizeBits;
        result = CppType{};
        result.kind = CppTypeKind::UnsignedInteger;
        result.canonicalSpelling = bits == 8 ? "unsigned char" : bits == 16 ? "unsigned short" :
            bits == 32 ? "unsigned int" : "unsigned long long";
        result.sizeBits = result.alignmentBits = bits;
        return true;
    };
    const bool recordReturn = physicalSignature.returnType.kind == CppTypeKind::Record;
    if (recordReturn && !compactRecordReturn(physicalSignature.returnType))
    {
        const auto& result = signature.returnType;
        if (result.isConst || result.isVolatile || result.isRestrict || !result.recordIdentity ||
            !result.recordIdentity->clangUsr || result.recordIdentity->clangUsr->empty() ||
            result.recordIdentity->anonymousContext || result.recordIdentity->specializationContext ||
            !result.sizeBits || *result.sizeBits % 8 || !result.alignmentBits || *result.alignmentBits % 8 ||
            !normalizePreserveNoneIndirectResult(normalized, *result.sizeBits / 8, *result.alignmentBits / 8)) return false;
        physicalSignature.returnType = CppType{};
        physicalSignature.returnType.kind = CppTypeKind::Void;
        physicalSignature.returnType.canonicalSpelling = "void";
    }
    unsigned records = 0;
    for (std::size_t i = 0; i < physicalSignature.parameterTypes.size(); ++i)
    {
        auto& parameter = physicalSignature.parameterTypes[i];
        if (parameter.kind != CppTypeKind::Record) continue;
        if (++records > 32 || parameter.isConst || parameter.isVolatile || parameter.isRestrict ||
            !parameter.recordIdentity || !parameter.recordIdentity->clangUsr ||
            parameter.recordIdentity->clangUsr->empty() || parameter.recordIdentity->anonymousContext ||
            parameter.recordIdentity->specializationContext || !parameter.sizeBits || !parameter.alignmentBits ||
            *parameter.sizeBits % 8 || *parameter.alignmentBits % 8 || !*parameter.alignmentBits ||
            (*parameter.alignmentBits & (*parameter.alignmentBits - 1)) ||
            *parameter.sizeBits % *parameter.alignmentBits != 0 || i >= normalized.logicalParameters.size() ||
            i >= normalized.physicalParameters.size()) return false;
        const auto bits = *parameter.sizeBits;
        const bool compact = bits == 8 || bits == 16 || bits == 32 || bits == 64;
        if (compact)
        {
            parameter = CppType{};
            parameter.kind = CppTypeKind::UnsignedInteger;
            parameter.canonicalSpelling = bits == 8 ? "unsigned char" : bits == 16 ? "unsigned short" :
                bits == 32 ? "unsigned int" : "unsigned long long";
            parameter.sizeBits = parameter.alignmentBits = bits;
        }
        else
        {
            auto& logical = normalized.logicalParameters[i];
            auto& physical = normalized.physicalParameters[i];
            const auto alignment = *parameter.alignmentBits / 8;
            if (logical.passing != Passing::Indirect || logical.alignment != alignment || logical.offset ||
                logical.coercion || logical.byValue || logical.signExtend || logical.zeroExtend ||
                logical.firstPhysical != i || logical.physicalCount != 1 || !logical.expandedOffsets.empty() ||
                physical.type.kind != TypeKind::Pointer || physical.type.widthOrCount || physical.pointee ||
                physical.alignment != alignment || physical.structureReturn || physical.byValue ||
                physical.signExtend || physical.zeroExtend || physical.inRegister || physical.noAlias) return false;
            CppType pointee;
            pointee.kind = CppTypeKind::SignedInteger;
            pointee.canonicalSpelling = "int";
            pointee.sizeBits = pointee.alignmentBits = 32;
            parameter = CppType{};
            parameter.kind = CppTypeKind::Pointer;
            parameter.canonicalSpelling = "int *";
            parameter.sizeBits = parameter.alignmentBits = 64;
            parameter.element = std::make_shared<const CppType>(std::move(pointee));
            logical.passing = Passing::Direct;
            logical.alignment = 0;
            physical.alignment = 0;
        }
    }
    return (records > 0 || recordReturn) && isPreserveNoneCppScalarPlan(normalized, physicalSignature, symbol);
}
// Reconcile the physical plan again with the NativeSharp scalar types used by emission.
inline bool isPreserveNoneScalarPlanForTypeKeys(const Plan& plan, std::string_view result,
                                               std::span<const std::string> parameters)
{
    const auto cppType = [](std::string_view key) -> std::optional<CppType> {
        unsigned pointerDepth = 0;
        while (key.ends_with('*'))
        {
            if (++pointerDepth > 16) return std::nullopt;
            key.remove_suffix(1);
        }
        CppType type;
        if (key == "builtin:void") { type.kind = CppTypeKind::Void; type.canonicalSpelling = "void"; }
        else if (key == "builtin:bool") { type.kind = CppTypeKind::Boolean; type.canonicalSpelling = "bool"; type.sizeBits = type.alignmentBits = 8; }
        else if (key == "builtin:sbyte") { type.kind = CppTypeKind::SignedInteger; type.canonicalSpelling = "signed char"; type.sizeBits = type.alignmentBits = 8; }
        else if (key == "builtin:byte") { type.kind = CppTypeKind::UnsignedInteger; type.canonicalSpelling = "unsigned char"; type.sizeBits = type.alignmentBits = 8; }
        else if (key == "builtin:short") { type.kind = CppTypeKind::SignedInteger; type.canonicalSpelling = "short"; type.sizeBits = type.alignmentBits = 16; }
        else if (key == "builtin:ushort") { type.kind = CppTypeKind::UnsignedInteger; type.canonicalSpelling = "unsigned short"; type.sizeBits = type.alignmentBits = 16; }
        else if (key == "builtin:int") { type.kind = CppTypeKind::SignedInteger; type.canonicalSpelling = "int"; type.sizeBits = type.alignmentBits = 32; }
        else if (key == "builtin:uint") { type.kind = CppTypeKind::UnsignedInteger; type.canonicalSpelling = "unsigned int"; type.sizeBits = type.alignmentBits = 32; }
        else if (key == "builtin:long") { type.kind = CppTypeKind::SignedInteger; type.canonicalSpelling = "long long"; type.sizeBits = type.alignmentBits = 64; }
        else if (key == "builtin:ulong") { type.kind = CppTypeKind::UnsignedInteger; type.canonicalSpelling = "unsigned long long"; type.sizeBits = type.alignmentBits = 64; }
        else if (key == "builtin:float") { type.kind = CppTypeKind::Floating; type.canonicalSpelling = "float"; type.sizeBits = type.alignmentBits = 32; }
        else if (key == "builtin:double") { type.kind = CppTypeKind::Floating; type.canonicalSpelling = "double"; type.sizeBits = type.alignmentBits = 64; }
        else if (pointerDepth && key.starts_with("type:") && key.size() > 5)
        {
            // The caller has already reconciled this identity with the imported alias.
            // Its machine carrier is the same as any other pointer.
            type.kind = CppTypeKind::SignedInteger;
            type.canonicalSpelling = "int";
            type.sizeBits = type.alignmentBits = 32;
        }
        else return std::nullopt;
        for (unsigned i = 0; i < pointerDepth; ++i)
        {
            CppType pointer;
            pointer.kind = CppTypeKind::Pointer;
            pointer.canonicalSpelling = type.canonicalSpelling + (i == 0 ? " *" : "*");
            pointer.sizeBits = pointer.alignmentBits = 64;
            pointer.element = std::make_shared<const CppType>(std::move(type));
            type = std::move(pointer);
        }
        return type;
    };
    const auto returnType = cppType(result);
    if (!returnType) return false;
    CppFunctionType signature;
    signature.returnType = *returnType;
    signature.callingConvention = "preserve_none";
    signature.hasPrototype = true;
    for (const auto& key : parameters)
    {
        const auto parameter = cppType(key);
        if (!parameter || parameter->kind == CppTypeKind::Void) return false;
        signature.parameterTypes.push_back(*parameter);
    }
    return isPreserveNoneCppScalarPlan(plan, signature, "__nsc_pointer_abi_probe");
}
}
