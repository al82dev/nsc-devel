#pragma once

#include "nativesharp/Interop/CppAbiPlanQueries.h"
#include "nativesharp/Interop/CppRecordDeclaration.h"

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
        if (logical.passing != Passing::Direct || logical.physicalCount != 1 ||
            logical.firstPhysical >= plan.physicalParameters.size()) return false;
        const auto& physical = plan.physicalParameters[logical.firstPhysical].type;
        if (!((physical.kind == TypeKind::Integer && physical.widthOrCount == 32) ||
              ((physical.kind == TypeKind::Float || physical.kind == TypeKind::Double) &&
               physical.widthOrCount == 0))) return false;
    }
    return recordCount != 0;
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
          (*result.alignmentBits == 64 && *result.sizeBits >= 192 && *result.sizeBits % 64 == 0))) return false;
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
        const auto& logical = plan.logicalParameters[i];
        if (logical.passing != Passing::Direct || logical.physicalCount != 1 ||
            logical.firstPhysical >= plan.physicalParameters.size() ||
            type.isConst || type.isVolatile || type.isRestrict) return false;
        const auto& physical = plan.physicalParameters[logical.firstPhysical].type;
        const bool integer = type.kind == CppTypeKind::SignedInteger && type.canonicalSpelling == "int" &&
            type.sizeBits == 32 && type.alignmentBits == 32 && physical.kind == TypeKind::Integer && physical.widthOrCount == 32;
        const bool real = type.kind == CppTypeKind::Floating && type.canonicalSpelling == "float" &&
            type.sizeBits == 32 && type.alignmentBits == 32 && physical.kind == TypeKind::Float && physical.widthOrCount == 0;
        const bool wide = type.kind == CppTypeKind::Floating && type.canonicalSpelling == "double" &&
            type.sizeBits == 64 && type.alignmentBits == 64 && physical.kind == TypeKind::Double && physical.widthOrCount == 0;
        if (!integer && !real && !wide) return false;
    }
    if (recordCount == 0) return false;
    return ((signature.parameterTypes.size() == 1 && result.sizeBits == 64 && plan.physicalReturn.widthOrCount == 64 &&
          isSysVSmallRecordRoundTrip(plan, "__nsc_pointer_abi_probe")) ||
         (signature.parameterTypes.size() == 1 && (result.sizeBits == 96 || result.sizeBits == 128) &&
          isSysVTwoIntegerRecordRoundTrip(plan, "__nsc_pointer_abi_probe") &&
          plan.physicalReturn.elements[1].widthOrCount == (result.sizeBits == 96 ? 32 : 64)) ||
         isSysVScalarIndirectRecordRoundTrip(plan, *result.sizeBits, *result.alignmentBits));
}
}
