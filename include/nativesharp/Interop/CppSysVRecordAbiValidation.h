#pragma once

#include "nativesharp/Interop/CppAbiPlanQueries.h"
#include "nativesharp/Interop/CppRecordDeclaration.h"
#include "nativesharp/Interop/CppRegCallAbiValidation.h"

#include <algorithm>
#include <span>

namespace nativesharp::interop::abi_plan
{
// Reconcile floating slots with observed C++ fields. Preserve the previous
// integer-only rules when neither side contains direct floating fields.
inline bool sysVFloatingRecordFieldsMatch(std::span<const CppRecordField> fields, const Type& pointee)
{
    const bool hasFloating = std::any_of(pointee.elements.begin(), pointee.elements.end(),
        [](const auto& element) { return element.kind == TypeKind::Float || element.kind == TypeKind::Double; }) ||
        std::any_of(fields.begin(), fields.end(),
        [](const auto& field) { return field.type.kind == CppTypeKind::Floating; });
    if (!hasFloating) return true;
    if (fields.size() != pointee.elements.size() || fields.empty()) return false;
    const auto bits = pointee.elements[0].kind == TypeKind::Float ? 32U :
        pointee.elements[0].kind == TypeKind::Double ? 64U : pointee.elements[0].widthOrCount;
    if (bits != 32 && bits != 64) return false;
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        const auto& field = fields[i];
        const auto& physical = pointee.elements[i];
        if (field.bitWidth || field.offsetBits != i * bits || field.type.sizeBits != bits ||
            field.type.alignmentBits != bits || field.type.isVolatile || field.type.isRestrict) return false;
        const bool integer = (field.type.kind == CppTypeKind::SignedInteger || field.type.kind == CppTypeKind::UnsignedInteger) &&
            physical.kind == TypeKind::Integer && physical.widthOrCount == bits;
        const bool floating = field.type.kind == CppTypeKind::Floating &&
            ((bits == 32 && field.type.canonicalSpelling == "float" && physical.kind == TypeKind::Float) ||
             (bits == 64 && field.type.canonicalSpelling == "double" && physical.kind == TypeKind::Double));
        if (!integer && !floating) return false;
    }
    return true;
}
inline bool sysVDirectFloatingFieldsMatch(std::span<const CppRecordField> fields, const Type& physical) noexcept
{
    const auto vector2 = [](const Type& type) {
        return type.kind == TypeKind::Vector && type.widthOrCount == 2 && type.elements.size() == 1 &&
            type.elements[0].kind == TypeKind::Float && type.elements[0].widthOrCount == 0;
    };
    unsigned bits = physical.kind == TypeKind::Double ? 64U : 32U;
    std::size_t count = vector2(physical) ? 2U : 1U;
    if (physical.kind == TypeKind::Structure && !physical.packed && physical.elements.size() == 2)
    {
        const auto& first = physical.elements[0];
        const auto& second = physical.elements[1];
        const bool doubleFirst = first.kind == TypeKind::Double && second.kind == TypeKind::Integer && second.widthOrCount == 32;
        const bool integerFirst = first.kind == TypeKind::Integer && first.widthOrCount == 32 && second.kind == TypeKind::Double;
        if (doubleFirst || integerFirst)
        {
            if (fields.size() != 2) return false;
            for (std::size_t i = 0; i < fields.size(); ++i)
            {
                const auto& field = fields[i];
                const bool floating = (i == 0) == doubleFirst;
                if (field.bitWidth || field.offsetBits != i * 64 || field.type.isVolatile || field.type.isRestrict ||
                    field.type.sizeBits != (floating ? 64U : 32U) || field.type.alignmentBits != (floating ? 64U : 32U)) return false;
                if (floating ? (field.type.kind != CppTypeKind::Floating || field.type.canonicalSpelling != "double") :
                    !((field.type.kind == CppTypeKind::SignedInteger && field.type.canonicalSpelling == "int") ||
                      (field.type.kind == CppTypeKind::UnsignedInteger && field.type.canonicalSpelling == "unsigned int"))) return false;
            }
            return true;
        }
        const bool firstInteger = first.kind == TypeKind::Integer && first.widthOrCount == 64;
        const bool secondInteger = second.kind == TypeKind::Integer &&
            (second.widthOrCount == 32 || second.widthOrCount == 64);
        if ((vector2(first) || firstInteger) &&
            (vector2(second) || second.kind == TypeKind::Float || secondInteger))
        {
            const std::size_t lastCount = vector2(second) || (secondInteger && second.widthOrCount == 64) ? 2U : 1U;
            if (fields.size() != 2U + lastCount) return false;
            const auto floating = [&](std::size_t index) {
                return fields[index].type.kind == CppTypeKind::Floating && fields[index].type.canonicalSpelling == "float";
            };
            for (std::size_t i = 0; i < fields.size(); ++i)
            {
                const auto& field = fields[i];
                const bool integer = (field.type.kind == CppTypeKind::SignedInteger && field.type.canonicalSpelling == "int") ||
                    (field.type.kind == CppTypeKind::UnsignedInteger && field.type.canonicalSpelling == "unsigned int");
                if (field.bitWidth || field.offsetBits != i * 32 || field.type.sizeBits != 32 ||
                    field.type.alignmentBits != 32 || field.type.isVolatile || field.type.isRestrict ||
                    (!integer && !floating(i))) return false;
            }
            const bool firstAllFloat = floating(0) && floating(1);
            const bool lastAllFloat = floating(2) && (lastCount == 1 || floating(3));
            return (vector2(first) == firstAllFloat) &&
                ((vector2(second) || second.kind == TypeKind::Float) == lastAllFloat);
        }
        if (vector2(first) && (second.kind == TypeKind::Float || vector2(second)))
            count = vector2(second) ? 4U : 3U;
        else if (first.kind == TypeKind::Double && second.kind == TypeKind::Double)
            bits = 64U, count = 2U;
        else return false;
    }
    else if (!vector2(physical) && physical.kind != TypeKind::Float && physical.kind != TypeKind::Double)
        return false;
    if (fields.size() != count) return false;
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        const auto& field = fields[i];
        if (field.bitWidth || field.offsetBits != i * bits || field.type.kind != CppTypeKind::Floating ||
            field.type.canonicalSpelling != (bits == 32 ? "float" : "double") ||
            field.type.sizeBits != bits || field.type.alignmentBits != bits ||
            field.type.isVolatile || field.type.isRestrict) return false;
    }
    return true;
}
// Physical and declared identity only; callers must also establish provenance,
// observed layout and C++ trivial-value eligibility.
inline bool isSysVScalarRecordPointee(const Type& type, std::uint64_t sizeBits, std::uint64_t alignmentBits) noexcept
{
    if ((alignmentBits != 32 && alignmentBits != 64) || sizeBits <= 128 || sizeBits % alignmentBits ||
        type.kind != TypeKind::Structure || type.packed ||
        type.widthOrCount || type.elements.size() != sizeBits / alignmentBits) return false;
    for (const auto& field : type.elements)
    {
        const bool integer = field.kind == TypeKind::Integer && field.widthOrCount == alignmentBits;
        const bool floating = ((alignmentBits == 32 && field.kind == TypeKind::Float) ||
                               (alignmentBits == 64 && field.kind == TypeKind::Double)) && field.widthOrCount == 0;
        if ((!integer && !floating) || field.packed || !field.elements.empty()) return false;
    }
    return true;
}
inline bool isSysVScalarIndirectRecordArgument(const Plan& plan, std::size_t index, std::uint64_t sizeBits, std::uint64_t alignmentBits) noexcept
{
    if (index >= plan.logicalParameters.size()) return false;
    const auto& logical = plan.logicalParameters[index];
    if (logical.passing != Passing::Indirect || logical.offset != 0 || !logical.byValue ||
        logical.physicalCount != 1 || logical.alignment != 8 ||
        logical.firstPhysical >= plan.physicalParameters.size()) return false;
    const auto& physical = plan.physicalParameters[logical.firstPhysical];
    return physical.byValue && physical.alignment == 8 && physical.pointee &&
        isSysVScalarRecordPointee(*physical.pointee, sizeBits, alignmentBits);
}
inline bool isSysVCppDirectIntegerRecordArgument(const Plan& plan, std::size_t index,
                                                 std::uint64_t sizeBits, std::uint64_t alignmentBits) noexcept
{
    if (index >= plan.logicalParameters.size() || alignmentBits != 32 ||
        (sizeBits != 32 && sizeBits != 64 && sizeBits != 96 && sizeBits != 128)) return false;
    const auto& logical = plan.logicalParameters[index];
    if (logical.passing != Passing::Direct || logical.offset != 0 || logical.byValue || !logical.coercion ||
        logical.firstPhysical >= plan.physicalParameters.size()) return false;
    const auto integer = [](const Type& type, std::uint64_t bits) {
        return type.kind == TypeKind::Integer && type.widthOrCount == bits;
    };
    if (sizeBits <= 64)
        return logical.physicalCount == 1 && integer(*logical.coercion, sizeBits) &&
            integer(plan.physicalParameters[logical.firstPhysical].type, sizeBits);
    if (logical.physicalCount != 2 ||
        !logical.coercion || logical.coercion->kind != TypeKind::Structure || logical.coercion->elements.size() != 2 ||
        logical.firstPhysical + 1 >= plan.physicalParameters.size()) return false;
    return integer(logical.coercion->elements[0], 64) && integer(logical.coercion->elements[1], sizeBits - 64) &&
        integer(plan.physicalParameters[logical.firstPhysical].type, 64) &&
        integer(plan.physicalParameters[logical.firstPhysical + 1].type, sizeBits - 64);
}
inline bool isSysVCppDirectFloatingRecordArgument(const Plan& plan, std::size_t index,
                                                  std::uint64_t sizeBits, std::uint64_t alignmentBits) noexcept
{
    if (index >= plan.logicalParameters.size()) return false;
    const auto& logical = plan.logicalParameters[index];
    if (logical.passing != Passing::Direct || logical.offset != 0 || logical.byValue ||
        !logical.coercion || logical.firstPhysical >= plan.physicalParameters.size()) return false;
    const auto& physical = plan.physicalParameters[logical.firstPhysical].type;
    if (logical.physicalCount == 2)
    {
        if (logical.firstPhysical + 1 >= plan.physicalParameters.size() ||
            !isSysVDirectPairLayout(*logical.coercion, sizeBits, alignmentBits) ||
            logical.coercion->elements.size() != 2) return false;
        const auto& second = plan.physicalParameters[logical.firstPhysical + 1].type;
        const auto& a = logical.coercion->elements[0];
        const auto& b = logical.coercion->elements[1];
        const bool vectors = a.kind == TypeKind::Vector &&
            (b.kind == TypeKind::Float || b.kind == TypeKind::Vector) && alignmentBits == 32 &&
            ((sizeBits == 96 && b.kind == TypeKind::Float) || (sizeBits == 128 && b.kind == TypeKind::Vector));
        const bool doubles = a.kind == TypeKind::Double && b.kind == TypeKind::Double &&
            sizeBits == 128 && alignmentBits == 64;
        const bool mixed = sizeBits == 128 && alignmentBits == 64 &&
            ((a.kind == TypeKind::Double && b.kind == TypeKind::Integer && b.widthOrCount == 32) ||
             (a.kind == TypeKind::Integer && a.widthOrCount == 32 && b.kind == TypeKind::Double));
        const bool mixed32 = alignmentBits == 32 &&
            (a.kind == TypeKind::Vector || (a.kind == TypeKind::Integer && a.widthOrCount == 64)) &&
            ((sizeBits == 96 && (b.kind == TypeKind::Float || (b.kind == TypeKind::Integer && b.widthOrCount == 32))) ||
             (sizeBits == 128 && (b.kind == TypeKind::Vector || (b.kind == TypeKind::Integer && b.widthOrCount == 64)))) &&
            (a.kind == TypeKind::Vector || b.kind == TypeKind::Vector || b.kind == TypeKind::Float);
        return (vectors || doubles || mixed || mixed32) && physical.kind == a.kind && physical.widthOrCount == a.widthOrCount &&
            second.kind == b.kind && second.widthOrCount == b.widthOrCount;
    }
    if (logical.physicalCount != 1) return false;
    return (physical.kind == TypeKind::Float || physical.kind == TypeKind::Double || physical.kind == TypeKind::Vector) &&
        isSysVDirectSingleRecordLayout(physical, sizeBits, alignmentBits) &&
        isSysVDirectSingleRecordLayout(*logical.coercion, sizeBits, alignmentBits) &&
        physical.kind == logical.coercion->kind && physical.widthOrCount == logical.coercion->widthOrCount;
}
inline bool isSysVScalarIndirectRecordRoundTrip(const Plan& plan, std::uint64_t sizeBits, std::uint64_t alignmentBits) noexcept
{
    const bool unary = isSysVIndirectRecordRoundTrip(plan, "__nsc_pointer_abi_probe");
    if ((alignmentBits != 32 && alignmentBits != 64) || sizeBits <= 128 || sizeBits % alignmentBits != 0 ||
        plan.logicalReturn.alignment != alignmentBits / 8 ||
        (!unary && !isSysVIndirectRecordWithArguments(plan, "__nsc_pointer_abi_probe")) ||
        plan.logicalParameters.empty()) return false;
    if (!isSysVScalarRecordPointee(*plan.physicalParameters[0].pointee, sizeBits, alignmentBits)) return false;
    std::size_t recordCount = 0;
    for (std::size_t i = 0; i < plan.logicalParameters.size(); ++i)
    {
        const auto& logical = plan.logicalParameters[i];
        if (logical.passing == Passing::Indirect)
        {
            if (logical.offset != 0 || logical.firstPhysical >= plan.physicalParameters.size() ||
                !plan.physicalParameters[logical.firstPhysical].pointee) return false;
            const auto& pointee = *plan.physicalParameters[logical.firstPhysical].pointee;
            if (pointee.elements.empty()) return false;
            const auto& first = pointee.elements[0];
            const auto fieldBits = first.kind == TypeKind::Float ? 32U :
                first.kind == TypeKind::Double ? 64U : first.widthOrCount;
            if ((fieldBits != 32 && fieldBits != 64) || pointee.elements.size() > UINT64_MAX / fieldBits ||
                !isSysVScalarIndirectRecordArgument(plan, i, pointee.elements.size() * fieldBits, fieldBits)) return false;
            ++recordCount;
            continue;
        }
        if ((logical.passing != Passing::Direct && logical.passing != Passing::Extend) ||
            logical.physicalCount != 1 || logical.firstPhysical >= plan.physicalParameters.size()) return false;
        const auto& physical = plan.physicalParameters[logical.firstPhysical].type;
        if (logical.passing == Passing::Extend)
        {
            if (physical.kind != TypeKind::Integer ||
                (physical.widthOrCount != 1 && physical.widthOrCount != 8 && physical.widthOrCount != 16)) return false;
            continue;
        }
        if (!((physical.kind == TypeKind::Integer && (physical.widthOrCount == 32 || physical.widthOrCount == 64)) ||
              ((physical.kind == TypeKind::Float || physical.kind == TypeKind::Double ||
                physical.kind == TypeKind::Pointer) && physical.widthOrCount == 0))) return false;
    }
    return recordCount != 0;
}
inline bool isSysVCppScalarPointer(const CppType& type) noexcept
{
    if (type.kind != CppTypeKind::Pointer) return false;
    const CppType* element = &type;
    unsigned depth = 0;
    while (element->kind == CppTypeKind::Pointer)
    {
        if (++depth > 16 || element->isConst || element->isVolatile || element->isRestrict || !element->element)
            return false;
        element = element->element.get();
    }
    if (element->isConst || element->isVolatile || element->isRestrict ||
        (element->kind != CppTypeKind::SignedInteger && element->kind != CppTypeKind::UnsignedInteger &&
         element->kind != CppTypeKind::Boolean && element->kind != CppTypeKind::Floating &&
         element->kind != CppTypeKind::Void)) return false;
    Type physical;
    physical.kind = TypeKind::Pointer;
    return regCallFloatRecordScalarMatches(physical, type);
}
inline bool isSysVCppRecordScalarArgument(const Plan& plan, std::size_t index, const CppType& type) noexcept
{
    if (index >= plan.logicalParameters.size() || type.isConst || type.isVolatile || type.isRestrict) return false;
    const auto& logical = plan.logicalParameters[index];
    if ((logical.passing != Passing::Direct && logical.passing != Passing::Extend) ||
        logical.physicalCount != 1 || logical.firstPhysical >= plan.physicalParameters.size()) return false;
    const auto& parameter = plan.physicalParameters[logical.firstPhysical];
    const auto& physical = parameter.type;
    const bool signedNarrow = type.kind == CppTypeKind::SignedInteger &&
        ((type.canonicalSpelling == "signed char" && type.sizeBits == 8 && type.alignmentBits == 8 && physical.widthOrCount == 8) ||
         (type.canonicalSpelling == "short" && type.sizeBits == 16 && type.alignmentBits == 16 && physical.widthOrCount == 16));
    const bool unsignedNarrow = type.kind == CppTypeKind::UnsignedInteger &&
        ((type.canonicalSpelling == "unsigned char" && type.sizeBits == 8 && type.alignmentBits == 8 && physical.widthOrCount == 8) ||
         (type.canonicalSpelling == "unsigned short" && type.sizeBits == 16 && type.alignmentBits == 16 && physical.widthOrCount == 16));
    const bool boolean = type.kind == CppTypeKind::Boolean && (type.canonicalSpelling == "bool" || type.canonicalSpelling == "_Bool") &&
        type.sizeBits == 8 && type.alignmentBits == 8 && physical.widthOrCount == 1;
    if (logical.passing == Passing::Extend)
        return physical.kind == TypeKind::Integer &&
            ((signedNarrow && logical.signExtend && !logical.zeroExtend && parameter.signExtend && !parameter.zeroExtend) ||
             ((unsignedNarrow || boolean) && logical.zeroExtend && !logical.signExtend && parameter.zeroExtend && !parameter.signExtend));
    const bool integer = ((type.kind == CppTypeKind::SignedInteger && type.canonicalSpelling == "int") ||
                          (type.kind == CppTypeKind::UnsignedInteger && type.canonicalSpelling == "unsigned int")) &&
        type.sizeBits == 32 && type.alignmentBits == 32 && physical.kind == TypeKind::Integer && physical.widthOrCount == 32;
    const bool integer64 = ((type.kind == CppTypeKind::SignedInteger && type.canonicalSpelling == "long long") ||
                            (type.kind == CppTypeKind::UnsignedInteger && type.canonicalSpelling == "unsigned long long")) &&
        type.sizeBits == 64 && type.alignmentBits == 64 && physical.kind == TypeKind::Integer && physical.widthOrCount == 64;
    const bool pointer = isSysVCppScalarPointer(type) &&
        physical.kind == TypeKind::Pointer && physical.widthOrCount == 0;
    const bool real = type.kind == CppTypeKind::Floating && type.canonicalSpelling == "float" &&
        type.sizeBits == 32 && type.alignmentBits == 32 && physical.kind == TypeKind::Float && physical.widthOrCount == 0;
    const bool wide = type.kind == CppTypeKind::Floating && type.canonicalSpelling == "double" &&
        type.sizeBits == 64 && type.alignmentBits == 64 && physical.kind == TypeKind::Double && physical.widthOrCount == 0;
    return integer || integer64 || pointer || real || wide;
}
// Physical and signature-level identity check; callers also reconcile every
// pointed-to declaration with the source session or precompiled catalog.
inline const CppType* sysVCppPointedRecord(const CppType& type) noexcept
{
    if (type.kind != CppTypeKind::Pointer) return nullptr;
    const CppType* leaf = &type;
    unsigned depth = 0;
    while (leaf->kind == CppTypeKind::Pointer)
    {
        if (++depth > 16 || !leaf->element) return nullptr;
        leaf = leaf->element.get();
    }
    return leaf->kind == CppTypeKind::Record ? leaf : nullptr;
}
inline bool isSysVCppKnownRecordPointer(const CppType& type, const CppFunctionType& signature) noexcept
{
    const auto* pointed = sysVCppPointedRecord(type);
    if (!pointed || !pointed->recordIdentity || !pointed->recordIdentity->clangUsr ||
        pointed->recordIdentity->clangUsr->empty()) return false;
    for (const CppType* level = &type; ; level = level->element.get())
    {
        if (level->isConst || level->isVolatile || level->isRestrict) return false;
        if (level->kind != CppTypeKind::Pointer) break;
    }
    Type physical; physical.kind = TypeKind::Pointer;
    if (!regCallFloatRecordScalarMatches(physical, type)) return false;
    const auto sameRecord = [&](const CppType& candidate) {
        return candidate.kind == CppTypeKind::Record && candidate.recordIdentity && candidate.recordIdentity->clangUsr &&
            candidate.recordIdentity->clangUsr == pointed->recordIdentity->clangUsr &&
            candidate.canonicalSpelling == pointed->canonicalSpelling && candidate.sizeBits && candidate.alignmentBits &&
            candidate.sizeBits == pointed->sizeBits && candidate.alignmentBits == pointed->alignmentBits;
    };
    const auto consistent = [&](const CppType& candidate) {
        if (candidate.kind != CppTypeKind::Record || !candidate.recordIdentity || !candidate.recordIdentity->clangUsr)
            return true;
        if (candidate.canonicalSpelling != pointed->canonicalSpelling &&
            candidate.recordIdentity->clangUsr != pointed->recordIdentity->clangUsr) return true;
        return sameRecord(candidate);
    };
    return consistent(signature.returnType) &&
        std::all_of(signature.parameterTypes.begin(), signature.parameterTypes.end(), consistent);
}
inline bool isSysVCppPointerToKnownRecord(const Plan& plan, std::size_t index,
                                          const CppType& type, const CppFunctionType& signature) noexcept
{
    if (index >= plan.logicalParameters.size() || !isSysVCppKnownRecordPointer(type, signature)) return false;
    const auto& logical = plan.logicalParameters[index];
    return logical.passing == Passing::Direct && logical.physicalCount == 1 &&
        logical.firstPhysical < plan.physicalParameters.size() &&
        regCallFloatRecordScalarMatches(plan.physicalParameters[logical.firstPhysical].type, type);
}
inline bool isSysVCppRecordPointerScalarReturn(const Plan& plan, const CppFunctionType& signature) noexcept
{
    const auto& result = signature.returnType;
    if (!signature.hasPrototype || signature.variadic || signature.callingConvention != "sysv_abi" ||
        result.isConst || result.isVolatile || result.isRestrict ||
        !isSysVScalarReturnWithRecordArguments(plan, "__nsc_pointer_abi_probe") ||
        plan.logicalParameters.size() != signature.parameterTypes.size()) return false;
    const auto& physicalReturn = plan.physicalReturn;
    const bool noResult = result.kind == CppTypeKind::Void && result.canonicalSpelling == "void" &&
        plan.logicalReturn.passing == Passing::Ignore && physicalReturn.kind == TypeKind::Void && physicalReturn.widthOrCount == 0;
    const bool pointerResult = (isSysVCppScalarPointer(result) || isSysVCppKnownRecordPointer(result, signature)) &&
        plan.logicalReturn.passing == Passing::Direct &&
        physicalReturn.kind == TypeKind::Pointer && physicalReturn.widthOrCount == 0;
    const bool integer32 = ((result.kind == CppTypeKind::SignedInteger && result.canonicalSpelling == "int") ||
        (result.kind == CppTypeKind::UnsignedInteger && result.canonicalSpelling == "unsigned int")) &&
        result.sizeBits == 32 && result.alignmentBits == 32 &&
        physicalReturn.kind == TypeKind::Integer && physicalReturn.widthOrCount == 32;
    const bool integer64 = ((result.kind == CppTypeKind::SignedInteger && result.canonicalSpelling == "long long") ||
        (result.kind == CppTypeKind::UnsignedInteger && result.canonicalSpelling == "unsigned long long")) &&
        result.sizeBits == 64 && result.alignmentBits == 64 &&
        physicalReturn.kind == TypeKind::Integer && physicalReturn.widthOrCount == 64;
    const bool real = result.kind == CppTypeKind::Floating && result.canonicalSpelling == "float" &&
        result.sizeBits == 32 && result.alignmentBits == 32 &&
        physicalReturn.kind == TypeKind::Float && physicalReturn.widthOrCount == 0;
    const bool wide = result.kind == CppTypeKind::Floating && result.canonicalSpelling == "double" &&
        result.sizeBits == 64 && result.alignmentBits == 64 &&
        physicalReturn.kind == TypeKind::Double && physicalReturn.widthOrCount == 0;
    const bool signedNarrow = result.kind == CppTypeKind::SignedInteger &&
        ((result.canonicalSpelling == "signed char" && result.sizeBits == 8 && result.alignmentBits == 8 && physicalReturn.widthOrCount == 8) ||
         (result.canonicalSpelling == "short" && result.sizeBits == 16 && result.alignmentBits == 16 && physicalReturn.widthOrCount == 16));
    const bool unsignedNarrow = result.kind == CppTypeKind::UnsignedInteger &&
        ((result.canonicalSpelling == "unsigned char" && result.sizeBits == 8 && result.alignmentBits == 8 && physicalReturn.widthOrCount == 8) ||
         (result.canonicalSpelling == "unsigned short" && result.sizeBits == 16 && result.alignmentBits == 16 && physicalReturn.widthOrCount == 16));
    const bool boolean = result.kind == CppTypeKind::Boolean &&
        (result.canonicalSpelling == "bool" || result.canonicalSpelling == "_Bool") &&
        result.sizeBits == 8 && result.alignmentBits == 8 && physicalReturn.widthOrCount == 1;
    const bool extended = physicalReturn.kind == TypeKind::Integer && plan.logicalReturn.passing == Passing::Extend &&
        ((signedNarrow && plan.returnSignExtend && !plan.returnZeroExtend) ||
         ((unsignedNarrow || boolean) && plan.returnZeroExtend && !plan.returnSignExtend));
    if (!integer32 && !integer64 && !real && !wide && !extended && !noResult && !pointerResult) return false;
    std::size_t records = 0;
    for (std::size_t i = 0; i < signature.parameterTypes.size(); ++i)
    {
        const auto& type = signature.parameterTypes[i];
        if (type.kind == CppTypeKind::Record)
        {
            if (type.isConst || type.isVolatile || type.isRestrict || !type.recordIdentity ||
                !type.recordIdentity->clangUsr || type.recordIdentity->clangUsr->empty() ||
                type.canonicalSpelling.empty() || !type.sizeBits || !type.alignmentBits ||
                !(isSysVScalarIndirectRecordArgument(plan, i, *type.sizeBits, *type.alignmentBits) ||
                  isSysVCppDirectIntegerRecordArgument(plan, i, *type.sizeBits, *type.alignmentBits) ||
                  isSysVCppDirectFloatingRecordArgument(plan, i, *type.sizeBits, *type.alignmentBits))) return false;
            ++records;
            continue;
        }
        if (!isSysVCppRecordScalarArgument(plan, i, type) &&
            !isSysVCppPointerToKnownRecord(plan, i, type, signature)) return false;
    }
    return records != 0;
}
inline bool isSysVCppFloatingPairRoundTrip(const Plan& plan, std::uint64_t sizeBits, std::uint64_t alignmentBits) noexcept
{
    const auto same = [](const auto& self, const Type& left, const Type& right) -> bool {
        if (left.kind != right.kind || left.widthOrCount != right.widthOrCount || left.packed != right.packed ||
            left.elements.size() != right.elements.size()) return false;
        for (std::size_t i = 0; i < left.elements.size(); ++i)
            if (!self(self, left.elements[i], right.elements[i])) return false;
        return true;
    };
    if (plan.symbol != "__nsc_pointer_abi_probe" || plan.callingConvention != 78 || plan.variadic ||
        !plan.targetTriple.starts_with("x86_64-pc-windows-msvc") || !plan.dataLayout.starts_with("e-") ||
        plan.returnSignExtend || plan.returnZeroExtend || !isSysVDirectPairLayout(plan.physicalReturn, sizeBits, alignmentBits) ||
        plan.logicalReturn.passing != Passing::Direct || plan.logicalReturn.offset != 0 ||
        !plan.logicalReturn.coercion || !same(same, *plan.logicalReturn.coercion, plan.physicalReturn)) return false;
    std::size_t next = 0, records = 0;
    for (std::size_t i = 0; i < plan.logicalParameters.size(); ++i)
    {
        const auto& logical = plan.logicalParameters[i];
        if (logical.offset != 0 || logical.firstPhysical != next || logical.physicalCount == 0 ||
            next > plan.physicalParameters.size() || logical.physicalCount > plan.physicalParameters.size() - next) return false;
        if (logical.physicalCount == 2)
        {
            if (!isSysVCppDirectFloatingRecordArgument(plan, i, sizeBits, alignmentBits) ||
                !same(same, *logical.coercion, plan.physicalReturn) ||
                !same(same, plan.physicalParameters[next].type, plan.physicalReturn.elements[0]) ||
                !same(same, plan.physicalParameters[next + 1].type, plan.physicalReturn.elements[1])) return false;
            ++records;
        }
        else if (logical.physicalCount != 1 ||
                 (logical.passing != Passing::Direct && logical.passing != Passing::Extend)) return false;
        for (std::size_t n = 0; n < logical.physicalCount; ++n)
        {
            const auto& physical = plan.physicalParameters[next + n];
            if (physical.structureReturn || physical.byValue || physical.inRegister || physical.noAlias ||
                (logical.physicalCount == 2 && (physical.signExtend || physical.zeroExtend))) return false;
        }
        next += logical.physicalCount;
    }
    return records == 1 && next == plan.physicalParameters.size();
}
inline bool isSysVCppRecordPointerRoundTrip(const Plan& plan, const CppFunctionType& signature) noexcept
{
    const auto& result = signature.returnType;
    if (!signature.hasPrototype || signature.variadic || signature.callingConvention != "sysv_abi" ||
        signature.parameterTypes.empty() || result.kind != CppTypeKind::Record ||
        result.isConst || result.isVolatile || result.isRestrict || !result.recordIdentity ||
        !result.recordIdentity->clangUsr || result.recordIdentity->clangUsr->empty() ||
        result.canonicalSpelling.empty() || !result.sizeBits || !result.alignmentBits ||
        !((*result.alignmentBits == 32 && (*result.sizeBits == 64 || *result.sizeBits == 96 ||
             *result.sizeBits == 128 || (*result.sizeBits >= 160 && *result.sizeBits % 32 == 0))) ||
          (*result.alignmentBits == 64 && *result.sizeBits >= 128 && *result.sizeBits % 64 == 0))) return false;
    if (plan.logicalParameters.size() != signature.parameterTypes.size()) return false;
    std::size_t recordCount = 0;
    for (std::size_t i = 0; i < signature.parameterTypes.size(); ++i)
    {
        const auto& type = signature.parameterTypes[i];
        if (type.kind == CppTypeKind::Record)
        {
            if (type.isConst || type.isVolatile || type.isRestrict || !type.recordIdentity ||
                !type.recordIdentity->clangUsr || type.recordIdentity->clangUsr->empty() ||
                type.canonicalSpelling.empty() || !type.sizeBits || !type.alignmentBits ||
                (*type.alignmentBits != 32 && *type.alignmentBits != 64) ||
                ((type.canonicalSpelling == result.canonicalSpelling) !=
                 (type.recordIdentity->clangUsr == result.recordIdentity->clangUsr)) ||
                (type.recordIdentity->clangUsr == result.recordIdentity->clangUsr &&
                 (type.sizeBits != result.sizeBits || type.alignmentBits != result.alignmentBits)) ||
                (*result.sizeBits >= 160 && !isSysVScalarIndirectRecordArgument(plan, i, *type.sizeBits, *type.alignmentBits)) ||
                (*result.sizeBits < 160 && (type.recordIdentity->clangUsr != result.recordIdentity->clangUsr ||
                 type.canonicalSpelling != result.canonicalSpelling || type.sizeBits != result.sizeBits))) return false;
            ++recordCount;
            continue;
        }
        if (!isSysVCppRecordScalarArgument(plan, i, type) &&
            !isSysVCppPointerToKnownRecord(plan, i, type, signature)) return false;
    }
    if (recordCount == 0) return false;
    return ((signature.parameterTypes.size() == 1 && result.sizeBits == 64 && plan.physicalReturn.widthOrCount == 64 &&
          isSysVSmallRecordRoundTrip(plan, "__nsc_pointer_abi_probe")) ||
         (signature.parameterTypes.size() == 1 && (result.sizeBits == 96 || result.sizeBits == 128) &&
          isSysVTwoIntegerRecordRoundTrip(plan, "__nsc_pointer_abi_probe") &&
          plan.physicalReturn.elements[1].widthOrCount == (result.sizeBits == 96 ? 32 : 64)) ||
         isSysVCppFloatingPairRoundTrip(plan, *result.sizeBits, *result.alignmentBits) ||
         isSysVScalarIndirectRecordRoundTrip(plan, *result.sizeBits, *result.alignmentBits));
}
}
