#include "nativesharp/Interop/CppSemanticRecord.h"
#include "nativesharp/Interop/CppCRecordAbiReplay.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Interop/CppSemanticFunction.h"
#include "nativesharp/Interop/CppSemanticEnumType.h"
#include "nativesharp/Frontend/CppAliasArrayType.h"
#include "nativesharp/Interop/CppSemanticFunctionGroups.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/QualifiedNameLadder.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Interop/CppNamespaceInventory.h"
#include "nativesharp/Interop/CppSemanticInventory.h"
#include "nativesharp/Frontend/ProjectNamespaceFunctionModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/TypeNameBinding.h"
#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "../Internal/CppCRecordMetadataCodec.h"
#include "../Internal/LibraryMetadataCodecSupport.h"
#include "CppPublicHeaderTypes.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <numeric>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace nativesharp::interop
{
namespace
{
std::string utf8SourcePath(const std::filesystem::path& path)
{
    const auto encoded = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}
std::string cEnumeratorSourceDigest(const std::optional<CppSourcePoint>& location)
{
    if (!location || location->file.empty()) return {};
    basic::Sha256Digest digest{};
    std::uintmax_t size = 0;
    std::string error;
    if (!basic::sha256File(location->file, digest, size, error)) return {};
    return basic::sha256Hex(digest);
}
// EN: T08-D26. The installed C record that a field of a record of `projectOrdinal` names: its own record or, when
//     that one is reconciled with an equivalent declaration of another producer, the canonical one.
// ES: T08-D26. El record C instalado que nombra un campo de un record de `projectOrdinal`: su propio record o,
//     si este esta conciliado con una declaracion equivalente de otro productor, el canonico.
const frontend::CppImportedTypeDeclaration* fieldRecordType(const frontend::ProjectNameLookup& lookup,
    const std::optional<compilation::TypeIdentity>& target, std::size_t projectOrdinal)
{
    const auto ordinal = target ? lookup.typeOrdinalForIdentity(target->key()) : std::nullopt;
    const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
    if (!imported) return nullptr;
    return imported->projectOrdinal == projectOrdinal || lookup.isReconciledCppTypeIdentity(target->key()) ?
        imported : nullptr;
}
std::optional<CppScalarTypeMapping> projectCArrayField(const CppType& type, std::size_t projectOrdinal,
    std::string_view moduleName, const frontend::ProjectNameLookup* lookup)
{
    const CppType* scalar = &type;
    std::vector<std::uint64_t> scalarDimensions;
    bool readonly = false;
    bool isVolatile = false;
    while (scalar && scalar->kind == CppTypeKind::Array && scalarDimensions.size() < 64)
    {
        if (!scalar->arrayCount || *scalar->arrayCount == 0 || !scalar->element) return std::nullopt;
        readonly |= scalar->isConst;
        isVolatile |= scalar->isVolatile;
        scalarDimensions.push_back(*scalar->arrayCount);
        scalar = scalar->element.get();
    }
    if (!scalarDimensions.empty() && scalar && scalar->kind != CppTypeKind::Array &&
        (scalar->kind == CppTypeKind::Boolean || scalar->kind == CppTypeKind::SignedInteger ||
         scalar->kind == CppTypeKind::UnsignedInteger || scalar->kind == CppTypeKind::Floating))
    {
        readonly |= scalar->isConst;
        isVolatile |= scalar->isVolatile;
        CppType unqualified = *scalar;
        unqualified.isConst = false;
        unqualified.isVolatile = false;
        auto mapped = mapCppScalarType(unqualified);
        if (!mapped) return std::nullopt;
        for (auto dimension = scalarDimensions.rbegin(); dimension != scalarDimensions.rend(); ++dimension)
            mapped->nativeSharpTypeKey += "[" + std::to_string(*dimension) + "]";
        mapped->canonicalCppType = type.canonicalSpelling;
        mapped->isConst = readonly;
        mapped->isVolatile = isVolatile;
        return mapped;
    }
    const auto array = frontend::cppAliasArrayType(type);
    std::string key;
    std::vector<std::uint64_t> dimensions;
    bool aggregateReadonly = false;
    bool aggregateVolatile = false;
    if (array)
    {
        if (array->element.token == lexer::TokenKind::KwVoid || array->element.pointeeConst) return std::nullopt;
        key = "builtin:" + std::string(array->element.name) + std::string(array->element.pointerDepth, '*');
        dimensions = array->dimensions;
    }
    else
    {
        const CppType* element = &type;
        while (element && element->kind == CppTypeKind::Array && dimensions.size() < 64)
        {
            if (!element->arrayCount || *element->arrayCount == 0)
                return std::nullopt;
            aggregateReadonly |= element->isConst;
            aggregateVolatile |= element->isVolatile;
            dimensions.push_back(*element->arrayCount);
            element = element->element.get();
        }
        if (dimensions.empty() || !element) return std::nullopt;
        aggregateReadonly |= element->isConst;
        aggregateVolatile |= element->isVolatile;
        CppType unqualified = *element;
        unqualified.isConst = false;
        unqualified.isVolatile = false;
        const auto mapped = projectCppSemanticEnumType(unqualified, projectOrdinal, moduleName, lookup);
        if (mapped) key = mapped->nativeSharpTypeKey;
        else if (lookup && element->kind == CppTypeKind::Record)
        {
            const auto target = projectCppRecordTypeIdentity(*element, moduleName);
            const auto* imported = fieldRecordType(*lookup, target, projectOrdinal);
            if (!target || !imported || !imported->objectLayout ||
                (element->recordIdentity && element->recordIdentity->clangUsr && imported->clangUsr &&
                 element->recordIdentity->clangUsr != imported->clangUsr) ||
                imported->isIncomplete || !element->sizeBits || *element->sizeBits % 8 != 0 ||
                *element->sizeBits / 8 != imported->objectLayout->sizeBytes || !element->alignmentBits ||
                *element->alignmentBits % 8 != 0 ||
                *element->alignmentBits / 8 != imported->objectLayout->alignmentBytes) return std::nullopt;
            key = "type:" + imported->identity.key();
        }
        else return std::nullopt;
    }
    for (auto dimension = dimensions.rbegin(); dimension != dimensions.rend(); ++dimension)
        key += "[" + std::to_string(*dimension) + "]";
    return CppScalarTypeMapping{std::move(key), type.canonicalSpelling, aggregateReadonly, aggregateVolatile};
}

// Project the final C flexible array member as NativeSharp's zero-storage T[*].
std::optional<CppSemanticInstanceField> projectCFlexibleArrayField(const CppRecordDeclaration& record,
    std::size_t fieldIndex, const std::string& ownerIdentityKey, std::size_t projectOrdinal,
    std::string_view moduleName, const frontend::ProjectNameLookup* lookup)
{
    const auto& field = record.fields[fieldIndex];
    const CppType* element = field.type.element.get();
    if (record.isCxx || record.kind != CppRecordKind::Struct || fieldIndex + 1 != record.fields.size() ||
        field.name.empty() || field.access != CppAccess::Public || field.bitWidth || !field.offsetBits ||
        *field.offsetBits % 8 != 0 || *field.offsetBits > *record.sizeBits ||
        !element || element->kind == CppTypeKind::Array ||
        !element->sizeBits || !element->alignmentBits || *element->sizeBits == 0 ||
        *element->sizeBits % 8 != 0 || *element->alignmentBits == 0 || *element->alignmentBits % 8 != 0)
        return std::nullopt;
    const auto naturalAlignment = *element->alignmentBits / 8;
    const auto recordAlignment = *record.alignmentBits / 8;
    if ((naturalAlignment & (naturalAlignment - 1)) != 0 || recordAlignment == 0 ||
        (recordAlignment & (recordAlignment - 1)) != 0) return std::nullopt;
    // EN: T08-D15: Clang's effective alignment of the member, guaranteed by the record's own alignment.
    // ES: T08-D15: alineacion efectiva de Clang para el miembro, garantizada por la alineacion del propio record.
    const auto effectiveAlignment = std::gcd(recordAlignment, *field.offsetBits / 8);
    if (effectiveAlignment == 0) return std::nullopt;
    const bool isConst = field.type.isConst || element->isConst;
    const bool isVolatile = field.type.isVolatile || element->isVolatile;
    CppType unqualified = *element;
    unqualified.isConst = false;
    unqualified.isVolatile = false;
    std::optional<CppScalarTypeMapping> mapped;
    if (element->kind == CppTypeKind::Boolean || element->kind == CppTypeKind::SignedInteger ||
        element->kind == CppTypeKind::UnsignedInteger || element->kind == CppTypeKind::Floating)
        mapped = mapCppScalarType(unqualified);
    else if (element->kind == CppTypeKind::Enumeration)
        mapped = projectCppSemanticEnumType(unqualified, projectOrdinal, moduleName, lookup);
    else if (element->kind == CppTypeKind::Record && lookup)
    {
        const auto target = projectCppRecordTypeIdentity(*element, moduleName);
        const auto* imported = fieldRecordType(*lookup, target, projectOrdinal);
        if (target && imported && imported->objectLayout && !imported->isIncomplete &&
            (!element->recordIdentity || !element->recordIdentity->clangUsr || !imported->clangUsr ||
             element->recordIdentity->clangUsr == imported->clangUsr) &&
            *element->sizeBits / 8 == imported->objectLayout->sizeBytes &&
            naturalAlignment == imported->objectLayout->alignmentBytes)
            mapped = CppScalarTypeMapping{"type:" + imported->identity.key(), element->canonicalSpelling, false,
                                          false};
    }
    if (!mapped) return std::nullopt;
    return CppSemanticInstanceField{projectOrdinal, ownerIdentityKey, field.name,
        CppScalarTypeMapping{mapped->nativeSharpTypeKey + "[*]", field.type.canonicalSpelling, isConst, isVolatile},
        *field.offsetBits / 8, false, 0, effectiveAlignment, field.clangUsr};
}
}

std::optional<CppSemanticInstanceField> projectCppSemanticInstanceField(
    const CppRecordDeclaration& record, std::size_t fieldIndex, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, const frontend::ProjectNameLookup* lookup)
{
    if (targetTriple != "x86_64-pc-windows-msvc" || !record.complete || !record.sizeBits || !record.alignmentBits ||
        (record.kind == CppRecordKind::Union && record.isCxx) || fieldIndex >= record.fields.size()) return std::nullopt;
    const auto identity = projectCppRecordIdentity(record, moduleName);
    const auto& field = record.fields[fieldIndex];
    if (identity && field.type.kind == CppTypeKind::Array && field.type.isIncompleteArray)
        return projectCFlexibleArrayField(record, fieldIndex, identity->key(), projectOrdinal, moduleName, lookup);
    if (field.bitWidth)
    {
        // Clang supplies absolute coordinates; union members intentionally overlap.
        // Struct windows must not overlap independent fields or bit-field runs.
        if (!identity || record.isCxx ||
            (record.kind != CppRecordKind::Struct && record.kind != CppRecordKind::Union) ||
            field.name.empty() || field.access != CppAccess::Public ||
            !field.offsetBits || !*field.bitWidth || !field.type.sizeBits || !field.type.alignmentBits ||
            !*field.type.alignmentBits || *field.type.alignmentBits % 8 ||
            *field.type.sizeBits > 64 || *field.bitWidth > *field.type.sizeBits ||
            *field.offsetBits > *record.sizeBits || *field.bitWidth > *record.sizeBits - *field.offsetBits ||
            *record.sizeBits % 8 || *record.alignmentBits % 8 || !*record.alignmentBits ||
            (field.type.kind != CppTypeKind::SignedInteger &&
             field.type.kind != CppTypeKind::UnsignedInteger && field.type.kind != CppTypeKind::Boolean))
            return std::nullopt;
        std::uint64_t byteOffset = *field.offsetBits / 8;
        std::uint64_t bitOffset = *field.offsetBits % 8;
        std::uint64_t neededBits = bitOffset + *field.bitWidth;
        std::uint64_t bytes = 1;
        if (field.type.isVolatile)
        {
            // MS CodeGen uses discrete storage units beginning at the first
            // field in each run, not the minimum window around the member.
            std::uint64_t start = 0, tail = 0;
            bool active = false;
            if (record.kind == CppRecordKind::Union)
            {
                if (*field.offsetBits != 0) return std::nullopt;
                tail = *field.type.sizeBits;
                active = true;
            }
            else for (std::size_t i = 0; i <= fieldIndex; ++i)
            {
                const auto& candidate = record.fields[i];
                if (!candidate.bitWidth || !*candidate.bitWidth) { active = false; continue; }
                if (!candidate.offsetBits || !candidate.type.sizeBits || !*candidate.type.sizeBits ||
                    *candidate.type.sizeBits > 64 || *candidate.type.sizeBits % 8 ||
                    *candidate.offsetBits > *record.sizeBits) return std::nullopt;
                if (!active || *candidate.offsetBits >= tail)
                {
                    start = *candidate.offsetBits;
                    if (*candidate.type.sizeBits > *record.sizeBits - start) return std::nullopt;
                    tail = start + *candidate.type.sizeBits;
                    active = true;
                }
                if (*candidate.offsetBits < start || *candidate.offsetBits > tail ||
                    *candidate.bitWidth > tail - *candidate.offsetBits) return std::nullopt;
            }
            if (!active || start % 8 || tail <= start || (tail - start) % 8 ||
                *field.offsetBits < start || *field.offsetBits > tail ||
                *field.bitWidth > tail - *field.offsetBits) return std::nullopt;
            byteOffset = start / 8;
            bitOffset = *field.offsetBits - start;
            bytes = (tail - start) / 8;
            if (bytes > 8 || (bytes & (bytes - 1)) != 0) return std::nullopt;
            neededBits = bitOffset + *field.bitWidth;
        }
        else
        {
            while (bytes < 8 && bytes * 8 < neededBits) bytes *= 2;
            // EN: The smallest window starting at the field's first byte can cross the end of the record (an
            //     `unsigned hi : 24` after one byte of bit fields in a 32-bit unit). Clang reads such a field
            //     through its allocation unit, so fall back to the smallest naturally aligned window that holds it.
            // ES: La ventana minima que empieza en el primer byte del campo puede cruzar el final del record (un
            //     `unsigned hi : 24` tras un byte de campos de bits en una unidad de 32 bits). Clang lee ese campo
            //     por su unidad de asignacion; se usa la ventana alineada naturalmente mas pequena que lo contiene.
            const auto availableBytes = *record.sizeBits / 8;
            if (bytes * 8 < neededBits || byteOffset > availableBytes || bytes > availableBytes - byteOffset)
                for (std::uint64_t width = 1; width <= 8; width *= 2)
                {
                    const std::uint64_t start = *field.offsetBits / (width * 8) * (width * 8);
                    if (*field.offsetBits + *field.bitWidth > start + width * 8 || start / 8 > availableBytes ||
                        width > availableBytes - start / 8) continue;
                    byteOffset = start / 8;
                    bitOffset = *field.offsetBits - start;
                    bytes = width;
                    neededBits = bitOffset + *field.bitWidth;
                    break;
                }
        }
        const auto recordBytes = *record.sizeBits / 8;
        if (bytes * 8 < neededBits || byteOffset > recordBytes || bytes > recordBytes - byteOffset)
            return std::nullopt;
        std::size_t runBegin = fieldIndex;
        std::size_t runEnd = fieldIndex + 1;
        while (runBegin > 0 && record.fields[runBegin - 1].bitWidth &&
               *record.fields[runBegin - 1].bitWidth != 0) --runBegin;
        while (runEnd < record.fields.size() && record.fields[runEnd].bitWidth &&
               *record.fields[runEnd].bitWidth != 0) ++runEnd;
        for (std::size_t i = 0; record.kind == CppRecordKind::Struct && i < record.fields.size(); ++i)
        {
            const auto& other = record.fields[i];
            if (other.bitWidth)
            {
                if (!*other.bitWidth || (i >= runBegin && i < runEnd)) continue;
                if (!other.offsetBits || *other.offsetBits > *record.sizeBits ||
                    *other.bitWidth > *record.sizeBits - *other.offsetBits) return std::nullopt;
                if (*other.offsetBits < (byteOffset + bytes) * 8 &&
                    *other.offsetBits + *other.bitWidth > byteOffset * 8) return std::nullopt;
                continue;
            }
            if (!other.offsetBits || *other.offsetBits % 8) return std::nullopt;
            const auto otherStart = *other.offsetBits / 8;
            if (otherStart >= byteOffset + bytes) continue;
            if (!other.type.sizeBits || *other.type.sizeBits % 8) return std::nullopt;
            if (otherStart > recordBytes || *other.type.sizeBits / 8 > recordBytes - otherStart)
                return std::nullopt;
            if (otherStart + *other.type.sizeBits / 8 > byteOffset) return std::nullopt;
        }
        const auto mapped = mapCppScalarType(field.type);
        if (!mapped) return std::nullopt;
        const auto alignment = std::min({bytes, *field.type.alignmentBits / 8,
            std::gcd(*record.alignmentBits / 8, byteOffset)});
        if (!alignment || (alignment & (alignment - 1)) != 0) return std::nullopt;
        return CppSemanticInstanceField{projectOrdinal, identity->key(), field.name, *mapped,
            byteOffset, false, bytes, alignment, field.clangUsr, bitOffset, *field.bitWidth};
    }
    if (!identity || field.name.empty() || field.access != CppAccess::Public || field.bitWidth ||
        !field.offsetBits || *field.offsetBits % 8 != 0 || !field.type.sizeBits || !field.type.alignmentBits ||
        *field.type.alignmentBits == 0 || *field.type.alignmentBits % 8 != 0 ||
        *record.alignmentBits == 0 || *record.alignmentBits % 8 != 0 || *field.type.sizeBits % 8 != 0 ||
        *field.type.sizeBits == 0 || *field.offsetBits > *record.sizeBits ||
        *field.type.sizeBits > *record.sizeBits - *field.offsetBits) return std::nullopt;
    const auto naturalAlignment = *field.type.alignmentBits / 8;
    const auto recordAlignment = *record.alignmentBits / 8;
    if ((naturalAlignment & (naturalAlignment - 1)) != 0 ||
        (recordAlignment & (recordAlignment - 1)) != 0) return std::nullopt;
    // EN: T08-D15: Clang's effective alignment for a member access, `alignmentAtOffset(record alignment, offset)`.
    //     The record's alignment is guaranteed for every object of the record: C storage and pointers by the C
    //     rules, and NativeSharp locals, globals and allocations by the record's object layout. Nested members and
    //     array elements combine it with their parent (fieldOrArrayAddressAlignment), so it is never assumed
    //     beyond what the containing object guarantees. It is computed from the record alignment and the member
    //     offset already carried by NSMETA, so the metadata does not change.
    // ES: T08-D15: alineacion efectiva de Clang para un acceso a miembro, `alignmentAtOffset(alineacion del record,
    //     offset)`. La alineacion del record esta garantizada para todo objeto del record: el almacenamiento y los
    //     punteros C por las reglas de C, y los locales, globales y reservas de NativeSharp por el layout de objeto
    //     del record. Los miembros anidados y los elementos de arreglo la combinan con su padre
    //     (fieldOrArrayAddressAlignment), asi que nunca se supone mas de lo que garantiza el objeto contenedor. Se
    //     calcula con la alineacion del record y el offset del miembro que NSMETA ya lleva; la metadata no cambia.
    const auto effectiveAlignment = std::gcd(recordAlignment, *field.offsetBits / 8);
    const CppType* arrayElement = &field.type;
    bool scalarArray = field.type.kind == CppTypeKind::Array;
    while (scalarArray && arrayElement && arrayElement->kind == CppTypeKind::Array)
    {
        if (!arrayElement->arrayCount || *arrayElement->arrayCount == 0 || !arrayElement->element) scalarArray = false;
        else arrayElement = arrayElement->element.get();
    }
    scalarArray = scalarArray && arrayElement &&
        (arrayElement->kind == CppTypeKind::Boolean || arrayElement->kind == CppTypeKind::SignedInteger ||
         arrayElement->kind == CppTypeKind::UnsignedInteger || arrayElement->kind == CppTypeKind::Floating);
    const bool recordArray = field.type.kind == CppTypeKind::Array && arrayElement &&
        arrayElement->kind == CppTypeKind::Record;
    const bool enumArray = field.type.kind == CppTypeKind::Array && arrayElement &&
        arrayElement->kind == CppTypeKind::Enumeration;
    const auto ownerOrdinal = lookup ? lookup->typeOrdinalForIdentity(identity->key()) : std::nullopt;
    const bool cppValueFields = record.isCxx && ownerOrdinal && lookup->cppTrivialValueEligible(*ownerOrdinal);
    const auto pointerArray = frontend::cppAliasArrayType(field.type);
    const bool representablePointerArray = pointerArray && pointerArray->element.pointerDepth > 0 &&
        !pointerArray->element.pointeeConst && pointerArray->element.token != lexer::TokenKind::KwVoid;
    // EN: T08-D23: an array of pointers whose element or pointee is qualified (projected below).
    // ES: T08-D23: un arreglo de punteros con el elemento o el dato apuntado calificado (se proyecta abajo).
    const bool qualifiedPointerArray = field.type.kind == CppTypeKind::Array && arrayElement &&
        arrayElement->kind == CppTypeKind::Pointer && !representablePointerArray;
    if (effectiveAlignment < naturalAlignment &&
        (record.isCxx || (field.type.kind != CppTypeKind::Boolean &&
         field.type.kind != CppTypeKind::SignedInteger && field.type.kind != CppTypeKind::UnsignedInteger &&
         field.type.kind != CppTypeKind::Floating && field.type.kind != CppTypeKind::Pointer &&
         field.type.kind != CppTypeKind::Record && field.type.kind != CppTypeKind::Enumeration &&
         !scalarArray && !recordArray && !enumArray && !representablePointerArray &&
         !qualifiedPointerArray))) return std::nullopt;
    auto mapped = mapCppScalarType(field.type);
    if (!mapped && !record.isCxx)
        mapped = projectCppStoredFunctionPointer(field.type, projectOrdinal, moduleName, lookup);
    if (!mapped && (!record.isCxx || (cppValueFields && scalarArray)) && field.type.kind == CppTypeKind::Array)
        mapped = projectCArrayField(field.type, projectOrdinal, moduleName, lookup);
    if (!mapped && (!record.isCxx || (cppValueFields && !field.type.isVolatile)) &&
        field.type.kind == CppTypeKind::Enumeration)
    {
        CppType unqualified = field.type;
        unqualified.isVolatile = false;
        mapped = projectCppSemanticEnumType(unqualified, projectOrdinal, moduleName, lookup);
        if (mapped)
        {
            mapped->canonicalCppType = field.type.canonicalSpelling;
            mapped->isVolatile = field.type.isVolatile;
        }
    }
    if (!mapped && !record.isCxx && field.type.kind == CppTypeKind::Enumeration &&
        field.type.enumIdentity && field.type.enumIdentity->anonymousContext &&
        field.type.enumIdentity->name.empty() && field.type.enumUnderlying &&
        field.type.sizeBits && field.type.alignmentBits &&
        field.type.sizeBits == field.type.enumUnderlying->sizeBits &&
        field.type.alignmentBits == field.type.enumUnderlying->alignmentBits)
    {
        CppType underlying = *field.type.enumUnderlying;
        underlying.isConst = field.type.isConst;
        underlying.isVolatile = field.type.isVolatile;
        mapped = mapCppScalarType(underlying);
        if (mapped) mapped->canonicalCppType = field.type.canonicalSpelling;
    }
    if (!mapped && (!record.isCxx || cppValueFields) && lookup && field.type.kind == CppTypeKind::Record)
    {
        const auto target = projectCppRecordTypeIdentity(field.type, moduleName);
        const auto* imported = fieldRecordType(*lookup, target, projectOrdinal);
        const auto targetOrdinal = target ? lookup->typeOrdinalForIdentity(target->key()) : std::nullopt;
        const auto* layout = imported && imported->objectLayout ? &*imported->objectLayout :
            imported && field.isAnonymousCarrier && imported->memberAccessLayout ? &*imported->memberAccessLayout : nullptr;
        if (target && imported && layout && (!record.isCxx ||
            (targetOrdinal && lookup->cppTrivialValueEligible(*targetOrdinal))) &&
            (!field.type.recordIdentity || !field.type.recordIdentity->clangUsr || !imported->clangUsr ||
             field.type.recordIdentity->clangUsr == imported->clangUsr) &&
            !imported->isIncomplete && field.type.sizeBits && *field.type.sizeBits % 8 == 0 &&
            *field.type.sizeBits / 8 == layout->sizeBytes && field.type.alignmentBits &&
            *field.type.alignmentBits % 8 == 0 &&
            *field.type.alignmentBits / 8 == layout->alignmentBytes)
            mapped = CppScalarTypeMapping{"type:" + imported->identity.key(), field.type.canonicalSpelling,
                                          field.type.isConst, field.type.isVolatile};
    }
    bool pointeeReadonly = false;
    bool pointeeVolatile = false;
    // A C pointee may be `volatile`: the field keeps that qualification (C++ fields keep the previous rule).
    // Un apuntado C puede ser `volatile`: el campo conserva esa calificación (los campos C++ siguen la regla previa).
    if (field.type.kind == CppTypeKind::Pointer && field.type.sizeBits == 64 &&
        field.type.element && (!field.type.element->isVolatile || !record.isCxx))
    {
        auto pointee = *field.type.element;
        pointee.isVolatile = false;
        auto element = mapCppScalarType(pointee);
        if (!element && !record.isCxx)
            element = projectCppSemanticEnumType(*field.type.element, projectOrdinal, moduleName, lookup);
        // EN: `Record **` (and deeper) with unqualified intermediate pointers keeps the record pointee; each
        //     intermediate level only adds `*` to the type key.
        // ES: `Record **` (y mas niveles) con punteros intermedios sin calificar conserva el record apuntado;
        //     cada nivel intermedio solo agrega `*` a la clave del tipo.
        const CppType* recordPointee = field.type.element.get();
        std::string extraLevels;
        while (!record.isCxx && recordPointee->kind == CppTypeKind::Pointer && recordPointee->element &&
               !recordPointee->isConst && !recordPointee->isVolatile && extraLevels.size() < 8)
        {
            recordPointee = recordPointee->element.get();
            extraLevels += '*';
        }
        if (!extraLevels.empty() && (recordPointee->isConst || recordPointee->isVolatile)) recordPointee = nullptr;
        // EN: `T **` over an unqualified scalar base keeps the scalar type key, as parameters already do.
        // ES: `T **` sobre una base escalar sin calificar conserva la clave escalar, como ya ocurre en parametros.
        if (!element && !extraLevels.empty() && recordPointee && recordPointee->kind != CppTypeKind::Record)
            if (auto scalar = mapCppScalarType(*recordPointee))
                element = CppScalarTypeMapping{scalar->nativeSharpTypeKey + extraLevels,
                                               field.type.element->canonicalSpelling, false, false};
        if (!element && lookup && recordPointee && recordPointee->kind == CppTypeKind::Record)
        {
            const auto target = projectCppRecordTypeIdentity(*recordPointee, moduleName);
            const auto* imported = fieldRecordType(*lookup, target, projectOrdinal);
            if (target && imported &&
                (!recordPointee->recordIdentity || !recordPointee->recordIdentity->clangUsr ||
                 !imported->clangUsr || recordPointee->recordIdentity->clangUsr == imported->clangUsr))
                element = CppScalarTypeMapping{"type:" + imported->identity.key() + extraLevels,
                                               field.type.element->canonicalSpelling, false, false};
        }
        if (element)
        {
            mapped = CppScalarTypeMapping{element->nativeSharpTypeKey + "*", field.type.canonicalSpelling,
                                          field.type.isConst, field.type.isVolatile};
            pointeeReadonly = field.type.element->isConst;
            pointeeVolatile = field.type.element->isVolatile;
        }
    }
    // EN: T08-D23: an array of pointers to `const` and/or `volatile` data, or of `const` pointers, as a C record
    //     field: the carrier is the array of unqualified pointers; the pointee qualifiers become the field's
    //     pointee flags (each element keeps them when indexed), and a `const` element makes the field readonly,
    //     as for C globals. Only one pointer level and scalar, enum or record pointees.
    // ES: T08-D23: un arreglo de punteros a datos `const` y/o `volatile`, o de punteros `const`, como campo de
    //     record C: el portador es el arreglo de punteros sin calificar; los calificadores del dato apuntado son
    //     marcas del campo (cada elemento las conserva al indexarse) y un elemento `const` hace el campo de solo
    //     lectura, como en las globales C. Un solo nivel de puntero y datos escalares, enum o record.
    if (!mapped && !record.isCxx && qualifiedPointerArray)
    {
        const CppType* element = &field.type;
        std::vector<std::uint64_t> dimensions;
        bool readonly = false;
        bool isVolatile = false;
        while (element && element->kind == CppTypeKind::Array && dimensions.size() < 64)
        {
            if (!element->arrayCount || *element->arrayCount == 0) { element = nullptr; break; }
            readonly |= element->isConst;
            isVolatile |= element->isVolatile;
            dimensions.push_back(*element->arrayCount);
            element = element->element.get();
        }
        const CppType* pointee = element && element->kind == CppTypeKind::Pointer && element->sizeBits == 64
            ? element->element.get() : nullptr;
        std::optional<CppScalarTypeMapping> target;
        if (pointee && pointee->kind != CppTypeKind::Pointer && pointee->kind != CppTypeKind::Array &&
            pointee->kind != CppTypeKind::Function && pointee->kind != CppTypeKind::Void)
        {
            CppType unqualified = *pointee;
            unqualified.isConst = false;
            unqualified.isVolatile = false;
            target = mapCppScalarType(unqualified);
            if (!target && unqualified.kind == CppTypeKind::Enumeration)
                target = projectCppSemanticEnumType(unqualified, projectOrdinal, moduleName, lookup);
            if (!target && lookup && unqualified.kind == CppTypeKind::Record)
            {
                const auto pointeeIdentity = projectCppRecordTypeIdentity(unqualified, moduleName);
                const auto* imported = fieldRecordType(*lookup, pointeeIdentity, projectOrdinal);
                if (pointeeIdentity && imported &&
                    (!unqualified.recordIdentity || !unqualified.recordIdentity->clangUsr || !imported->clangUsr ||
                     unqualified.recordIdentity->clangUsr == imported->clangUsr))
                    target = CppScalarTypeMapping{"type:" + imported->identity.key(), unqualified.canonicalSpelling,
                                                  false, false};
            }
        }
        if (target && !target->isConst && !target->isVolatile)
        {
            std::string key = target->nativeSharpTypeKey + "*";
            for (auto dimension = dimensions.rbegin(); dimension != dimensions.rend(); ++dimension)
                key += "[" + std::to_string(*dimension) + "]";
            mapped = CppScalarTypeMapping{std::move(key), field.type.canonicalSpelling,
                                          readonly || element->isConst, isVolatile || element->isVolatile};
            pointeeReadonly = pointee->isConst;
            pointeeVolatile = pointee->isVolatile;
        }
    }
    if (!mapped || field.type.kind == CppTypeKind::Void) return std::nullopt;
    CppSemanticInstanceField result{projectOrdinal, identity->key(), field.name,
                                    std::move(*mapped), *field.offsetBits / 8, pointeeReadonly,
                                    *field.type.sizeBits / 8, effectiveAlignment, field.clangUsr};
    result.pointeeVolatile = pointeeVolatile;
    return result;
}

frontend::CppImportedInstanceField toCppImportedInstanceField(const CppSemanticInstanceField& field)
{
    frontend::CppImportedInstanceField result{field.projectOrdinal, field.ownerIdentityKey, field.name,
        field.type.nativeSharpTypeKey, field.byteOffset, field.type.isConst, field.type.isVolatile,
        field.pointeeReadonly, field.byteSize, field.byteAlignment, std::nullopt, field.clangUsr,
        field.bitOffset, field.bitWidth};
    result.pointeeVolatile = field.pointeeVolatile;
    return result;
}

std::optional<CppSemanticStaticField> projectCppSemanticStaticField(
    const CppRecordDeclaration& record, std::size_t fieldIndex, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, const frontend::ProjectNameLookup* lookup)
{
    if (targetTriple != "x86_64-pc-windows-msvc" || !record.complete || fieldIndex >= record.staticFields.size())
        return std::nullopt;
    const auto identity = projectCppRecordIdentity(record, moduleName);
    const auto& field = record.staticFields[fieldIndex];
    if (!identity || field.name.empty() || field.access != CppAccess::Public || field.threadLocal ||
        !field.externalSymbol || field.externalSymbol->empty()) return std::nullopt;
    auto mapped = mapCppScalarType(field.type);
    bool pointeeReadonly = false;
    if (field.type.kind == CppTypeKind::Pointer && field.type.sizeBits == 64 &&
        field.type.element && !field.type.element->isVolatile)
    {
        auto element = mapCppScalarType(*field.type.element);
        if (!element && lookup && field.type.element->kind == CppTypeKind::Record)
        {
            const auto target = projectCppRecordTypeIdentity(*field.type.element, moduleName);
            const auto ordinal = target ? lookup->typeOrdinalForIdentity(target->key()) : std::nullopt;
            const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
            if (target && imported && imported->projectOrdinal == projectOrdinal &&
                (!field.type.element->recordIdentity || !field.type.element->recordIdentity->clangUsr ||
                 !imported->clangUsr || field.type.element->recordIdentity->clangUsr == imported->clangUsr))
                element = CppScalarTypeMapping{"type:" + target->key(), field.type.element->canonicalSpelling, false, false};
        }
        if (element)
        {
            mapped = CppScalarTypeMapping{element->nativeSharpTypeKey + "*", field.type.canonicalSpelling,
                                          field.type.isConst, field.type.isVolatile};
            pointeeReadonly = field.type.element->isConst;
        }
    }
    if (!mapped || field.type.kind == CppTypeKind::Void) return std::nullopt;
    return CppSemanticStaticField{projectOrdinal, identity->key(), field.name, *field.externalSymbol,
                                  std::move(*mapped), field.dllImport, pointeeReadonly};
}

frontend::CppImportedStaticField toCppImportedStaticField(const CppSemanticStaticField& field)
{
    return {field.projectOrdinal, field.ownerIdentityKey, field.name, field.externalSymbol,
            field.type.nativeSharpTypeKey, field.type.isConst, field.type.isVolatile, field.dllImport, field.pointeeReadonly};
}

bool provideExperimentalCppStaticFields(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedStaticField>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    { error = failure->message; return false; }
    const auto& entries = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    auto refined = refineCppRecordGroupsForPointerTransport(entries, groupCppSemanticRecords(entries), projects);
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
    { error = failure->message; return false; }
    const auto& groups = std::get<CppRecordGroups>(refined);
    std::vector<frontend::CppImportedStaticField> result;
    for (const auto& group : groups.groups)
    {
        const auto selected = selectCppRecordMemberDefinition(entries, group);
        if (!selected || group.projectOrdinal >= graph.projects().size()) continue;
        const auto ordinal = lookup.typeOrdinalForIdentity(group.identity.key());
        const auto* owner = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != group.projectOrdinal) continue;
        for (const auto index : {*selected})
        {
            const auto& entry = entries[index];
            if (!entry.declaration.complete) continue;
            for (std::size_t i = 0; i < entry.declaration.staticFields.size(); ++i)
                if (auto field = projectCppSemanticStaticField(entry.declaration, i, group.projectOrdinal,
                    graph.projects()[group.projectOrdinal].moduleName(), entry.targetTriple, &lookup))
                    result.push_back(toCppImportedStaticField(*field));
        }
    }
    output = std::move(result);
    error.clear();
    return true;
}

bool provideExperimentalCppInstanceFields(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    const frontend::ProjectNameLookup& lookup,
    std::vector<frontend::CppImportedInstanceField>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    { error = failure->message; return false; }
    const auto& entries = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    auto refined = refineCppRecordGroupsForPointerTransport(entries, groupCppSemanticRecords(entries), projects);
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
    { error = failure->message; return false; }
    const auto& groups = std::get<CppRecordGroups>(refined);
    std::vector<frontend::CppImportedInstanceField> result;
    for (const auto& group : groups.groups)
    {
        const auto selected = selectCppRecordMemberDefinition(entries, group);
        if (!selected || group.projectOrdinal >= graph.projects().size()) continue;
        const auto ordinal = lookup.typeOrdinalForIdentity(group.identity.key());
        const auto* owner = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (!owner || owner->projectOrdinal != group.projectOrdinal ||
            (!owner->objectLayout && !owner->memberAccessLayout)) continue;
        auto volatileMode = entries[*selected].microsoftVolatileSemantics;
        for (const auto observed : group.inventoryIndices)
            if (entries[observed].microsoftVolatileSemantics != volatileMode)
                volatileMode = std::nullopt;
        for (const auto index : {*selected})
        {
            const auto& entry = entries[index];
            if (!entry.declaration.complete) continue;
            for (std::size_t fieldIndex = 0; fieldIndex < entry.declaration.fields.size(); ++fieldIndex)
                if (auto field = projectCppSemanticInstanceField(entry.declaration, fieldIndex, group.projectOrdinal,
                    graph.projects()[group.projectOrdinal].moduleName(), entry.targetTriple, &lookup))
                {
                    auto imported = toCppImportedInstanceField(*field);
                    imported.isAnonymousCarrier = entry.declaration.fields[fieldIndex].isAnonymousCarrier;
                    if (!imported.clangUsr || imported.clangUsr->empty())
                        for (const auto observed : group.inventoryIndices)
                        {
                            const auto& declaration = entries[observed].declaration;
                            if (!declaration.complete || fieldIndex >= declaration.fields.size()) continue;
                            const auto& usr = declaration.fields[fieldIndex].clangUsr;
                            if (usr && !usr->empty()) { imported.clangUsr = usr; break; }
                        }
                    imported.microsoftVolatileSemantics = volatileMode;
                    result.push_back(std::move(imported));
                }
        }
    }
    output = std::move(result);
    error.clear();
    return true;
}

std::optional<compilation::TypeIdentity> projectCppRecordIdentity(
    const CppRecordDeclaration& record, std::string_view moduleName)
{
    if (record.anonymousContext || record.specializationContext || record.name.empty() || moduleName.empty())
        return std::nullopt;
    std::vector<compilation::TypeNameSegment> enclosing;
    enclosing.reserve(record.containingRecords.size());
    for (const auto& name : record.containingRecords) enclosing.push_back({name, 0});
    compilation::TypeIdentity identity(std::string(moduleName), record.namespaceSegments,
                                       std::move(enclosing), record.name);
    if (!identity.isValid()) return std::nullopt;
    return identity;
}

std::variant<std::vector<CppRecordDeclaration>, std::string> materializeCppAnonymousCRecords(
    const CppRecordDeclaration& owner)
{
    if (owner.isCxx || !owner.complete || owner.name.empty() || owner.anonymousContext)
        return std::string("anonymous C record owner is not a named complete C record");
    std::vector<CppRecordDeclaration> result{owner};
    result.front().anonymousCRecords.clear();
    std::map<std::vector<std::size_t>, std::size_t> ordinalByPath{{{}, 0}};
    std::vector<const CppAnonymousCRecord*> ordered;
    for (const auto& record : owner.anonymousCRecords) ordered.push_back(&record);
    std::sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) {
        return a->carrierPath.size() == b->carrierPath.size() ? a->carrierPath < b->carrierPath
                                                               : a->carrierPath.size() < b->carrierPath.size();
    });
    for (const auto* nested : ordered)
    {
        if (nested->carrierPath.empty()) return std::string("anonymous C record has an empty carrier path");
        auto parentPath = nested->carrierPath;
        const auto index = parentPath.back();
        parentPath.pop_back();
        const auto parent = ordinalByPath.find(parentPath);
        if (parent == ordinalByPath.end() || ordinalByPath.contains(nested->carrierPath))
            return std::string("anonymous C record path is missing a parent or duplicated");
        auto& containing = result[parent->second];
        // EN: T08-D14: a named member of unnamed record type keeps its name; its type gets a synthesized identity
        //     nested in the owner, and its members are not promoted (it is not a carrier).
        // ES: T08-D14: un miembro con nombre de tipo record sin nombre conserva su nombre; su tipo recibe una
        //     identidad sintetizada anidada en el propietario y sus miembros no se promueven (no es portador).
        const bool namedMember = !nested->memberName.empty();
        // EN: for a named member the unnamed record may be the element of arrays and pointers; the chain is
        //     copied before it is changed because CppType shares its elements.
        // ES: para un miembro con nombre el record sin nombre puede ser el elemento de arreglos y punteros; la
        //     cadena se copia antes de cambiarla porque CppType comparte sus elementos.
        const auto innermostRecord = [](CppType& type) -> CppType* {
            CppType* current = &type;
            for (unsigned depth = 0; depth < 64 && current; ++depth)
            {
                if (current->kind == CppTypeKind::Record) return current;
                if ((current->kind != CppTypeKind::Array && current->kind != CppTypeKind::Pointer) ||
                    !current->element) return nullptr;
                auto copy = std::make_shared<CppType>(*current->element);
                CppType* next = copy.get();
                current->element = std::move(copy);
                current = next;
            }
            return nullptr;
        };
        if (index >= containing.fields.size() ||
            (namedMember ? innermostRecord(containing.fields[index].type) == nullptr
                         : containing.fields[index].type.kind != CppTypeKind::Record) ||
            (namedMember ? containing.fields[index].isAnonymousCarrier ||
                           containing.fields[index].name != nested->memberName
                         : !containing.fields[index].isAnonymousCarrier || !nested->siblingMemberIndices.empty()) ||
            !nested->sizeBits || !nested->alignmentBits)
            return std::string("anonymous C record carrier or dimensions are invalid");
        const auto name = namedMember ? "__nsc_c_member_" + nested->memberName
                                      : "__nsc_c_anonymous_" + std::to_string(index);
        if (std::any_of(containing.fields.begin(), containing.fields.end(), [&](const auto& field) {
                return field.name == name;
            })) return std::string("anonymous C carrier name conflicts with a source field");
        CppRecordDeclaration child;
        child.name = name;
        child.qualifiedName = containing.qualifiedName + "::" + name;
        child.namespaceSegments = containing.namespaceSegments;
        child.containingRecords = containing.containingRecords;
        child.containingRecords.push_back(containing.name);
        child.kind = nested->kind;
        child.access = CppAccess::Public;
        child.location = containing.location;
        child.sourceProvenance = containing.sourceProvenance;
        child.complete = true;
        child.sizeBits = nested->sizeBits;
        child.alignmentBits = nested->alignmentBits;
        child.fields = nested->fields;
        child.isCxx = false;
        child.isAnonymousCInternal = true;
        auto& carrier = containing.fields[index];
        auto* carrierRecord = innermostRecord(carrier.type);
        child.clangUsr = carrierRecord->recordIdentity ? carrierRecord->recordIdentity->clangUsr : std::nullopt;
        if (!namedMember) carrier.name = name;
        carrierRecord->recordIdentity = CppRecordTypeIdentity{name, child.namespaceSegments,
            child.containingRecords, false, false, child.clangUsr};
        for (const auto sibling : nested->siblingMemberIndices)
        {
            auto* siblingRecord = sibling < containing.fields.size() && sibling != index &&
                !containing.fields[sibling].isAnonymousCarrier ? innermostRecord(containing.fields[sibling].type)
                                                                : nullptr;
            if (!siblingRecord || !siblingRecord->recordIdentity ||
                siblingRecord->recordIdentity->clangUsr != child.clangUsr)
                return std::string("C member sharing an unnamed record type is invalid");
            siblingRecord->recordIdentity = carrierRecord->recordIdentity;
        }
        ordinalByPath.emplace(nested->carrierPath, result.size());
        result.push_back(std::move(child));
    }
    for (const auto& declaration : result)
        for (const auto& field : declaration.fields)
            if (field.isAnonymousCarrier && field.name.empty())
                return std::string("anonymous C carrier has no captured aggregate");
    return result;
}

std::optional<compilation::TypeIdentity> projectCppRecordTypeIdentity(
    const CppType& type, std::string_view moduleName)
{
    if (type.kind != CppTypeKind::Record || !type.recordIdentity || moduleName.empty()) return std::nullopt;
    const auto& record = *type.recordIdentity;
    if (record.anonymousContext || record.specializationContext || record.name.empty()) return std::nullopt;
    std::vector<compilation::TypeNameSegment> enclosing;
    enclosing.reserve(record.containingRecords.size());
    for (const auto& name : record.containingRecords) enclosing.push_back({name, 0});
    compilation::TypeIdentity identity(std::string(moduleName), record.namespaceSegments,
                                       std::move(enclosing), record.name);
    if (!identity.isValid()) return std::nullopt;
    return identity;
}

CppSemanticRecordsResult collectCppSemanticRecords(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects)
{
    std::vector<CppSemanticRecordEntry> result;
    for (const auto& group : projects)
    {
        std::optional<std::size_t> owner;
        for (std::size_t index = 0; index < graph.projects().size(); ++index)
        {
            std::error_code error;
            const bool same = std::filesystem::equivalent(group.projectFile, graph.projects()[index].projectFile(), error);
            if (error || !same) continue;
            if (owner) return CppSemanticRecordFailure{group.projectFile, 0, "ambiguous project owner"};
            owner = index;
        }
        if (!owner) return CppSemanticRecordFailure{group.projectFile, 0, "project owner not found in graph"};
        for (std::size_t index = 0; index < group.sessions.size(); ++index)
        {
            const auto& session = group.sessions[index];
            if (!session) return CppSemanticRecordFailure{group.projectFile, index, "null C/C++ session"};
            auto declarations = session->recordsSnapshot();
            if (!declarations) return CppSemanticRecordFailure{group.projectFile, index, "C/C++ session is not available"};
            const auto target = session->optionsSnapshot().targetTriple;
            const auto microsoftVolatileSemantics = session->microsoftVolatileSemantics();
            for (auto& declaration : *declarations)
            {
                if (!declaration.isCxx && !declaration.anonymousCRecords.empty())
                {
                    auto expanded = materializeCppAnonymousCRecords(declaration);
                    if (const auto* failure = std::get_if<std::string>(&expanded))
                        return CppSemanticRecordFailure{group.projectFile, index, *failure};
                    for (auto& item : std::get<std::vector<CppRecordDeclaration>>(expanded))
                    {
                        auto identity = projectCppRecordIdentity(item, graph.projects()[*owner].moduleName());
                        result.push_back({group.projectFile, index, *owner, target,
                                          std::move(item), std::move(identity), microsoftVolatileSemantics});
                    }
                }
                else
                {
                    auto identity = projectCppRecordIdentity(declaration, graph.projects()[*owner].moduleName());
                    result.push_back({group.projectFile, index, *owner, target,
                                      std::move(declaration), std::move(identity), microsoftVolatileSemantics});
                }
            }
        }
    }
    return result;
}

std::optional<CppRecordValueObservation> observeCppRecordValueForEntry(
    const CppSemanticRecordEntry& entry, std::span<const CppProjectSessions> projects)
{
    const auto& record = entry.declaration;
    if (!entry.identity || !entry.identity->isValid() || entry.targetTriple.empty() ||
        !record.isCxx || !record.complete || record.qualifiedName.empty() ||
        !record.clangUsr || record.clangUsr->empty() || !record.valueTraits)
        return std::nullopt;
    const CppInteropSession* source = nullptr;
    for (const auto& project : projects)
    {
        std::error_code ec;
        if (!std::filesystem::equivalent(project.projectFile, entry.projectFile, ec) || ec) continue;
        if (source || entry.referenceIndex >= project.sessions.size() || !project.sessions[entry.referenceIndex])
            return std::nullopt;
        source = project.sessions[entry.referenceIndex].get();
    }
    if (!source || source->state() != CppSessionState::Ready ||
        source->optionsSnapshot().targetTriple != entry.targetTriple) return std::nullopt;
    const auto operations = source->recordValueOperations(record.qualifiedName);
    if (!operations || !operations->clangUsr || operations->clangUsr != record.clangUsr) return std::nullopt;
    return CppRecordValueObservation{*record.valueTraits, *operations};
}

CppRecordGroups groupCppSemanticRecords(std::span<const CppSemanticRecordEntry> inventory)
{
    CppRecordGroups result;
    std::map<std::pair<std::size_t, std::string>, std::size_t> indexByIdentity;
    for (std::size_t index = 0; index < inventory.size(); ++index)
    {
        const auto& entry = inventory[index];
        if (!entry.identity) { result.unprojectedIndices.push_back(index); continue; }
        const auto key = std::make_pair(entry.projectOrdinal, entry.identity->key());
        const auto [where, inserted] = indexByIdentity.emplace(key, result.groups.size());
        if (inserted)
            result.groups.push_back({entry.projectOrdinal, *entry.identity, {}, CppRecordGroupStatus::ForwardOnly});
        result.groups[where->second].inventoryIndices.push_back(index);
    }
    for (auto& group : result.groups)
    {
        const CppSemanticRecordEntry* firstDefinition = nullptr;
        const auto& firstEntry = inventory[group.inventoryIndices.front()];
        std::size_t definitions = 0;
        bool conflict = false;
        std::optional<std::string> knownUsr;
        for (const auto index : group.inventoryIndices)
        {
            const auto& entry = inventory[index];
            conflict |= entry.targetTriple != firstEntry.targetTriple;
            if (entry.declaration.clangUsr && !entry.declaration.clangUsr->empty())
            {
                if (knownUsr && knownUsr != entry.declaration.clangUsr) conflict = true;
                else knownUsr = entry.declaration.clangUsr;
            }
            if (!entry.declaration.complete) continue;
            ++definitions;
            if (!firstDefinition) { firstDefinition = &entry; continue; }
            const auto& a = firstDefinition->declaration;
            const auto& b = entry.declaration;
            conflict |= (a.sizeBits && b.sizeBits && a.sizeBits != b.sizeBits) ||
                (a.alignmentBits && b.alignmentBits && a.alignmentBits != b.alignmentBits) ||
                (a.odrHash && b.odrHash && a.odrHash != b.odrHash);
        }
        group.status = conflict ? CppRecordGroupStatus::Conflict :
            definitions == 0 ? CppRecordGroupStatus::ForwardOnly :
            definitions == 1 ? CppRecordGroupStatus::OneDefinition : CppRecordGroupStatus::MultipleDefinitionsUnverified;
        if (!conflict) group.clangUsr = std::move(knownUsr);
    }
    return result;
}

CppRecordStructuralComparisonResult compareCppRecordDefinitions(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroups& groups,
    std::span<const CppProjectSessions> projects)
{
    std::vector<CppRecordStructuralComparison> result;
    std::vector<bool> claimed(inventory.size());
    std::set<std::pair<std::size_t, std::string>> identities;
    for (std::size_t groupIndex = 0; groupIndex < groups.groups.size(); ++groupIndex)
    {
        const auto& group = groups.groups[groupIndex];
        if (!group.identity.isValid() || group.inventoryIndices.empty() ||
            !identities.emplace(group.projectOrdinal, group.identity.key()).second)
            return CppRecordStructuralComparisonFailure{"invalid or duplicate record group"};
        std::vector<const CppInteropSession*> definitions;
        std::string qualifiedName;
        for (const auto index : group.inventoryIndices)
        {
            if (index >= inventory.size() || claimed[index])
                return CppRecordStructuralComparisonFailure{"invalid or duplicate record inventory index"};
            claimed[index] = true;
            const auto& entry = inventory[index];
            if (!entry.identity || entry.identity->key() != group.identity.key() ||
                entry.projectOrdinal != group.projectOrdinal || entry.declaration.qualifiedName.empty())
                return CppRecordStructuralComparisonFailure{"record inventory does not match group identity"};
            if (qualifiedName.empty()) qualifiedName = entry.declaration.qualifiedName;
            else if (qualifiedName != entry.declaration.qualifiedName)
                return CppRecordStructuralComparisonFailure{"record group has inconsistent qualified names"};
            const CppProjectSessions* owner = nullptr;
            for (const auto& candidate : projects)
            {
                std::error_code ec;
                if (std::filesystem::equivalent(candidate.projectFile, entry.projectFile, ec) && !ec)
                {
                    if (owner) return CppRecordStructuralComparisonFailure{"ambiguous session owner"};
                    owner = &candidate;
                }
            }
            if (!owner || entry.referenceIndex >= owner->sessions.size() || !owner->sessions[entry.referenceIndex])
                return CppRecordStructuralComparisonFailure{"record session is missing"};
            if (owner->sessions[entry.referenceIndex]->optionsSnapshot().targetTriple != entry.targetTriple)
                return CppRecordStructuralComparisonFailure{"record target does not match its session"};
            if (entry.declaration.complete) definitions.push_back(owner->sessions[entry.referenceIndex].get());
        }
        if (definitions.size() < 2) continue;
        CppRecordStructuralVerdict verdict = CppRecordStructuralVerdict::Equivalent;
        for (std::size_t i = 1; i < definitions.size(); ++i)
        {
            const auto comparison = definitions.front()->structurallyEquivalentRecord(*definitions[i], qualifiedName);
            if (comparison && !*comparison) verdict = CppRecordStructuralVerdict::Different;
            else if (!comparison && verdict != CppRecordStructuralVerdict::Different)
                verdict = CppRecordStructuralVerdict::Unavailable;
        }
        result.push_back({groupIndex, definitions.size(), verdict});
    }
    return result;
}

namespace {
bool compatibleKnownUsr(const std::optional<std::string>& a, const std::optional<std::string>& b)
{
    return !a || a->empty() || !b || b->empty() || a == b;
}
bool sameMemberType(const CppType& a, const CppType& b, unsigned depth = 0)
{
    if (depth > 64 || a.kind != b.kind || a.canonicalSpelling != b.canonicalSpelling ||
        a.unqualifiedCanonicalSpelling != b.unqualifiedCanonicalSpelling ||
        a.ieeeBinary64 != b.ieeeBinary64 ||
        a.isConst != b.isConst || a.isVolatile != b.isVolatile || a.isRestrict != b.isRestrict || a.sizeBits != b.sizeBits ||
        a.alignmentBits != b.alignmentBits || a.arrayCount != b.arrayCount ||
        a.isIncompleteArray != b.isIncompleteArray ||
        static_cast<bool>(a.element) != static_cast<bool>(b.element) ||
        static_cast<bool>(a.enumUnderlying) != static_cast<bool>(b.enumUnderlying) ||
        a.enumIdentity.has_value() != b.enumIdentity.has_value() ||
        a.recordIdentity.has_value() != b.recordIdentity.has_value() ||
        static_cast<bool>(a.function) != static_cast<bool>(b.function)) return false;
    if (a.enumIdentity && (a.enumIdentity->name != b.enumIdentity->name ||
        a.enumIdentity->qualifiedName != b.enumIdentity->qualifiedName ||
        a.enumIdentity->anonymousContext != b.enumIdentity->anonymousContext ||
        !compatibleKnownUsr(a.enumIdentity->clangUsr, b.enumIdentity->clangUsr))) return false;
    if (a.recordIdentity && (a.recordIdentity->name != b.recordIdentity->name ||
        a.recordIdentity->namespaceSegments != b.recordIdentity->namespaceSegments ||
        a.recordIdentity->containingRecords != b.recordIdentity->containingRecords ||
        a.recordIdentity->anonymousContext != b.recordIdentity->anonymousContext ||
        a.recordIdentity->specializationContext != b.recordIdentity->specializationContext ||
        !compatibleKnownUsr(a.recordIdentity->clangUsr, b.recordIdentity->clangUsr))) return false;
    if (a.element && !sameMemberType(*a.element, *b.element, depth + 1)) return false;
    if (a.enumUnderlying && !sameMemberType(*a.enumUnderlying, *b.enumUnderlying, depth + 1)) return false;
    if (a.function)
    {
        const auto& x = *a.function; const auto& y = *b.function;
        if (x.callingConvention != y.callingConvention || x.hasPrototype != y.hasPrototype ||
            x.variadic != y.variadic || x.parameterTypes.size() != y.parameterTypes.size() ||
            !sameMemberType(x.returnType, y.returnType, depth + 1)) return false;
        for (std::size_t i = 0; i < x.parameterTypes.size(); ++i)
            if (!sameMemberType(x.parameterTypes[i], y.parameterTypes[i], depth + 1)) return false;
    }
    return true;
}
bool sameCapturedMembers(const CppRecordDeclaration& a, const CppRecordDeclaration& b)
{
    if (a.kind != b.kind || a.access != b.access || a.implicitLifetimeMembers != b.implicitLifetimeMembers || a.bases.size() != b.bases.size() ||
        a.fields.size() != b.fields.size() || a.anonymousCRecords.size() != b.anonymousCRecords.size() ||
        a.staticFields.size() != b.staticFields.size() ||
        a.methods.size() != b.methods.size()) return false;
    for (std::size_t i = 0; i < a.bases.size(); ++i)
        if (a.bases[i].access != b.bases[i].access || a.bases[i].isVirtual != b.bases[i].isVirtual ||
            a.bases[i].offsetBits != b.bases[i].offsetBits || !sameMemberType(a.bases[i].type, b.bases[i].type)) return false;
    for (std::size_t i = 0; i < a.fields.size(); ++i)
        if (a.fields[i].name != b.fields[i].name || a.fields[i].access != b.fields[i].access ||
            (a.fields[i].clangUsr && !a.fields[i].clangUsr->empty() && b.fields[i].clangUsr &&
             !b.fields[i].clangUsr->empty() && a.fields[i].clangUsr != b.fields[i].clangUsr) ||
            a.fields[i].offsetBits != b.fields[i].offsetBits || a.fields[i].bitWidth != b.fields[i].bitWidth ||
            a.fields[i].isAnonymousCarrier != b.fields[i].isAnonymousCarrier ||
            !sameMemberType(a.fields[i].type, b.fields[i].type)) return false;
    for (std::size_t i = 0; i < a.anonymousCRecords.size(); ++i)
    {
        const auto& x = a.anonymousCRecords[i]; const auto& y = b.anonymousCRecords[i];
        if (x.carrierPath != y.carrierPath || x.kind != y.kind || x.sizeBits != y.sizeBits ||
            x.alignmentBits != y.alignmentBits || x.fields.size() != y.fields.size() ||
            x.memberName != y.memberName || x.siblingMemberIndices != y.siblingMemberIndices) return false;
        for (std::size_t j = 0; j < x.fields.size(); ++j)
        {
            const auto& left = x.fields[j]; const auto& right = y.fields[j];
            if (left.name != right.name || left.access != right.access || left.offsetBits != right.offsetBits ||
                left.bitWidth != right.bitWidth || left.isAnonymousCarrier != right.isAnonymousCarrier ||
                !compatibleKnownUsr(left.clangUsr, right.clangUsr) || !sameMemberType(left.type, right.type))
                return false;
        }
    }
    for (std::size_t i = 0; i < a.staticFields.size(); ++i)
        if (a.staticFields[i].name != b.staticFields[i].name || a.staticFields[i].access != b.staticFields[i].access ||
            a.staticFields[i].threadLocal != b.staticFields[i].threadLocal || a.staticFields[i].dllImport != b.staticFields[i].dllImport ||
            a.staticFields[i].externalSymbol != b.staticFields[i].externalSymbol ||
            !sameMemberType(a.staticFields[i].type, b.staticFields[i].type)) return false;
    for (std::size_t i = 0; i < a.methods.size(); ++i)
    {
        const auto& x = a.methods[i]; const auto& y = b.methods[i];
        if (x.kind != y.kind || x.name != y.name || x.qualifiedName != y.qualifiedName ||
            x.canonicalFunctionType != y.canonicalFunctionType || x.callingConvention != y.callingConvention ||
            x.refQualifier != y.refQualifier || x.access != y.access || x.isStatic != y.isStatic ||
            x.isConst != y.isConst || x.isVolatile != y.isVolatile || x.isNonThrowing != y.isNonThrowing ||
            x.dllImport != y.dllImport || x.isVirtual != y.isVirtual || x.isPureVirtual != y.isPureVirtual ||
            x.isDeleted != y.isDeleted || x.variadic != y.variadic || x.externalSymbol != y.externalSymbol ||
            x.parameters.size() != y.parameters.size() || !sameMemberType(x.returnType, y.returnType)) return false;
        for (std::size_t p = 0; p < x.parameters.size(); ++p)
            if (x.parameters[p].name != y.parameters[p].name ||
                x.parameters[p].hasDefaultArgument != y.parameters[p].hasDefaultArgument ||
                !sameMemberType(x.parameters[p].type, y.parameters[p].type)) return false;
    }
    return true;
}

// Both trees were checked by sameCapturedMembers before this merge. Clone
// shared children so reconciling identity never changes an input observation.
void mergeMemberTypeIdentity(CppType& destination, const CppType& source)
{
    const auto mergeUsr = [](std::optional<std::string>& into, const std::optional<std::string>& from) {
        if ((!into || into->empty()) && from && !from->empty()) into = from;
    };
    if (destination.recordIdentity && source.recordIdentity)
        mergeUsr(destination.recordIdentity->clangUsr, source.recordIdentity->clangUsr);
    if (destination.enumIdentity && source.enumIdentity)
        mergeUsr(destination.enumIdentity->clangUsr, source.enumIdentity->clangUsr);
    if (destination.element && source.element)
    {
        auto child = std::make_shared<CppType>(*destination.element);
        mergeMemberTypeIdentity(*child, *source.element);
        destination.element = std::move(child);
    }
    if (destination.enumUnderlying && source.enumUnderlying)
    {
        auto child = std::make_shared<CppType>(*destination.enumUnderlying);
        mergeMemberTypeIdentity(*child, *source.enumUnderlying);
        destination.enumUnderlying = std::move(child);
    }
    if (destination.function && source.function)
    {
        auto function = std::make_shared<CppFunctionType>(*destination.function);
        mergeMemberTypeIdentity(function->returnType, source.function->returnType);
        for (std::size_t i = 0; i < function->parameterTypes.size(); ++i)
            mergeMemberTypeIdentity(function->parameterTypes[i], source.function->parameterTypes[i]);
        destination.function = std::move(function);
    }
}
}

std::optional<std::size_t> selectCppRecordMemberDefinition(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroup& group)
{
    if (group.status != CppRecordGroupStatus::OneDefinition &&
        group.status != CppRecordGroupStatus::MultipleDefinitionsPointerCompatible) return std::nullopt;
    std::optional<std::size_t> first;
    std::vector<std::optional<std::string>> knownFieldUsrs;
    std::vector<std::size_t> previousDefinitions;
    for (const auto i : group.inventoryIndices)
    {
        if (i >= inventory.size()) return std::nullopt;
        const auto& item = inventory[i].declaration;
        if (!item.complete) continue;
        if (!first) knownFieldUsrs.resize(item.fields.size());
        if (knownFieldUsrs.size() != item.fields.size()) return std::nullopt;
        for (std::size_t field = 0; field < item.fields.size(); ++field)
        {
            const auto& usr = item.fields[field].clangUsr;
            if (!usr || usr->empty()) continue;
            if (knownFieldUsrs[field] && knownFieldUsrs[field] != usr) return std::nullopt;
            knownFieldUsrs[field] = usr;
        }
        for (const auto previous : previousDefinitions)
            if (!sameCapturedMembers(inventory[previous].declaration, item)) return std::nullopt;
        previousDefinitions.push_back(i);
        if (!first) { first = i; continue; }
        const auto& prior = inventory[*first].declaration;
        // Equivalent C definitions in separate translation units may originate
        // from different headers. The group and member checks above must have
        // established compatibility, and both declarations must name the same
        // Clang identity; C++ retains its stricter source-location rule.
        if (group.status == CppRecordGroupStatus::MultipleDefinitionsPointerCompatible &&
            !prior.isCxx && !item.isCxx && prior.clangUsr && !prior.clangUsr->empty() &&
            item.clangUsr && prior.clangUsr == item.clangUsr) continue;
        if (!prior.location || !item.location || prior.location->line != item.location->line ||
            prior.location->column != item.location->column) return std::nullopt;
        std::error_code ec;
        if (!std::filesystem::equivalent(prior.location->file, item.location->file, ec) || ec) return std::nullopt;
    }
    return first;
}

namespace
{
// EN: T08-D09. A C struct or union defined in several headers (identical text, or one header with guards reached
//     through two references) is one compatible type across translation units (C11 6.2.7). When the group was
//     verified (MultipleDefinitionsPointerCompatible: equivalent structure, same ODR hash and dimensions) and its
//     members were reconciled (selectCppRecordMemberDefinition), every complete observation must also be C with
//     the chosen definition's byte dimensions; then the record has a value layout, as with one definition.
// ES: T08-D09. Un struct o union C definido en varios encabezados (texto identico, o un encabezado con guardas
//     al que llegan dos referencias) es un solo tipo compatible entre unidades de traduccion (C11 6.2.7). Si el
//     grupo se verifico (MultipleDefinitionsPointerCompatible: estructura equivalente, mismo hash ODR y mismas
//     dimensiones) y sus miembros se reconciliaron (selectCppRecordMemberDefinition), toda observacion completa
//     debe ser C con las dimensiones en bytes de la definicion elegida; entonces tiene layout de valor.
bool equivalentCRecordsHaveValueLayout(std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroup& group,
                                       std::size_t chosen)
{
    if (group.status != CppRecordGroupStatus::MultipleDefinitionsPointerCompatible || chosen >= inventory.size())
        return false;
    const auto& selected = inventory[chosen].declaration;
    if (selected.isCxx || !selected.complete || !selected.sizeBits || !selected.alignmentBits ||
        *selected.sizeBits == 0 || *selected.alignmentBits == 0 || *selected.sizeBits % 8 != 0 ||
        *selected.alignmentBits % 8 != 0) return false;
    const auto size = *selected.sizeBits / 8;
    const auto alignment = *selected.alignmentBits / 8;
    if ((alignment & (alignment - 1)) != 0 || size % alignment != 0) return false;
    std::size_t complete = 0;
    for (const auto index : group.inventoryIndices)
    {
        if (index >= inventory.size()) return false;
        const auto& record = inventory[index].declaration;
        if (record.isCxx || (record.kind != CppRecordKind::Struct && record.kind != CppRecordKind::Union) ||
            record.kind != selected.kind) return false;
        if (!record.complete) continue;
        if (record.sizeBits != selected.sizeBits || record.alignmentBits != selected.alignmentBits) return false;
        ++complete;
    }
    return complete >= 2;
}
}

static CppCRecordSnapshotResult captureRecordSnapshot(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroup& group,
    std::span<const CppProjectSessions> cppValueSessions)
{
    const auto fail = [](std::string message) -> CppCRecordSnapshotResult {
        return CppCRecordSnapshotFailure{std::move(message)};
    };
    if (!group.identity.isValid() || group.inventoryIndices.empty() ||
        (group.status != CppRecordGroupStatus::ForwardOnly &&
         group.status != CppRecordGroupStatus::OneDefinition &&
         group.status != CppRecordGroupStatus::MultipleDefinitionsPointerCompatible))
        return fail("C record group is incomplete or not reconciled");
    std::optional<std::string> target;
    std::optional<std::string> knownUsr;
    std::optional<CppRecordKind> kind;
    std::optional<bool> isCpp;
    std::optional<bool> volatileMode;
    bool firstVolatileMode = true;
    bool volatileModeAgrees = true;
    std::size_t completeCount = 0;
    for (const auto index : group.inventoryIndices)
    {
        if (index >= inventory.size()) return fail("C record observation is out of range");
        const auto& entry = inventory[index];
        if (!entry.identity || entry.projectOrdinal != group.projectOrdinal ||
            entry.identity->key() != group.identity.key() ||
            (entry.declaration.isCxx && cppValueSessions.empty()) ||
            (entry.declaration.kind != CppRecordKind::Struct && entry.declaration.kind != CppRecordKind::Union) ||
            entry.targetTriple.empty()) return fail("record is not a consistent C declaration");
        if ((target && *target != entry.targetTriple) || (kind && *kind != entry.declaration.kind) ||
            (isCpp && *isCpp != entry.declaration.isCxx))
            return fail("C record observations disagree on target or kind");
        const auto& usr = entry.declaration.clangUsr;
        if (usr && !usr->empty())
        {
            if (knownUsr && knownUsr != usr) return fail("C record observations disagree on identity");
            knownUsr = usr;
        }
        if (entry.declaration.complete) ++completeCount;
        if (firstVolatileMode) { volatileMode = entry.microsoftVolatileSemantics; firstVolatileMode = false; }
        else if (volatileMode != entry.microsoftVolatileSemantics) volatileModeAgrees = false;
        target = entry.targetTriple;
        kind = entry.declaration.kind;
        isCpp = entry.declaration.isCxx;
    }
    if ((group.status == CppRecordGroupStatus::ForwardOnly && completeCount != 0) ||
        (group.status == CppRecordGroupStatus::OneDefinition && completeCount != 1) ||
        (group.status == CppRecordGroupStatus::MultipleDefinitionsPointerCompatible && completeCount < 2) ||
        (knownUsr && group.clangUsr && !group.clangUsr->empty() && knownUsr != group.clangUsr))
        return fail("C record group does not match its observations");
    if (isCpp == true && (group.status != CppRecordGroupStatus::OneDefinition ||
                          !knownUsr || knownUsr->empty()))
        return fail("C++ record requires one definition and a known identity");
    CppCRecordSnapshot snapshot;
    snapshot.identityKey = group.identity.key();
    if (group.identity.arity() != 0) return fail("C record identity has generic arity");
    snapshot.moduleName = group.identity.moduleName();
    snapshot.namespaceSegments = group.identity.namespaceSegments();
    for (const auto& container : group.identity.containingTypes())
    {
        if (container.arity != 0) return fail("C record container has generic arity");
        snapshot.containingRecords.push_back(container.name);
    }
    snapshot.name = group.identity.name();
    snapshot.targetTriple = *target;
    snapshot.clangUsr = knownUsr;
    snapshot.microsoftVolatileSemantics = volatileModeAgrees ? volatileMode : std::nullopt;
    snapshot.kind = *kind;
    if (group.status == CppRecordGroupStatus::ForwardOnly)
    {
        snapshot.sourceProvenance = inventory[group.inventoryIndices.front()].declaration.sourceProvenance;
        for (const auto index : group.inventoryIndices)
            if (inventory[index].declaration.sourceProvenance != snapshot.sourceProvenance)
            {
                snapshot.sourceProvenance.reset();
                break;
            }
        return snapshot;
    }
    const auto selected = selectCppRecordMemberDefinition(inventory, group);
    if (!selected) return fail("C record definition or member identity is not reconciled");
    const auto& record = inventory[*selected].declaration;
    if (!record.complete || !record.sizeBits || !record.alignmentBits || !*record.sizeBits || !*record.alignmentBits)
        return fail("complete C record has no usable dimensions");
    snapshot.complete = true;
    if (isCpp == true)
    {
        const auto observed = observeCppRecordValueForEntry(inventory[*selected], cppValueSessions);
        if (!observed || observed->operations.clangUsr != knownUsr ||
            !cppRecordValueOperationsEligible(observed->traits, observed->operations))
            return fail("C++ record has no eligible value observation");
        snapshot.cppValueObservation = std::move(observed);
    }
    snapshot.valueLayoutUsable = group.status == CppRecordGroupStatus::OneDefinition ||
        equivalentCRecordsHaveValueLayout(inventory, group, *selected);
    snapshot.sizeBits = record.sizeBits;
    snapshot.alignmentBits = record.alignmentBits;
    snapshot.sourceProvenance = record.sourceProvenance;
    for (const auto index : group.inventoryIndices)
    {
        const auto& observed = inventory[index].declaration;
        if (observed.complete && observed.sourceProvenance != snapshot.sourceProvenance)
        {
            snapshot.sourceProvenance.reset();
            break;
        }
    }
    snapshot.fields = record.fields;
    for (std::size_t field = 0; field < snapshot.fields.size(); ++field)
        for (const auto index : group.inventoryIndices)
        {
            const auto& observed = inventory[index].declaration;
            if (!observed.complete || field >= observed.fields.size() || index == *selected) continue;
            if (!snapshot.fields[field].clangUsr || snapshot.fields[field].clangUsr->empty())
            {
                const auto& usr = observed.fields[field].clangUsr;
                if (usr && !usr->empty()) snapshot.fields[field].clangUsr = usr;
            }
            mergeMemberTypeIdentity(snapshot.fields[field].type, observed.fields[field].type);
        }
    return snapshot;
}

CppCRecordSnapshotResult captureCppCRecordSnapshot(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroup& group)
{
    return captureRecordSnapshot(inventory, group, {});
}

CppCRecordCatalogResult captureCppCRecordCatalog(
    std::span<const CppSemanticRecordEntry> inventory, std::span<const CppRecordGroup> groups)
{
    std::vector<CppCRecordSnapshot> result;
    result.reserve(groups.size());
    for (const auto& group : groups)
    {
        auto captured = captureCppCRecordSnapshot(inventory, group);
        if (const auto* failure = std::get_if<CppCRecordSnapshotFailure>(&captured)) return *failure;
        result.push_back(std::move(std::get<CppCRecordSnapshot>(captured)));
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.identityKey < right.identityKey;
    });
    for (std::size_t i = 1; i < result.size(); ++i)
        if (result[i - 1].identityKey == result[i].identityKey)
            return CppCRecordSnapshotFailure{"duplicate C record identity in catalog"};
    return result;
}

CppCRecordClosureResult collectCppCRecordClosure(
    std::span<const CppSemanticRecordEntry> inventory, std::span<const CppRecordGroup> groups,
    std::size_t projectOrdinal, std::span<const std::string> seedIdentityKeys,
    std::span<const CppProjectSessions> cppValueSessions)
{
    std::map<std::string, std::size_t> byIdentity;
    for (std::size_t i = 0; i < groups.size(); ++i)
        if (groups[i].projectOrdinal == projectOrdinal &&
            !byIdentity.emplace(groups[i].identity.key(), i).second)
            return CppCRecordSnapshotFailure{"ambiguous C record identity in project"};
    std::set<std::size_t> selected;
    std::vector<std::size_t> pending;
    const auto addRecord = [&](const std::string& key, const std::optional<std::string>& usr) -> bool {
        const auto found = byIdentity.find(key);
        if (found == byIdentity.end()) return false;
        const auto& group = groups[found->second];
        if (usr && !usr->empty() && group.clangUsr && !group.clangUsr->empty() && usr != group.clangUsr)
            return false;
        if (selected.insert(found->second).second) pending.push_back(found->second);
        return true;
    };
    for (const auto& seed : seedIdentityKeys)
        if (!addRecord(seed, std::nullopt)) return CppCRecordSnapshotFailure{"C record seed is unavailable: " + seed};
    std::set<std::string> enums;
    std::string failure;
    while (!pending.empty())
    {
        const auto index = pending.back(); pending.pop_back();
        auto captured = captureRecordSnapshot(inventory, groups[index], cppValueSessions);
        if (const auto* why = std::get_if<CppCRecordSnapshotFailure>(&captured)) return *why;
        const auto& record = std::get<CppCRecordSnapshot>(captured);
        const std::string module = groups[index].identity.moduleName();
        std::function<bool(const CppType&, unsigned)> visit = [&](const CppType& type, unsigned depth) {
            if (depth > 64) { failure = "C record dependency exceeds type depth"; return false; }
            if (type.kind == CppTypeKind::Record)
            {
                const auto identity = projectCppRecordTypeIdentity(type, module);
                if (!identity || !addRecord(identity->key(), type.recordIdentity ? type.recordIdentity->clangUsr : std::nullopt))
                { failure = "C record field dependency is unavailable or incompatible"; return false; }
            }
            if (type.kind == CppTypeKind::Enumeration)
            {
                if (!type.enumIdentity)
                { failure = "C enum field dependency has no importable identity"; return false; }
                if (type.enumIdentity->anonymousContext || type.enumIdentity->name.empty())
                {
                    if (!type.enumIdentity->clangUsr || type.enumIdentity->clangUsr->empty() || !type.enumUnderlying ||
                        !type.sizeBits || !type.alignmentBits || type.sizeBits != type.enumUnderlying->sizeBits ||
                        type.alignmentBits != type.enumUnderlying->alignmentBits)
                    { failure = "C anonymous enum field has incomplete identity or layout"; return false; }
                }
                else
                {
                    const auto identity = record.cppValueObservation ? projectCppEnumTypeIdentity(type, module) :
                        std::optional<compilation::TypeIdentity>{compilation::TypeIdentity(module, {}, {}, type.enumIdentity->name)};
                    if (!identity || !identity->isValid()) { failure = "C enum field identity is invalid"; return false; }
                    enums.insert(identity->key());
                }
            }
            if (type.element && !visit(*type.element, depth + 1)) return false;
            if (type.enumUnderlying && !visit(*type.enumUnderlying, depth + 1)) return false;
            if (type.function)
            {
                if (!visit(type.function->returnType, depth + 1)) return false;
                for (const auto& parameter : type.function->parameterTypes)
                    if (!visit(parameter, depth + 1)) return false;
            }
            return true;
        };
        for (const auto& field : record.fields)
            if (!visit(field.type, 0)) return CppCRecordSnapshotFailure{failure};
    }
    CppCRecordClosure result;
    for (const auto index : selected)
    {
        auto captured = captureRecordSnapshot(inventory, groups[index], cppValueSessions);
        if (const auto* why = std::get_if<CppCRecordSnapshotFailure>(&captured)) return *why;
        result.records.push_back(std::move(std::get<CppCRecordSnapshot>(captured)));
    }
    std::sort(result.records.begin(), result.records.end(), [](const auto& left, const auto& right) {
        return left.identityKey < right.identityKey;
    });
    result.enumIdentityKeys.assign(enums.begin(), enums.end());
    return result;
}

bool provideCppCRecordMetadataCatalog(const frontend::ProjectCompilation& compilation,
    const LibraryMetadata& metadata, std::vector<std::uint8_t>& output, std::string& error)
{
    const auto* lookup = compilation.nameLookup();
    const auto* graph = compilation.projectGraph();
    if (!lookup || !graph) { error = "C record metadata requires a compiled project lookup"; return false; }
    metadata_detail::CDependencyCatalog dependencies;
    std::vector<frontend::NamespaceFunctionSymbol> projectedFunctions;
    if (metadata.formatMinor >= 46)
    {
        auto aliasesResult = collectCppSemanticTypeAliases(*graph, compilation.cppProjectSessions());
        if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&aliasesResult))
        { error = failure->message; return false; }
        const auto& aliases = std::get<std::vector<CppSemanticTypeAliasEntry>>(aliasesResult);
        auto candidates = selectCppPublishedTemplateAliases(compilation, metadata, aliases);
        if (const auto* members = compilation.memberModel())
        {
            const auto types = lookup->namedTypes();
            const auto addAlias = [&](const ast::TypeSyntax* syntax) {
                if (!syntax) return;
                for (const auto& binding : lookup->bindings())
                    if (binding.syntax == syntax && binding.expandedCppAlias)
                    {
                        CppPublishedTemplateAliasCandidates candidate{};
                        candidate.resolvedAlias = binding.expandedCppAlias;
                        candidate.hasNameBinding = true;
                        candidates.push_back(std::move(candidate));
                        break;
                    }
            };
            for (const auto& member : members->members())
            {
                if (member.isPrecompiled || member.isCppExternal || member.declaringTypeOrdinal >= types.size()) continue;
                const auto owner = "type:" + types[member.declaringTypeOrdinal].identity.key();
                const bool published = std::any_of(metadata.members.begin(), metadata.members.end(), [&](const auto& item) {
                    return item.declaringTypeKey == owner && item.name == member.name &&
                        item.signatureKey == member.signatureKey && item.kind == static_cast<std::uint8_t>(member.kind);
                });
                if (!published) continue;
                addAlias(member.declaredTypeSyntax);
                addAlias(member.declaredErrorTypeSyntax);
                for (const auto& parameter : member.parameters) addAlias(parameter.typeSyntax);
            }
        }
        if (!captureCppTemplateAliasCandidates(*graph, candidates, aliases, dependencies.aliases, error)) return false;
    }
    if (metadata.formatMinor >= 46 && !metadata.genericTemplates.empty())
    {
        auto functionsResult = collectCppSemanticFunctions(*graph, compilation.cppProjectSessions());
        if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&functionsResult))
        { error = failure->message; return false; }
        auto& functions = std::get<std::vector<CppSemanticInventoryEntry>>(functionsResult);
        std::vector<frontend::CppFunctionDemand> functionDemands;
        for (const auto& item : compilation.cppTemplateDemands()) functionDemands.push_back(item.demand);
        std::vector<CppDeferredFunctionCandidate> deferredFunctions;
        if (!provideExperimentalCppFunctionsWithRecords(*graph, compilation.cppProjectSessions(), *lookup,
                functionDemands, projectedFunctions, deferredFunctions, error)) return false;
        for (auto& entry : functions)
            if (std::holds_alternative<CppSemanticFunctionFailure>(entry.projection))
                for (const auto& projected : projectedFunctions)
                    if (entry.projectOrdinal == projected.projectOrdinal &&
                        entry.declaration.externalSymbol == projected.mangledName &&
                        entry.declaration.clangUsr == projected.cppClangUsr)
                    { entry.projection = projected; break; }
        const auto functionCandidates = selectCppPublishedTemplateFunctions(compilation, metadata, functions);
        if (!captureCppTemplateFunctionCandidates(*graph, functionCandidates, functions, dependencies.functions, error))
            return false;
        for (auto& dependency : dependencies.functions)
        {
            const auto& declaration = dependency.declaration;
            if (!declaration.variadic || !declaration.externalLinkage || !declaration.hasPrototype ||
                declaration.callingConvention != "sysv_abi" || !declaration.clangUsr) continue;
            const CppSemanticInventoryEntry* match = nullptr;
            for (const auto& entry : functions)
            {
                if (entry.projectOrdinal >= graph->projects().size() ||
                    graph->projects()[entry.projectOrdinal].moduleName() != dependency.moduleName ||
                    entry.declaration.clangUsr != declaration.clangUsr ||
                    entry.declaration.externalSymbol != declaration.externalSymbol ||
                    entry.declaration.canonicalFunctionType != declaration.canonicalFunctionType) continue;
                const auto reference = "cpp-reference:" + std::to_string(entry.referenceIndex);
                if (dependency.referenceIdentity != reference &&
                    !dependency.referenceIdentity.starts_with(reference + "#")) continue;
                if (match) { match = nullptr; break; }
                match = &entry;
            }
            if (!match) continue;
            const CppInteropSession* session = nullptr;
            for (const auto& source : compilation.cppProjectSessions())
                if (source.projectFile == match->projectFile && match->referenceIndex < source.sessions.size())
                {
                    if (session) { session = nullptr; break; }
                    session = source.sessions[match->referenceIndex].get();
                }
            if (!session) continue;
            auto base = session->cVariadicCallAbiPlan(declaration.name, declaration.canonicalFunctionType, {});
            if (base && base->symbol == dependency.linkSymbol && base->arguments.size() == declaration.parameters.size())
                dependency.variadicBasePlan = std::make_shared<const abi_plan::VariadicCallPlan>(std::move(*base));
        }
        auto globalsResult = collectCppSemanticGlobals(*graph, compilation.cppProjectSessions());
        if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&globalsResult))
        { error = failure->message; return false; }
        const auto& globals = std::get<std::vector<CppSemanticGlobalEntry>>(globalsResult);
        if (!captureCppTemplateGlobalCandidates(compilation, metadata, globals, dependencies.globals, error))
            return false;
    }
    std::map<std::size_t, std::set<std::string>> seeds;
    std::map<std::size_t, std::set<std::string>> enumSeeds;
    if (!metadata.genericTemplates.empty())
    {
        std::set<std::string> published;
        for (const auto& item : metadata.genericTemplates) published.insert(item.canonicalKey);
        auto inventory = collectCppSemanticEnums(*graph, compilation.cppProjectSessions());
        if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&inventory))
        { error = failure->message; return false; }
        std::vector<frontend::CppFunctionDemand> demands;
        for (const auto& item : compilation.cppTemplateDemands())
            if (published.contains(item.templateKey)) demands.push_back(item.demand);
        std::vector<frontend::NamespaceStateSymbol> constants;
        if (!provideCppSemanticEnumConstants(*graph, compilation.cppProjectSessions(), demands, constants, error)) return false;
        std::set<std::pair<std::size_t, std::string>> capturedEnumerators;
        for (const auto& item : compilation.cppTemplateDemands())
        {
            const auto& demand = item.demand;
            if (!published.contains(item.templateKey) ||
                std::find(demand.lookupNamespaces.begin(), demand.lookupNamespaces.end(), "") == demand.lookupNamespaces.end()) continue;
            for (const auto& entry : std::get<std::vector<CppSemanticEnumEntry>>(inventory))
            {
                if (entry.declaration.isCxx || entry.projectOrdinal >= graph->projects().size() ||
                    std::find(demand.visibleProjects.begin(), demand.visibleProjects.end(), entry.projectOrdinal) == demand.visibleProjects.end()) continue;
                if (std::none_of(entry.declaration.enumerators.begin(), entry.declaration.enumerators.end(),
                    [&](const auto& value) { return value.name == demand.lookupName; })) continue;
                if (entry.declaration.name.empty())
                {
                    if (metadata.formatMinor < 47) continue;
                    const auto selected = std::find_if(constants.begin(), constants.end(), [&](const auto& value) {
                        return value.projectOrdinal == entry.projectOrdinal && value.name == demand.lookupName &&
                            value.isCppEnumConstant && value.cppImportError.empty() && value.cppClangUsr && !value.cppClangUsr->empty();
                    });
                    if (selected == constants.end() || !capturedEnumerators.emplace(entry.projectOrdinal, selected->name).second) continue;
                    const auto declaration = std::find_if(entry.declaration.enumerators.begin(), entry.declaration.enumerators.end(), [&](const auto& value) {
                        return value.name == selected->name && value.clangUsr == selected->cppClangUsr;
                    });
                    const auto digest = declaration == entry.declaration.enumerators.end() ? std::string{} : cEnumeratorSourceDigest(declaration->location);
                    auto reference = "cpp-reference:" + std::to_string(entry.referenceIndex);
                    if (!digest.empty() && declaration->location)
                        reference += "#sha256:" + digest + "#path:" + utf8SourcePath(declaration->location->file);
                    dependencies.enumerators.push_back({selected->moduleName, std::move(reference),
                        selected->name, *selected->cppClangUsr, selected->declaredTypeKey, selected->integerValue});
                    continue;
                }
                compilation::TypeIdentity identity(graph->projects()[entry.projectOrdinal].moduleName(), {}, {}, entry.declaration.name);
                if (identity.isValid()) enumSeeds[entry.projectOrdinal].insert(identity.key());
            }
        }
    }
    std::function<bool(const std::string&, unsigned)> visit = [&](const std::string& key, unsigned depth) {
        if (depth > 64) { error = "metadata function type exceeds depth limit"; return false; }
        if (key.starts_with("fn:"))
        {
            const auto function = language::parseFunctionTypeIdentity(key);
            if (!function) { error = "metadata contains an invalid function type"; return false; }
            if (!visit(function->result, depth + 1) || !visit(function->errorType, depth + 1)) return false;
            for (const auto& parameter : function->parameters)
                if (!visit(parameter, depth + 1)) return false;
            return true;
        }
        const auto base = metadata_detail::metadataBaseTypeKey(key);
        if (!base.starts_with("type:")) return true;
        const auto ordinal = lookup->typeOrdinalForIdentity(std::string_view(base).substr(5));
        const auto* imported = ordinal ? lookup->cppImportedType(*ordinal) : nullptr;
        if (!imported) return true;
        if (imported->origin == frontend::CppImportedTypeOrigin::CEnum)
        { enumSeeds[imported->projectOrdinal].insert(imported->identity.key()); return true; }
        if (imported->origin == frontend::CppImportedTypeOrigin::CppRecord && ordinal &&
            lookup->cppTrivialValueEligible(*ordinal))
        { seeds[imported->projectOrdinal].insert(imported->identity.key()); return true; }
        if (imported->origin != frontend::CppImportedTypeOrigin::CStruct &&
            imported->origin != frontend::CppImportedTypeOrigin::CUnion)
        { error = "imported record metadata origin is unsupported or unresolved: " + base; return false; }
        seeds[imported->projectOrdinal].insert(imported->identity.key());
        return true;
    };
    const auto visitCallable = [&](const auto& callable) {
        if (!visit(callable.declaredTypeKey, 0) || !visit(callable.declaredErrorTypeKey, 0)) return false;
        for (const auto& parameter : callable.parameters) if (!visit(parameter.typeKey, 0)) return false;
        return true;
    };
    for (const auto& type : metadata.types)
    {
        if (!visit(type.baseTypeKey, 0) || !visit(type.delegateReturnTypeKey, 0) ||
            !visit(type.delegateErrorTypeKey, 0)) return false;
        for (const auto& key : type.interfaceTypeKeys) if (!visit(key, 0)) return false;
        for (const auto& key : type.delegateParameterTypeKeys) if (!visit(key, 0)) return false;
        for (const auto& field : type.fields) if (!visit(field.typeKey, 0)) return false;
        for (const auto& item : type.associatedItems) if (!visit(item.typeKey, 0)) return false;
        for (const auto& variant : type.variantCases) if (!visit(variant.payloadTypeKey, 0)) return false;
    }
    for (const auto& member : metadata.members) if (!visitCallable(member)) return false;
    for (const auto& function : metadata.namespaceFunctions) if (!visitCallable(function)) return false;
    for (const auto& state : metadata.namespaceStates) if (!visit(state.declaredTypeKey, 0)) return false;
    for (const auto& specialization : metadata.genericSpecializations)
        for (const auto& key : specialization.typeArguments) if (!visit(key, 0)) return false;
    for (const auto& exported : metadata.exports)
    {
        if (!visit(exported.returnTypeKey, 0)) return false;
        for (const auto& key : exported.parameterTypeKeys) if (!visit(key, 0)) return false;
    }
    for (const auto& dependency : dependencies.functions)
        for (const auto& function : projectedFunctions)
            if (function.moduleName == dependency.moduleName && function.mangledName == dependency.linkSymbol &&
                function.cppClangUsr == dependency.declaration.clangUsr && !visitCallable(function)) return false;
    // A template may name a C global whose record or enum type appears nowhere else in it: seed that type so
    // the consumer can project the global without the header. Una plantilla puede nombrar una global C cuyo
    // tipo record o enum no aparece en otra parte: se siembra para proyectarla sin el encabezado.
    for (const auto& dependency : dependencies.globals)
    {
        const auto owner = std::find_if(graph->projects().begin(), graph->projects().end(),
            [&](const auto& project) { return project.moduleName() == dependency.moduleName; });
        if (owner == graph->projects().end()) continue;
        const auto projected = projectCppPrecompiledCGlobal(dependency,
            static_cast<std::size_t>(owner - graph->projects().begin()), dependency.moduleName,
            "x86_64-pc-windows-msvc", *lookup);
        if (const auto* value = std::get_if<CppSemanticGlobal>(&projected); value && !visit(value->typeKey, 0))
            return false;
    }
    // EN: Template bodies may use imported C tags or C++ record or enum names absent from their signatures.
    //     Seed visible C tags and the first matching qualified-name rung for C++ records and enums.
    // ES: Los cuerpos de plantilla pueden usar etiquetas C o registros o enums C++ ausentes en sus firmas.
    //     Se siembran etiquetas C visibles y el primer nivel de nombre calificado coincidente para C++.
    if (const auto* model = compilation.genericModel(); model && !metadata.genericTemplates.empty())
    {
        std::set<std::string> published;
        for (const auto& item : metadata.genericTemplates) published.insert(item.canonicalKey);
        const auto types = lookup->namedTypes();
        for (const auto& record : model->templates())
        {
            if (!published.contains(record.canonicalKey)) continue;
            for (const auto& candidate : record.foreignReferenceCandidates)
            {
                if (!candidate.typeSyntax || candidate.writtenTypeName.empty()) continue;
                const auto owner = candidate.typeSyntax->originProjectOrdinal().value_or(record.projectOrdinal);
                if (owner >= lookup->modules().size() || !lookup->modules()[owner]) continue;
                const auto visible = lookup->modules()[owner]->visibleProjectOrdinals();
                const auto& lexical = candidate.typeSyntax->originNamespace().value_or(candidate.lexicalNamespace);
                std::vector<std::string> segments;
                for (std::size_t start = 0; start < lexical.size();)
                {
                    const auto end = lexical.find('.', start);
                    segments.push_back(lexical.substr(start, end - start));
                    if (end == std::string::npos) break;
                    start = end + 1;
                }
                frontend::QualifiedNameSite site;
                site.namespaceSegments = segments;
                site.importedNamespaces = candidate.importedNamespaces;
                site.aliases = &candidate.aliases;
                for (const auto& rung : frontend::qualifiedNameLadder(site, candidate.writtenTypeName))
                {
                    bool matched = false;
                    for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
                    {
                        const auto* imported = lookup->cppImportedType(ordinal);
                        if (!imported || (imported->origin != frontend::CppImportedTypeOrigin::CppRecord &&
                                          imported->origin != frontend::CppImportedTypeOrigin::CppEnum) ||
                            (imported->projectOrdinal != owner &&
                             std::find(visible.begin(), visible.end(), imported->projectOrdinal) == visible.end()) ||
                            std::find(rung.spellings.begin(), rung.spellings.end(),
                                      imported->identity.fullyQualifiedName()) == rung.spellings.end()) continue;
                        if (imported->origin == frontend::CppImportedTypeOrigin::CppEnum)
                            enumSeeds[imported->projectOrdinal].insert(imported->identity.key());
                        else seeds[imported->projectOrdinal].insert(imported->identity.key());
                        matched = true;
                    }
                    if (matched) break;
                }
                for (std::size_t ordinal = 0; ordinal < types.size(); ++ordinal)
                {
                    const auto* imported = lookup->cppImportedType(ordinal);
                    if (!imported ||
                        (imported->projectOrdinal != owner &&
                         std::find(visible.begin(), visible.end(), imported->projectOrdinal) == visible.end()))
                        continue;
                    if (imported->origin == frontend::CppImportedTypeOrigin::CppRecord) continue;
                    if (imported->identity.name() != candidate.writtenTypeName ||
                        !imported->identity.containingTypes().empty()) continue;
                    if (imported->origin == frontend::CppImportedTypeOrigin::CEnum)
                        enumSeeds[imported->projectOrdinal].insert(imported->identity.key());
                    else if (imported->origin == frontend::CppImportedTypeOrigin::CStruct ||
                             imported->origin == frontend::CppImportedTypeOrigin::CUnion)
                        seeds[imported->projectOrdinal].insert(imported->identity.key());
                }
            }
        }
    }
    // EN: T08-D25. References declared with PublicTypes="Header" add their header's own C types to the catalog,
    //     even when no published signature uses them (CppPublicHeaderTypes.h states the surface rule).
    // ES: T08-D25. Las referencias declaradas con PublicTypes="Header" agregan al catalogo los tipos C propios de
    //     su encabezado, aunque ninguna firma publicada los use (la regla esta en CppPublicHeaderTypes.h).
    detail::PublicHeaderSelection publicTypes;
    if (!detail::selectPublicHeaderTypes(*graph, compilation.cppProjectSessions(), publicTypes, error)) return false;
    for (const auto& [project, keys] : publicTypes.recordIdentities) seeds[project].insert(keys.begin(), keys.end());
    for (const auto& [project, keys] : publicTypes.enumIdentities) enumSeeds[project].insert(keys.begin(), keys.end());
    if (!publicTypes.aliasCandidates.empty())
    {
        std::vector<metadata_detail::CDependencyAliasSnapshot> captured;
        if (!captureCppTemplateAliasCandidates(*graph, publicTypes.aliasCandidates, publicTypes.aliasInventory,
                captured, error)) return false;
        for (auto& snapshot : captured)
        {
            bool duplicate = false;
            for (const auto& existing : dependencies.aliases)
            {
                if (existing.moduleName == snapshot.moduleName &&
                    existing.declaration.qualifiedName == snapshot.declaration.qualifiedName)
                {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) dependencies.aliases.push_back(std::move(snapshot));
        }
    }
    if (seeds.empty() && enumSeeds.empty() && dependencies.aliases.empty() && dependencies.functions.empty() &&
        dependencies.globals.empty() && dependencies.enumerators.empty())
    { output.clear(); error.clear(); return true; }
    auto inventoryResult = collectCppSemanticRecords(*graph, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&inventoryResult))
    { error = failure->message; return false; }
    const auto& inventory = std::get<std::vector<CppSemanticRecordEntry>>(inventoryResult);
    auto grouped = groupCppSemanticRecords(inventory);
    auto refined = refineCppRecordGroupsForPointerTransport(inventory, grouped, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
    { error = failure->message; return false; }
    const auto& groups = std::get<CppRecordGroups>(refined).groups;
    std::vector<CppSemanticEnumEntry> aliasEnums;
    if (!dependencies.aliases.empty())
    {
        auto observed = collectCppSemanticEnums(*graph, compilation.cppProjectSessions());
        if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&observed))
        { error = failure->message; return false; }
        aliasEnums = std::move(std::get<std::vector<CppSemanticEnumEntry>>(observed));
    }
    // A public generic body may mention only a typedef whose function signature
    // contains a record. The callable metadata above does not see that type:
    // seed its C record closure from the captured alias before publication.
    for (const auto& alias : dependencies.aliases)
    {
        const auto owner = std::find_if(graph->projects().begin(), graph->projects().end(),
            [&](const auto& project) { return project.moduleName() == alias.moduleName; });
        if (owner == graph->projects().end()) continue;
        const auto project = static_cast<std::size_t>(owner - graph->projects().begin());
        const auto visitAliasType = [&](const auto& self, const CppType& type, unsigned depth) -> bool {
            if (depth > 64) { error = "C alias dependency exceeds type depth"; return false; }
            if (type.kind == CppTypeKind::Record && type.recordIdentity &&
                type.recordIdentity->clangUsr && !type.recordIdentity->clangUsr->empty())
            {
                for (const auto& entry : inventory)
                    if (entry.projectOrdinal == project && entry.identity &&
                        entry.declaration.clangUsr == type.recordIdentity->clangUsr)
                    {
                        const auto ordinal = lookup->typeOrdinalForIdentity(entry.identity->key());
                        if (!entry.declaration.isCxx || (ordinal && lookup->cppTrivialValueEligible(*ordinal)))
                            seeds[project].insert(entry.identity->key());
                    }
            }
            if (type.kind == CppTypeKind::Enumeration && type.enumIdentity &&
                type.enumIdentity->clangUsr && !type.enumIdentity->clangUsr->empty())
            {
                for (const auto& entry : aliasEnums)
                    if (entry.projectOrdinal == project && !entry.declaration.isCxx &&
                        !entry.declaration.name.empty() &&
                        entry.declaration.clangUsr == type.enumIdentity->clangUsr)
                    {
                        compilation::TypeIdentity identity(owner->moduleName(), {}, {}, entry.declaration.name);
                        if (identity.isValid()) enumSeeds[project].insert(identity.key());
                    }
            }
            if (type.element && !self(self, *type.element, depth + 1)) return false;
            if (type.function)
            {
                if (!self(self, type.function->returnType, depth + 1)) return false;
                for (const auto& parameter : type.function->parameterTypes)
                    if (!self(self, parameter, depth + 1)) return false;
            }
            return true;
        };
        if (!visitAliasType(visitAliasType, alias.declaration.underlyingType, 0)) return false;
    }
    std::vector<CppCRecordSnapshot> records;
    for (const auto& [project, identities] : seeds)
    {
        const std::vector<std::string> requested(identities.begin(), identities.end());
        auto closure = collectCppCRecordClosure(inventory, groups, project, requested, compilation.cppProjectSessions());
        if (const auto* failure = std::get_if<CppCRecordSnapshotFailure>(&closure))
        { error = failure->message; return false; }
        auto& selected = std::get<CppCRecordClosure>(closure);
        for (auto& record : selected.records)
        {
            if (!record.clangUsr || record.clangUsr->empty() ||
                !makeSimpleCRecordAbiReplay(record)) continue;
            const auto group = std::find_if(groups.begin(), groups.end(), [&](const auto& item) {
                return item.projectOrdinal == project && item.identity.key() == record.identityKey;
            });
            if (group == groups.end()) continue;
            bool sawDefinition = false;
            bool plain = true;
            bool packed = true;
            for (const auto index : group->inventoryIndices)
            {
                if (index >= inventory.size()) { plain = packed = false; break; }
                const auto& entry = inventory[index];
                if (!entry.declaration.complete) continue;
                sawDefinition = true;
                const CppInteropSession* session = nullptr;
                for (const auto& source : compilation.cppProjectSessions())
                    if (source.projectFile == entry.projectFile && entry.referenceIndex < source.sessions.size())
                    {
                        if (session) { plain = packed = false; break; }
                        session = source.sessions[entry.referenceIndex].get();
                    }
                if (!session || entry.declaration.clangUsr != record.clangUsr)
                { plain = packed = false; break; }
                if (record.cppValueObservation)
                {
                    plain = plain && session->cppRecordHasPlainReplaySyntax(entry.declaration.qualifiedName) == true;
                    packed = false;
                }
                else
                {
                    plain = plain && session->cRecordHasPlainReplaySyntax(entry.declaration.qualifiedName) == true;
                    packed = packed && session->cRecordHasPackedReplaySyntax(entry.declaration.qualifiedName) == true;
                }
                if (!plain && !packed) break;
            }
            record.plainReplaySyntax = sawDefinition && plain;
            record.packedReplaySyntax = sawDefinition && packed && record.alignmentBits == 8;
        }
        if (!selected.enumIdentityKeys.empty())
            enumSeeds[project].insert(selected.enumIdentityKeys.begin(), selected.enumIdentityKeys.end());
        records.insert(records.end(), std::make_move_iterator(selected.records.begin()),
                       std::make_move_iterator(selected.records.end()));
    }
    std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) { return a.identityKey < b.identityKey; });
    std::set<std::pair<std::string, std::string>> anonymousEnumFields;
    for (const auto& record : records)
    {
        std::function<void(const CppType&)> collect = [&](const CppType& type) {
            if (type.kind == CppTypeKind::Enumeration && type.enumIdentity &&
                (type.enumIdentity->anonymousContext || type.enumIdentity->name.empty()) && type.enumIdentity->clangUsr)
                anonymousEnumFields.emplace(record.moduleName, *type.enumIdentity->clangUsr);
            if (type.element) collect(*type.element);
        };
        for (const auto& field : record.fields) collect(field.type);
    }
    if (!anonymousEnumFields.empty())
    {
        if (metadata.formatMinor < 47) { error = "anonymous C enum fields require metadata 1.47"; return false; }
        auto observed = collectCppSemanticEnums(*graph, compilation.cppProjectSessions());
        if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&observed))
        { error = failure->message; return false; }
        std::map<std::tuple<std::string, std::string, std::string>, std::pair<std::string, std::uint64_t>> captured;
        for (const auto& value : dependencies.enumerators)
            captured.emplace(std::make_tuple(value.moduleName, value.name, value.clangUsr), std::make_pair(value.typeKey, value.bits));
        std::set<std::pair<std::string, std::string>> matched;
        for (const auto& entry : std::get<std::vector<CppSemanticEnumEntry>>(observed))
        {
            if (entry.projectOrdinal >= graph->projects().size() || entry.declaration.isCxx ||
                !entry.declaration.clangUsr || !entry.declaration.name.empty()) continue;
            const auto& module = graph->projects()[entry.projectOrdinal].moduleName();
            const auto identity = std::make_pair(module, *entry.declaration.clangUsr);
            if (!anonymousEnumFields.contains(identity)) continue;
            matched.insert(identity);
            for (std::size_t i = 0; i < entry.declaration.enumerators.size(); ++i)
            {
                auto projected = projectCppEnumConstant(entry, i);
                const auto* value = std::get_if<CppSemanticEnumConstant>(&projected);
                if (!value || !value->clangUsr || value->clangUsr->empty())
                { error = "C anonymous enum field enumerator is unavailable"; return false; }
                const auto [prior, inserted] = captured.emplace(std::make_tuple(module, value->name, *value->clangUsr),
                    std::make_pair(value->typeKey, value->bits));
                if (!inserted)
                {
                    if (prior->second != std::make_pair(value->typeKey, value->bits))
                    { error = "conflicting C anonymous enum field enumerator"; return false; }
                    continue;
                }
                auto reference = "cpp-reference:" + std::to_string(entry.referenceIndex);
                const auto& declaration = entry.declaration.enumerators[i];
                const auto digest = cEnumeratorSourceDigest(declaration.location);
                if (!digest.empty() && declaration.location)
                    reference += "#sha256:" + digest + "#path:" + utf8SourcePath(declaration.location->file);
                dependencies.enumerators.push_back({module, std::move(reference), value->name,
                    *value->clangUsr, value->typeKey, value->bits});
            }
        }
        if (matched != anonymousEnumFields) { error = "C anonymous enum field definition is unavailable"; return false; }
    }
    std::vector<CppCEnumSnapshot> enums;
    if (!enumSeeds.empty())
    {
        auto enumInventory = collectCppSemanticEnums(*graph, compilation.cppProjectSessions());
        if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&enumInventory))
        { error = failure->message; return false; }
        const auto& entries = std::get<std::vector<CppSemanticEnumEntry>>(enumInventory);
        for (const auto& [project, identities] : enumSeeds)
        {
            if (identities.empty()) continue;
            if (project >= graph->projects().size()) { error = "C enum project is unavailable"; return false; }
            const std::vector<std::string> requested(identities.begin(), identities.end());
            auto captured = captureCppCEnumCatalog(entries, project, graph->projects()[project].moduleName(), requested);
            if (const auto* failure = std::get_if<CppCEnumSnapshotFailure>(&captured))
            { error = failure->message; return false; }
            auto& selected = std::get<std::vector<CppCEnumSnapshot>>(captured);
            enums.insert(enums.end(), std::make_move_iterator(selected.begin()), std::make_move_iterator(selected.end()));
        }
    }
    std::sort(enums.begin(), enums.end(), [](const auto& a, const auto& b) { return a.identityKey < b.identityKey; });
    for (const auto& record : records)
    {
        std::function<bool(const CppType&, unsigned)> checkEnum = [&](const CppType& type, unsigned depth) {
            if (depth > 64) { error = "C enum field dependency exceeds type depth"; return false; }
            if (type.kind == CppTypeKind::Enumeration)
            {
                if (!type.enumIdentity) { error = "C enum field identity is unavailable"; return false; }
                if (type.enumIdentity->anonymousContext || type.enumIdentity->name.empty())
                {
                    if (!type.enumIdentity->clangUsr || type.enumIdentity->clangUsr->empty() || !type.enumUnderlying ||
                        type.sizeBits != type.enumUnderlying->sizeBits || type.alignmentBits != type.enumUnderlying->alignmentBits)
                    { error = "C anonymous enum field has incompatible layout"; return false; }
                    return true;
                }
                const auto identity = record.cppValueObservation ? projectCppEnumTypeIdentity(type, record.moduleName) :
                    std::optional<compilation::TypeIdentity>{compilation::TypeIdentity(record.moduleName, {}, {}, type.enumIdentity->name)};
                const auto found = std::find_if(enums.begin(), enums.end(), [&](const auto& value) {
                    return identity && value.identityKey == identity->key();
                });
                if (!identity || !identity->isValid() || found == enums.end() || found->targetTriple != record.targetTriple ||
                    (type.enumIdentity->clangUsr && found->declaration.clangUsr &&
                     type.enumIdentity->clangUsr != found->declaration.clangUsr) ||
                    (type.sizeBits && type.sizeBits != found->declaration.sizeBits) ||
                    (type.alignmentBits && type.alignmentBits != found->declaration.alignmentBits))
                { error = "C enum field dependency conflicts with its definition: " + (identity ? identity->key() : std::string{}); return false; }
            }
            if (type.element && !checkEnum(*type.element, depth + 1)) return false;
            if (type.enumUnderlying && !checkEnum(*type.enumUnderlying, depth + 1)) return false;
            if (type.function)
            {
                if (!checkEnum(type.function->returnType, depth + 1)) return false;
                for (const auto& parameter : type.function->parameterTypes)
                    if (!checkEnum(parameter, depth + 1)) return false;
            }
            return true;
        };
        for (const auto& field : record.fields) if (!checkEnum(field.type, 0)) return false;
    }
    const bool hasEnumProvenance = std::any_of(enums.begin(), enums.end(), [](const auto& value) {
        return value.declaration.sourceProvenance.has_value();
    });
    if (hasEnumProvenance && metadata.formatMinor >= 49)
        return metadata_detail::encodeCTypeCatalogWithEnumProvenance(records, enums, dependencies, output, error);
    const bool hasAnonymousCarriers = std::any_of(records.begin(), records.end(), [](const auto& record) {
        return std::any_of(record.fields.begin(), record.fields.end(), [](const auto& field) {
            return field.isAnonymousCarrier;
        });
    });
    if (hasAnonymousCarriers)
    {
        if (metadata.formatMinor < 48) { error = "anonymous C carriers require metadata 1.48"; return false; }
        return metadata_detail::encodeCTypeCatalogWithAnonymousCarriers(records, enums, dependencies, output, error);
    }
    if (!dependencies.aliases.empty() || !dependencies.functions.empty() || !dependencies.globals.empty() || !dependencies.enumerators.empty())
        return metadata_detail::encodeCTypeCatalogWithDependencies(records, enums, dependencies, output, error);
    if (metadata.formatMinor >= 43)
    {
        if (!metadata_detail::encodeCTypeCatalogWithProvenance(records, enums, output, error)) return false;
        if (metadata.formatMinor < 45 && !output.empty() && output[0] >= 3)
        { error = "metadata before 1.45 cannot represent incomplete C source provenance"; output.clear(); return false; }
        if (metadata.formatMinor < 44 && !output.empty() && output[0] == 2)
        { error = "metadata before 1.44 cannot represent C floating format"; output.clear(); return false; }
        return true;
    }
    std::vector<std::uint8_t> checked;
    if (!metadata_detail::encodeCTypeCatalogWithProvenance(records, enums, checked, error)) return false;
    if (!checked.empty() && checked[0] >= 3)
    { error = "metadata before 1.45 cannot represent incomplete C source provenance"; return false; }
    if (!checked.empty() && checked[0] == 2)
    { error = "metadata before 1.44 cannot represent C floating format"; return false; }
    return metadata_detail::encodeCTypeCatalog(records, enums, output, error);
}

bool providePrecompiledCParserTypes(const frontend::ProjectCompilation& compilation,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error)
{
    const auto* graph = compilation.projectGraph();
    if (!graph) { error = "C parser type selection requires a project graph"; return false; }
    std::vector<frontend::CppImportedTypeDeclaration> restored;
    if (!metadata_detail::reconstructPrecompiledCImportedTypes(compilation.precompiledModules(), *graph,
            "x86_64-pc-windows-msvc", restored, error)) return false;
    output.insert(output.end(), std::make_move_iterator(restored.begin()), std::make_move_iterator(restored.end()));
    return true;
}

bool selectCppTypesWithPrecompiledCatalogs(const frontend::ProjectCompilation& compilation,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error)
{
    const auto* graph = compilation.projectGraph();
    if (!graph) { error = "C type selection requires a project graph"; return false; }
    std::vector<frontend::CppImportedTypeDeclaration> selected = output;
    if (!compilation.cppProjectSessions().empty() &&
        !selectExperimentalCppRecordTypesForCompilation(compilation, selected, error)) return false;
    std::vector<frontend::CppImportedTypeDeclaration> precompiled;
    if (!metadata_detail::reconstructPrecompiledCImportedTypes(compilation.precompiledModules(), *graph,
            "x86_64-pc-windows-msvc", precompiled, error)) return false;
    std::set<std::string> identities;
    for (const auto& type : selected)
        if (!identities.insert(type.identity.key()).second)
        { error = "duplicate selected C/C++ type: " + type.identity.key(); return false; }
    for (auto& type : precompiled)
    {
        if (!identities.insert(type.identity.key()).second)
        { error = "live and precompiled C/C++ type identity requires reconciliation: " + type.identity.key(); return false; }
        selected.push_back(std::move(type));
    }
    output = std::move(selected); error.clear(); return true;
}

bool reconstructCImportedFields(std::span<const CppCRecordSnapshot> records,
    std::size_t projectOrdinal, std::string_view moduleName, std::string_view targetTriple,
    const frontend::ProjectNameLookup& lookup, std::vector<frontend::CppImportedInstanceField>& output,
    std::string& error)
{
    std::vector<frontend::CppImportedInstanceField> result;
    for (const auto& snapshot : records)
    {
        const auto ordinal = lookup.typeOrdinalForIdentity(snapshot.identityKey);
        const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        const auto expectedOrigin = snapshot.cppValueObservation ? frontend::CppImportedTypeOrigin::CppRecord :
            snapshot.kind == CppRecordKind::Union ? frontend::CppImportedTypeOrigin::CUnion :
                                                   frontend::CppImportedTypeOrigin::CStruct;
        if ((snapshot.kind != CppRecordKind::Struct && snapshot.kind != CppRecordKind::Union) ||
            snapshot.moduleName != moduleName || snapshot.targetTriple != targetTriple || !imported ||
            imported->projectOrdinal != projectOrdinal || imported->origin != expectedOrigin ||
            imported->targetTriple != snapshot.targetTriple || imported->clangUsr != snapshot.clangUsr ||
            imported->isIncomplete != !snapshot.complete ||
            (snapshot.cppValueObservation && (!ordinal || !lookup.cppTrivialValueEligible(*ordinal))))
        { error = "C record is not installed with matching identity"; return false; }
        if (!snapshot.complete) continue;
        const auto& layout = snapshot.valueLayoutUsable ? imported->objectLayout : imported->memberAccessLayout;
        if (!snapshot.sizeBits || !snapshot.alignmentBits || !layout || *snapshot.sizeBits % 8 ||
            *snapshot.alignmentBits % 8 || layout->sizeBytes != *snapshot.sizeBits / 8 ||
            layout->alignmentBytes != *snapshot.alignmentBits / 8)
        { error = "C record is not installed with matching layout"; return false; }
        CppRecordDeclaration record;
        record.name = snapshot.name;
        record.namespaceSegments = snapshot.namespaceSegments;
        record.containingRecords = snapshot.containingRecords;
        record.kind = snapshot.kind;
        record.complete = true;
        record.isCxx = snapshot.cppValueObservation.has_value();
        record.sizeBits = snapshot.sizeBits;
        record.alignmentBits = snapshot.alignmentBits;
        record.fields = snapshot.fields;
        for (std::size_t i = 0; i < record.fields.size(); ++i)
        {
            if (record.fields[i].name.empty() && record.fields[i].bitWidth) continue;
            const auto field = projectCppSemanticInstanceField(record, i, projectOrdinal, moduleName, targetTriple, &lookup);
            if (!field || field->ownerIdentityKey != snapshot.identityKey)
            { error = "C record field cannot be reconstructed: " + snapshot.identityKey + "." + record.fields[i].name; return false; }
            auto projected = toCppImportedInstanceField(*field);
            projected.isAnonymousCarrier = record.fields[i].isAnonymousCarrier;
            projected.microsoftVolatileSemantics = snapshot.microsoftVolatileSemantics;
            result.push_back(std::move(projected));
        }
    }
    output = std::move(result); error.clear(); return true;
}

bool reconstructCEnumConstants(std::span<const CppCEnumSnapshot> enums,
    std::size_t projectOrdinal, std::string_view moduleName, std::string_view targetTriple,
    const frontend::ProjectNameLookup& lookup, std::vector<frontend::NamespaceStateSymbol>& output,
    std::string& error)
{
    std::vector<frontend::NamespaceStateSymbol> result;
    std::set<std::pair<std::string, std::string>> names;
    for (const auto& snapshot : enums)
    {
        const auto ordinal = lookup.typeOrdinalForIdentity(snapshot.identityKey);
        const auto* imported = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
        if (snapshot.targetTriple != targetTriple || !imported || imported->projectOrdinal != projectOrdinal ||
            imported->origin != (snapshot.declaration.isCxx ? frontend::CppImportedTypeOrigin::CppEnum :
                frontend::CppImportedTypeOrigin::CEnum) || imported->targetTriple != snapshot.targetTriple ||
            imported->clangUsr != snapshot.declaration.clangUsr)
        { error = "C enum is not installed with matching identity"; return false; }
        if (snapshot.declaration.isCxx && snapshot.declaration.scoped) continue;
        CppSemanticEnumEntry entry{{}, 0, projectOrdinal, snapshot.targetTriple, snapshot.declaration};
        std::string enumNamespace;
        for (const auto& segment : imported->identity.namespaceSegments())
        {
            if (!enumNamespace.empty()) enumNamespace += '.';
            enumNamespace += segment;
        }
        for (std::size_t i = 0; i < entry.declaration.enumerators.size(); ++i)
        {
            auto projected = snapshot.declaration.isCxx ? projectCppCxxEnumConstant(entry, i, moduleName) :
                projectCppEnumConstant(entry, i);
            if (const auto* failure = std::get_if<CppSemanticEnumConstantFailure>(&projected))
            { error = failure->message; return false; }
            const auto& value = std::get<CppSemanticEnumConstant>(projected);
            if (!names.emplace(enumNamespace, value.name).second)
            { error = "conflicting C enumerator name: " + value.name; return false; }
            frontend::NamespaceStateSymbol state;
            state.projectOrdinal = projectOrdinal;
            state.moduleName = std::string(moduleName);
            state.namespaceName = enumNamespace;
            state.name = value.name;
            state.accessibility = frontend::NamespaceFunctionAccessibility::Public;
            state.isConst = true;
            state.isCppEnumConstant = true;
            state.cppClangUsr = value.clangUsr;
            state.declaredTypeKey = value.typeKey;
            state.initializerKind = frontend::NamespaceStateInitializerKind::Integer;
            state.integerValue = value.bits;
            state.identityKey = "state:" + state.moduleName + "::" + enumNamespace + "." + state.name;
            state.storageName = "__nsc_cenum_" + std::to_string(state.moduleName.size()) + "_" +
                state.moduleName + "_" + std::to_string(state.name.size()) + "_" + state.name;
            if (!enumNamespace.empty()) state.storageName += "_ns_" + compilation::symbolFragmentForIdentityKey(enumNamespace);
            result.push_back(std::move(state));
        }
    }
    output = std::move(result); error.clear(); return true;
}

bool reconstructPrecompiledCMembers(std::span<const frontend::PrecompiledSyntaxModule> modules,
    const compilation::ProjectGraph& graph, std::string_view targetTriple,
    const frontend::ProjectNameLookup& lookup, std::vector<frontend::CppImportedInstanceField>& fields,
    std::vector<frontend::NamespaceStateSymbol>& constants, std::string& error)
{
    std::vector<frontend::CppImportedTypeDeclaration> checkedTypes;
    if (!metadata_detail::reconstructPrecompiledCImportedTypes(modules, graph, targetTriple, checkedTypes, error))
        return false;
    std::vector<frontend::CppImportedInstanceField> resultFields;
    std::vector<frontend::NamespaceStateSymbol> resultConstants;
    std::set<std::string> seenTypes;
    std::set<std::tuple<std::size_t, std::string, std::string>> seenConstants;
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        if (!metadata_detail::decodeVersionedCTypeCatalog(module.cppCTypeCatalog, records, enums, error,
                                                          module.cppCTypeCatalogFormatMinor)) return false;
        for (const auto& record : records)
        {
            // EN: T08-D26: the fields and enumerators of a reconciled producer type are those of its canonical type.
            // ES: T08-D26: los campos y enumeradores de un tipo conciliado son los de su tipo canonico.
            if (lookup.isReconciledCppTypeIdentity(record.identityKey)) continue;
            if (!seenTypes.insert(record.identityKey).second) continue;
            const auto ordinal = lookup.typeOrdinalForIdentity(record.identityKey);
            const auto* owner = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
            if (!owner) { error = "precompiled C record is missing from name lookup"; return false; }
            std::vector<frontend::CppImportedInstanceField> one;
            if (!reconstructCImportedFields(std::span(&record, 1), owner->projectOrdinal,
                    owner->identity.moduleName(), targetTriple, lookup, one, error)) return false;
            resultFields.insert(resultFields.end(), std::make_move_iterator(one.begin()), std::make_move_iterator(one.end()));
        }
        for (const auto& enumeration : enums)
        {
            if (lookup.isReconciledCppTypeIdentity(enumeration.identityKey)) continue;
            if (!seenTypes.insert(enumeration.identityKey).second) continue;
            const auto ordinal = lookup.typeOrdinalForIdentity(enumeration.identityKey);
            const auto* owner = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
            if (!owner) { error = "precompiled C enum is missing from name lookup"; return false; }
            std::vector<frontend::NamespaceStateSymbol> one;
            if (!reconstructCEnumConstants(std::span(&enumeration, 1), owner->projectOrdinal,
                    owner->identity.moduleName(), targetTriple, lookup, one, error)) return false;
            for (auto& constant : one)
            {
                if (!seenConstants.emplace(constant.projectOrdinal, constant.namespaceName, constant.name).second)
                { error = "precompiled C enumerators have conflicting names: " + constant.name; return false; }
                resultConstants.push_back(std::move(constant));
            }
        }
    }
    fields = std::move(resultFields); constants = std::move(resultConstants); error.clear(); return true;
}

bool provideCppFieldsWithPrecompiledCatalogs(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects, std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, std::vector<frontend::CppImportedInstanceField>& output,
    std::string& error)
{
    std::vector<frontend::CppImportedInstanceField> live, restored;
    std::vector<frontend::NamespaceStateSymbol> constants;
    if (!provideExperimentalCppInstanceFields(graph, projects, lookup, live, error) ||
        !reconstructPrecompiledCMembers(modules, graph, "x86_64-pc-windows-msvc", lookup,
                                       restored, constants, error)) return false;
    std::set<std::pair<std::string, std::string>> names;
    for (const auto& field : live)
        if (!names.emplace(field.ownerIdentityKey, field.name).second)
        { error = "duplicate live C/C++ field: " + field.ownerIdentityKey + "." + field.name; return false; }
    for (auto& field : restored)
    {
        if (!names.emplace(field.ownerIdentityKey, field.name).second)
        { error = "live and precompiled C field require reconciliation: " + field.ownerIdentityKey + "." + field.name; return false; }
        live.push_back(std::move(field));
    }
    output = std::move(live); error.clear(); return true;
}

bool provideCppValuesWithPrecompiledCatalogs(const compilation::ProjectGraph& graph,
    std::span<const CppProjectSessions> projects, std::span<const frontend::PrecompiledSyntaxModule> modules,
    const frontend::ProjectNameLookup& lookup, std::span<const frontend::CppFunctionDemand> demands,
    std::vector<frontend::NamespaceStateSymbol>& output, std::string& error)
{
    std::vector<frontend::NamespaceStateSymbol> live, constants;
    std::vector<frontend::CppImportedInstanceField> fields;
    if (!provideCppSemanticValuesWithLookup(graph, projects, lookup, demands, live, error) ||
        !reconstructPrecompiledCMembers(modules, graph, "x86_64-pc-windows-msvc", lookup,
                                       fields, constants, error)) return false;
    const auto liveCount = live.size();
    using Key = std::tuple<std::size_t, std::string, std::string>;
    std::map<Key, std::size_t> names;
    for (std::size_t i = 0; i < live.size(); ++i)
        if (!names.emplace(Key{live[i].projectOrdinal, live[i].namespaceName, live[i].name}, i).second)
        { error = "duplicate live C/C++ value: " + live[i].name; return false; }
    std::map<std::string, std::size_t> owners;
    std::map<std::tuple<std::size_t, std::string, std::string>, std::pair<std::string, std::string>> enumeratorSources;
    std::set<std::string> ambiguous;
    for (std::size_t i = 0; i < graph.projects().size(); ++i)
        if (!owners.emplace(graph.projects()[i].moduleName(), i).second)
            ambiguous.insert(graph.projects()[i].moduleName());
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        metadata_detail::CDependencyCatalog dependencies;
        if (!metadata_detail::decodeCTypeCatalogAndDependencies(module.cppCTypeCatalog,
                module.cppCTypeCatalogFormatMinor, records, enums, dependencies, error)) return false;
        for (const auto& global : dependencies.globals)
        {
            const auto owner = owners.find(global.moduleName);
            if (owner == owners.end() || ambiguous.contains(global.moduleName))
            { error = "precompiled C global owner is missing or ambiguous: " + global.moduleName; return false; }
            const bool demanded = std::any_of(demands.begin(), demands.end(), [&](const auto& demand) {
                return demand.lookupName == global.declaration.name &&
                    std::find(demand.visibleProjects.begin(), demand.visibleProjects.end(), owner->second) != demand.visibleProjects.end() &&
                    std::find(demand.lookupNamespaces.begin(), demand.lookupNamespaces.end(), "") != demand.lookupNamespaces.end();
            });
            if (!demanded) continue;
            auto projected = projectCppPrecompiledCGlobal(global, owner->second, global.moduleName,
                                                           "x86_64-pc-windows-msvc", lookup);
            if (const auto* failure = std::get_if<CppSemanticGlobalFailure>(&projected))
            {
                // Keep the producer's unimportable declaration as a state that body analysis reports when it is
                // used, exactly like a live one: it can no longer be joined in silence with another declaration.
                // Se conserva la declaración no importable del productor como un estado que el análisis del cuerpo
                // informa al usarse, igual que uno vivo: ya no puede unirse en silencio con otra declaración.
                frontend::NamespaceStateSymbol state;
                state.projectOrdinal = owner->second;
                state.moduleName = global.moduleName;
                state.name = global.declaration.name;
                state.accessibility = frontend::NamespaceFunctionAccessibility::Public;
                state.isCppExternal = true;
                state.declaredTypeKey = "builtin:int"; // Never emitted; body analysis reports the reason.
                state.cppClangUsr = global.declaration.clangUsr;
                state.cppImportError = failure->message;
                const Key key{state.projectOrdinal, state.namespaceName, state.name};
                if (names.contains(key) || global.declaration.qualifiedName != global.declaration.name) continue;
                names.emplace(key, live.size());
                live.push_back(std::move(state));
                continue;
            }
            const auto& value = std::get<CppSemanticGlobal>(projected);
            frontend::NamespaceStateSymbol state;
            state.projectOrdinal = owner->second;
            state.moduleName = value.moduleName;
            state.namespaceName = value.namespaceName;
            state.name = value.name;
            state.accessibility = global.declaration.externalLinkage
                ? frontend::NamespaceFunctionAccessibility::Public
                : frontend::NamespaceFunctionAccessibility::Internal;
            state.isCppExternal = true;
            state.cppCGlobal = true;
            state.declaredTypeKey = value.typeKey;
            state.storageName = value.externalSymbol;
            state.isVolatile = value.isVolatile;
            state.isCppReadonly = value.isConst;
            state.cppPointerPointeeReadonly = value.pointerPointeeReadonly;
            state.cppPointerPointeeVolatile = value.pointerPointeeVolatile;
            state.cppMicrosoftVolatileSemantics = value.microsoftVolatileSemantics;
            state.cppStorageSizeBits = value.storageSizeBits;
            state.cppDeclarationAlignmentBits = value.declarationAlignmentBits;
            state.cppClangUsr = value.clangUsr;
            state.cppDllImport = value.dllImport;
            state.cppThreadLocal = value.threadLocal;
            state.cppAtomicStorage = value.atomicStorage;
            state.cppCInternalLinkage = !global.declaration.externalLinkage;
            state.cppCanonicalCType = value.canonicalCppType;
            const auto tlsDllMarker = global.referenceIdentity.rfind("#tls-dll:");
            if (tlsDllMarker != std::string::npos)
                state.cppTlsRuntimeIdentity = global.referenceIdentity.substr(tlsDllMarker + 9);
            const Key key{state.projectOrdinal, state.namespaceName, state.name};
            const auto existing = names.find(key);
            if (existing != names.end())
            {
                const auto& other = live[existing->second];
                if (!other.cppCGlobal || other.storageName != state.storageName ||
                    other.declaredTypeKey != state.declaredTypeKey || other.cppClangUsr != state.cppClangUsr ||
                    other.isVolatile != state.isVolatile || other.cppThreadLocal != state.cppThreadLocal ||
                    other.cppAtomicStorage != state.cppAtomicStorage || other.cppDllImport != state.cppDllImport)
                { error = "conflicting precompiled C global: " + state.name; return false; }
                continue;
            }
            names.emplace(key, live.size());
            live.push_back(std::move(state));
        }
        for (const auto& enumerator : dependencies.enumerators)
        {
            const auto owner = owners.find(enumerator.moduleName);
            if (owner == owners.end() || ambiguous.contains(enumerator.moduleName))
            { error = "precompiled C enumerator owner is missing or ambiguous: " + enumerator.moduleName; return false; }
            const auto marker = enumerator.referenceIdentity.rfind("#sha256:");
            const auto pathMarker = marker == std::string::npos ? std::string::npos : marker + 8 + 64;
            if (pathMarker != std::string::npos && pathMarker + 6 < enumerator.referenceIdentity.size() &&
                enumerator.referenceIdentity.compare(pathMarker, 6, "#path:") == 0)
                enumeratorSources[{owner->second, enumerator.name, enumerator.clangUsr}] = {
                    enumerator.referenceIdentity.substr(marker + 8, 64), enumerator.referenceIdentity.substr(pathMarker + 6)};
            frontend::NamespaceStateSymbol state;
            state.projectOrdinal = owner->second;
            state.moduleName = enumerator.moduleName;
            state.name = enumerator.name;
            state.accessibility = frontend::NamespaceFunctionAccessibility::Public;
            state.isConst = true;
            state.isCppEnumConstant = true;
            state.cppClangUsr = enumerator.clangUsr;
            state.declaredTypeKey = enumerator.typeKey;
            state.initializerKind = frontend::NamespaceStateInitializerKind::Integer;
            state.integerValue = enumerator.bits;
            state.identityKey = "state:" + state.moduleName + "::." + state.name;
            state.storageName = "__nsc_cenum_" + std::to_string(state.moduleName.size()) + "_" +
                state.moduleName + "_" + std::to_string(state.name.size()) + "_" + state.name;
            constants.push_back(std::move(state));
        }
    }
    for (std::size_t index = liveCount; index < live.size(); ++index) live[index].cppFromPrecompiledCatalog = true;
    for (auto& constant : constants)
    {
        constant.cppFromPrecompiledCatalog = true;
        const bool demanded = std::any_of(demands.begin(), demands.end(), [&](const auto& demand) {
            return demand.lookupName == constant.name &&
                std::find(demand.visibleProjects.begin(), demand.visibleProjects.end(), constant.projectOrdinal) != demand.visibleProjects.end() &&
                std::find(demand.lookupNamespaces.begin(), demand.lookupNamespaces.end(), constant.namespaceName) != demand.lookupNamespaces.end();
        });
        if (!demanded) continue;
        const Key key{constant.projectOrdinal, constant.namespaceName, constant.name};
        const auto found = names.find(key);
        if (found == names.end())
        {
            names.emplace(key, live.size());
            live.push_back(std::move(constant));
        }
        else
        {
            auto& candidate = live[found->second];
            candidate.isCppExternal = true;
            candidate.isCppEnumConstant = false;
            candidate.isConst = false;
            candidate.storageName.clear();
            candidate.cppImportError = "precompiled C enumerator conflicts with another imported value";
        }
    }
    if (liveCount && live.size() > liveCount)
    {
        auto observed = collectCppSemanticEnums(graph, projects);
        if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&observed))
        { error = failure->message; return false; }
        const auto& enums = std::get<std::vector<CppSemanticEnumEntry>>(observed);
        for (std::size_t index = 0; index < liveCount; ++index)
        {
            auto& item = live[index];
            if (!item.isCppEnumConstant || !item.cppClangUsr || item.cppClangUsr->empty() ||
                item.projectOrdinal >= graph.projects().size()) continue;
            std::set<std::string> candidates;
            for (const auto& enumeration : enums)
            {
                if (enumeration.projectOrdinal != item.projectOrdinal || enumeration.declaration.isCxx) continue;
                if (enumeration.declaration.name.empty())
                {
                    for (const auto& value : enumeration.declaration.enumerators)
                    {
                        if (value.name != item.name || value.clangUsr != item.cppClangUsr) continue;
                        const auto digest = cEnumeratorSourceDigest(value.location);
                        if (digest.empty()) continue;
                        for (std::size_t target = liveCount; target < live.size(); ++target)
                        {
                            const auto& other = live[target];
                            const auto source = enumeratorSources.find({other.projectOrdinal, other.name, *item.cppClangUsr});
                            std::error_code sourceError;
                            const bool sameFile = source != enumeratorSources.end() && value.location &&
                                std::filesystem::equivalent(value.location->file, source->second.second, sourceError) && !sourceError;
                            if (sameFile && source->second.first == digest && other.isCppEnumConstant &&
                                other.cppClangUsr == item.cppClangUsr && other.name == item.name &&
                                other.declaredTypeKey == item.declaredTypeKey && other.integerValue == item.integerValue &&
                                other.cppImportError.empty() && item.cppImportError.empty()) candidates.insert(other.identityKey);
                        }
                    }
                    continue;
                }
                const bool hasValue = std::any_of(enumeration.declaration.enumerators.begin(),
                    enumeration.declaration.enumerators.end(), [&](const auto& value) {
                        return value.name == item.name && value.clangUsr == item.cppClangUsr;
                    });
                if (!hasValue) continue;
                const compilation::TypeIdentity enumIdentity(graph.projects()[item.projectOrdinal].moduleName(),
                    {}, {}, enumeration.declaration.name);
                if (!lookup.isReconciledCppTypeIdentity(enumIdentity.key())) continue;
                const auto ordinal = lookup.typeOrdinalForIdentity(enumIdentity.key());
                const auto* canonical = ordinal ? lookup.cppImportedType(*ordinal) : nullptr;
                if (!canonical || canonical->origin != frontend::CppImportedTypeOrigin::CEnum) continue;
                for (std::size_t target = liveCount; target < live.size(); ++target)
                {
                    const auto& other = live[target];
                    if (other.projectOrdinal == canonical->projectOrdinal && other.isCppEnumConstant &&
                        other.cppClangUsr == item.cppClangUsr && other.name == item.name &&
                        other.namespaceName == item.namespaceName && other.declaredTypeKey == item.declaredTypeKey &&
                        other.initializerKind == item.initializerKind && other.integerValue == item.integerValue &&
                        other.cppImportError.empty() && item.cppImportError.empty())
                        candidates.insert(other.identityKey);
                }
            }
            if (candidates.size() == 1) item.cppReconciledStateKey = *candidates.begin();
        }
    }
    output = std::move(live); error.clear(); return true;
}

CppRecordRefinementResult refineCppRecordGroupsForPointerTransport(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroups& groups,
    std::span<const CppProjectSessions> projects)
{
    auto compared = compareCppRecordDefinitions(inventory, groups, projects);
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&compared)) return *failure;
    auto result = groups;
    for (const auto& item : std::get<std::vector<CppRecordStructuralComparison>>(compared))
    {
        auto& group = result.groups[item.groupIndex];
        if (group.status != CppRecordGroupStatus::MultipleDefinitionsUnverified ||
            item.verdict != CppRecordStructuralVerdict::Equivalent) continue;
        const CppRecordDeclaration* first = nullptr;
        bool consistent = true;
        for (const auto index : group.inventoryIndices)
        {
            const auto& record = inventory[index].declaration;
            if (!record.complete) continue;
            if (!record.odrHash || !record.sizeBits || !record.alignmentBits)
            { consistent = false; break; }
            if (!first) first = &record;
            else if (record.odrHash != first->odrHash || record.sizeBits != first->sizeBits ||
                     record.alignmentBits != first->alignmentBits)
            { consistent = false; break; }
        }
        if (consistent && first) group.status = CppRecordGroupStatus::MultipleDefinitionsPointerCompatible;
    }
    // Clang cannot look up the internal names we synthesize for C anonymous
    // aggregates. Compare the captured contract only when every observation
    // has the same known Clang identity and comes from the same parsed file.
    for (auto& group : result.groups)
    {
        if (group.status != CppRecordGroupStatus::MultipleDefinitionsUnverified) continue;
        const CppRecordDeclaration* first = nullptr;
        bool matching = true;
        for (const auto index : group.inventoryIndices)
        {
            const auto& record = inventory[index].declaration;
            if (!record.complete) continue;
            if (record.isCxx || !record.isAnonymousCInternal || !record.clangUsr || record.clangUsr->empty() ||
                !record.sourceProvenance || !record.sizeBits || !record.alignmentBits)
            { matching = false; break; }
            if (!first) { first = &record; continue; }
            std::error_code ec;
            const bool sameFile = std::filesystem::equivalent(first->sourceProvenance->sourcePath,
                record.sourceProvenance->sourcePath, ec);
            if (ec || !sameFile || first->clangUsr != record.clangUsr ||
                first->sourceProvenance->bufferSha256 != record.sourceProvenance->bufferSha256 ||
                first->sourceProvenance->byteOffset != record.sourceProvenance->byteOffset ||
                first->sizeBits != record.sizeBits || first->alignmentBits != record.alignmentBits ||
                !sameCapturedMembers(*first, record))
            { matching = false; break; }
        }
        if (matching && first) group.status = CppRecordGroupStatus::MultipleDefinitionsPointerCompatible;
    }
    return result;
}

CppSignatureRecordResult collectCppSignatureRecordDependencies(
    const compilation::ProjectGraph& graph, std::span<const CppSemanticInventoryEntry> functions,
    const CppRecordGroups& groups)
{
    std::map<std::pair<std::size_t, std::string>, std::size_t> byIdentity;
    for (std::size_t index = 0; index < groups.groups.size(); ++index)
    {
        const auto& group = groups.groups[index];
        if (group.projectOrdinal >= graph.projects().size() || !group.identity.isValid() ||
            group.identity.moduleName() != graph.projects()[group.projectOrdinal].moduleName() ||
            !byIdentity.emplace(std::make_pair(group.projectOrdinal, group.identity.key()), index).second)
            return CppSignatureRecordFailure{"invalid or duplicate record group identity"};
    }
    std::vector<CppSignatureRecordDependency> result;
    for (std::size_t functionIndex = 0; functionIndex < functions.size(); ++functionIndex)
    {
        const auto& function = functions[functionIndex];
        if (function.projectOrdinal >= graph.projects().size())
            return CppSignatureRecordFailure{"function project is not in graph"};
        const auto module = graph.projects()[function.projectOrdinal].moduleName();
        auto collect = [&](const CppType& initial, std::optional<std::size_t> parameter) {
            const CppType* type = &initial;
            std::size_t depth = 0;
            while (type && depth <= 64)
            {
                if (type->kind == CppTypeKind::Record)
                {
                    const auto identity = projectCppRecordTypeIdentity(*type, module);
                    if (!identity)
                        result.push_back({functionIndex, parameter, depth, CppSignatureRecordStatus::Unprojectable, std::nullopt});
                    else
                    {
                        const auto found = byIdentity.find({function.projectOrdinal, identity->key()});
                        if (found == byIdentity.end())
                            result.push_back({functionIndex, parameter, depth, CppSignatureRecordStatus::Missing, std::nullopt});
                        else if (type->recordIdentity && type->recordIdentity->clangUsr &&
                                 groups.groups[found->second].clangUsr &&
                                 type->recordIdentity->clangUsr != groups.groups[found->second].clangUsr)
                            result.push_back({functionIndex, parameter, depth, CppSignatureRecordStatus::Unprojectable, std::nullopt});
                        else
                            result.push_back({functionIndex, parameter, depth, CppSignatureRecordStatus::Matched, found->second});
                    }
                    return true;
                }
                if (type->kind != CppTypeKind::Pointer && type->kind != CppTypeKind::LValueReference &&
                    type->kind != CppTypeKind::RValueReference && type->kind != CppTypeKind::Array)
                    return true;
                type = type->element.get();
                ++depth;
            }
            return false;
        };
        if (!collect(function.declaration.returnType, std::nullopt))
            return CppSignatureRecordFailure{"signature type nesting is incomplete or exceeds limit"};
        for (std::size_t index = 0; index < function.declaration.parameters.size(); ++index)
            if (!collect(function.declaration.parameters[index].type, index))
                return CppSignatureRecordFailure{"signature type nesting is incomplete or exceeds limit"};
    }
    return result;
}

CppRecordObservationResult prepareCppRecordObservation(
    std::span<const CppSemanticRecordEntry> inventory, const CppRecordGroups& groups)
{
    CppRecordObservationInputs result;
    std::vector<bool> claimed(inventory.size());
    for (std::size_t groupIndex = 0; groupIndex < groups.groups.size(); ++groupIndex)
    {
        const auto& group = groups.groups[groupIndex];
        if (group.inventoryIndices.empty() || !group.identity.isValid())
            return CppRecordObservationFailure{"empty or invalid record group"};
        std::string sourcePath;
        std::optional<std::string> knownUsr;
        std::optional<frontend::CppImportedTypeOrigin> knownOrigin;
        std::optional<std::string> knownTarget;
        bool originConflict = false;
        bool targetConflict = false;
        std::optional<frontend::CppImportedObjectLayout> objectLayout;
        bool usrConflict = false;
        bool deferred = !group.identity.containingTypes().empty();
        bool allAnonymousCInternal = true;
        for (const auto entryIndex : group.inventoryIndices)
        {
            if (entryIndex >= inventory.size() || claimed[entryIndex])
                return CppRecordObservationFailure{"invalid or repeated record inventory index"};
            claimed[entryIndex] = true;
            const auto& entry = inventory[entryIndex];
            allAnonymousCInternal &= !entry.declaration.isCxx && entry.declaration.isAnonymousCInternal;
            if (!entry.identity || entry.projectOrdinal != group.projectOrdinal ||
                entry.identity->key() != group.identity.key())
                return CppRecordObservationFailure{"record inventory does not match group identity"};
            const auto origin = entry.declaration.isCxx ? frontend::CppImportedTypeOrigin::CppRecord :
                entry.declaration.kind == CppRecordKind::Union ? frontend::CppImportedTypeOrigin::CUnion :
                frontend::CppImportedTypeOrigin::CStruct;
            if (knownOrigin && *knownOrigin != origin) originConflict = true;
            else knownOrigin = origin;
            if (!entry.targetTriple.empty())
            {
                if (knownTarget && *knownTarget != entry.targetTriple) targetConflict = true;
                else knownTarget = entry.targetTriple;
            }
            else targetConflict = true;
            if (entry.declaration.location && sourcePath.empty())
                sourcePath = utf8SourcePath(entry.declaration.location->file);
            if (entry.declaration.clangUsr && !entry.declaration.clangUsr->empty())
            {
                if (knownUsr && knownUsr != entry.declaration.clangUsr) usrConflict = true;
                else if (!knownUsr) knownUsr = entry.declaration.clangUsr;
            }
            if (group.status == CppRecordGroupStatus::OneDefinition && entry.declaration.complete &&
                entry.declaration.sizeBits && entry.declaration.alignmentBits)
            {
                const auto sizeBits = *entry.declaration.sizeBits;
                const auto alignmentBits = *entry.declaration.alignmentBits;
                if (sizeBits == 0 || alignmentBits == 0 || sizeBits % 8 != 0 || alignmentBits % 8 != 0)
                    return CppRecordObservationFailure{"record dimensions are not representable in bytes"};
                const auto size = sizeBits / 8;
                const auto alignment = alignmentBits / 8;
                if ((alignment & (alignment - 1)) != 0 || size % alignment != 0 || objectLayout)
                    return CppRecordObservationFailure{"record dimensions or definition count are invalid"};
                objectLayout = frontend::CppImportedObjectLayout{size, alignment};
            }
        }
        if (allAnonymousCInternal) deferred = false;
        if (deferred || sourcePath.empty())
        {
            result.deferredGroups.push_back(groupIndex);
            continue;
        }
        // This struct is only a name-lookup placeholder; it does not describe the C++ ABI.
        std::optional<frontend::CppImportedObjectLayout> memberAccessLayout;
        if (group.status == CppRecordGroupStatus::MultipleDefinitionsPointerCompatible)
            if (const auto chosen = selectCppRecordMemberDefinition(inventory, group))
            {
                const auto& record = inventory[*chosen].declaration;
                if (record.sizeBits && record.alignmentBits && *record.sizeBits % 8 == 0 && *record.alignmentBits % 8 == 0)
                    memberAccessLayout = frontend::CppImportedObjectLayout{*record.sizeBits / 8, *record.alignmentBits / 8};
                const bool hasCarrier = !record.isCxx && std::any_of(record.fields.begin(), record.fields.end(),
                    [](const auto& field) { return field.isAnonymousCarrier; });
                bool sameSource = hasCarrier && record.clangUsr && !record.clangUsr->empty() && record.sourceProvenance;
                for (const auto entryIndex : group.inventoryIndices)
                {
                    const auto& other = inventory[entryIndex].declaration;
                    if (!other.complete) continue;
                    if (!sameSource || other.isCxx || other.clangUsr != record.clangUsr || !other.sourceProvenance ||
                        other.sourceProvenance->bufferSha256 != record.sourceProvenance->bufferSha256 ||
                        other.sourceProvenance->byteOffset != record.sourceProvenance->byteOffset ||
                        other.sizeBits != record.sizeBits || other.alignmentBits != record.alignmentBits ||
                        !sameCapturedMembers(record, other)) { sameSource = false; break; }
                    std::error_code ec;
                    if (!std::filesystem::equivalent(record.sourceProvenance->sourcePath,
                            other.sourceProvenance->sourcePath, ec) || ec) { sameSource = false; break; }
                }
                if (sameSource && memberAccessLayout) objectLayout = memberAccessLayout;
                if (!objectLayout && memberAccessLayout && equivalentCRecordsHaveValueLayout(inventory, group, *chosen))
                    objectLayout = memberAccessLayout;
            }
        result.types.push_back({group.identity, frontend::NamedTypeDeclarationKind::Struct,
                                frontend::NamedTypeAccessibility::Public, group.projectOrdinal,
                                std::move(sourcePath), objectLayout, memberAccessLayout});
        result.types.back().isIncomplete = group.status == CppRecordGroupStatus::ForwardOnly;
        if (group.status == CppRecordGroupStatus::OneDefinition ||
            (group.status == CppRecordGroupStatus::MultipleDefinitionsPointerCompatible && objectLayout))
            for (const auto entryIndex : group.inventoryIndices)
            {
                const auto& record = inventory[entryIndex].declaration;
                if (!record.isCxx && record.kind == CppRecordKind::Struct && record.complete &&
                    !record.fields.empty() && record.fields.back().type.kind == CppTypeKind::Array &&
                    record.fields.back().type.isIncompleteArray)
                    result.types.back().hasCFlexibleArrayMember = true;
            }
        if (group.status != CppRecordGroupStatus::Conflict && !originConflict && knownOrigin)
            result.types.back().origin = *knownOrigin;
        if (group.status != CppRecordGroupStatus::Conflict && !targetConflict)
            result.types.back().targetTriple = knownTarget;
        if (group.status != CppRecordGroupStatus::Conflict && !usrConflict) result.types.back().clangUsr = knownUsr;
        result.groupByImportedType.push_back(groupIndex);
    }
    return result;
}

bool provideExperimentalCppRecordTypes(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    {
        error = "C/C++ record collection failed for project '" + failure->projectFile.generic_string() +
            "', reference " + std::to_string(failure->referenceIndex) + ": " + failure->message;
        return false;
    }
    const auto& inventory = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    const auto groups = groupCppSemanticRecords(inventory);
    auto prepared = prepareCppRecordObservation(inventory, groups);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&prepared))
    {
        error = "C/C++ record preparation failed: " + failure->message;
        return false;
    }
    auto& inputs = std::get<CppRecordObservationInputs>(prepared);
    for (const auto index : inputs.groupByImportedType)
        if (groups.groups[index].status != CppRecordGroupStatus::ForwardOnly &&
            groups.groups[index].status != CppRecordGroupStatus::OneDefinition)
        {
            error = "C/C++ record has conflicting or unverified definitions: " + groups.groups[index].identity.key();
            return false;
        }
    output = std::move(inputs.types);
    error.clear();
    return true;
}

bool provideExperimentalCppRecordCandidates(
    const compilation::ProjectGraph& graph, std::span<const CppProjectSessions> projects,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error)
{
    auto collected = collectCppSemanticRecords(graph, projects);
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
    {
        error = failure->message;
        return false;
    }
    const auto& inventory = std::get<std::vector<CppSemanticRecordEntry>>(collected);
    const auto groups = groupCppSemanticRecords(inventory);
    auto prepared = prepareCppRecordObservation(inventory, groups);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&prepared))
    {
        error = failure->message;
        return false;
    }
    output = std::move(std::get<CppRecordObservationInputs>(prepared).types);
    error.clear();
    return true;
}

static CppRecordBindingResult selectCppRecordBindingsWithCatalogs(
    const frontend::ProjectNameLookup& lookup, const CppRecordGroups& groups,
    std::span<const std::size_t> groupByImportedType,
    std::span<const frontend::CppImportedTypeDeclaration> catalogs)
{
    std::vector<std::optional<std::size_t>> groupByOrdinal(lookup.namedTypes().size());
    std::vector<bool> seen(groupByImportedType.size());
    std::unordered_set<std::size_t> seenGroups;
    for (std::size_t ordinal = 0; ordinal < groupByOrdinal.size(); ++ordinal)
    {
        const auto imported = lookup.cppImportedTypeIndex(ordinal);
        if (!imported) continue;
        if (*imported >= groupByImportedType.size())
        {
            const auto* type = lookup.cppImportedType(ordinal);
            const auto offset = *imported - groupByImportedType.size();
            if (offset < catalogs.size())
            {
                if (!type || type->identity.key() != catalogs[offset].identity.key() ||
                    type->projectOrdinal != catalogs[offset].projectOrdinal)
                    return CppRecordBindingFailure{"invalid precompiled C type mapping"};
                continue;
            }
            if (type && type->kind == frontend::NamedTypeDeclarationKind::Enum) continue;
        }
        if (*imported >= groupByImportedType.size() || seen[*imported])
            return CppRecordBindingFailure{"invalid imported type mapping"};
        const auto groupIndex = groupByImportedType[*imported];
        if (groupIndex >= groups.groups.size() || !seenGroups.insert(groupIndex).second)
            return CppRecordBindingFailure{"invalid or duplicate record group"};
        const auto& declaration = lookup.namedTypes()[ordinal];
        const auto& group = groups.groups[groupIndex];
        if (declaration.projectOrdinal != group.projectOrdinal || declaration.identity.key() != group.identity.key())
            return CppRecordBindingFailure{"record group does not match installed type"};
        seen[*imported] = true;
        groupByOrdinal[ordinal] = groupIndex;
    }
    if (std::find(seen.begin(), seen.end(), false) != seen.end())
        return CppRecordBindingFailure{"imported type mapping was not installed"};
    for (const auto groupIndex : groupByImportedType)
    {
        const auto canonical = lookup.typeOrdinalForIdentity(groups.groups[groupIndex].identity.key());
        if (!canonical || *canonical >= groupByOrdinal.size())
            return CppRecordBindingFailure{"canonical record group is not installed"};
        if (groupByOrdinal[*canonical] && *groupByOrdinal[*canonical] != groupIndex)
            return CppRecordBindingFailure{"canonical record maps to multiple live groups"};
        groupByOrdinal[*canonical] = groupIndex;
    }
    CppRecordBindingSelection result;
    auto append = [&](std::vector<std::size_t>& destination, std::size_t ordinal) {
        if (ordinal >= groupByOrdinal.size() || !groupByOrdinal[ordinal]) return;
        destination.push_back(*groupByOrdinal[ordinal]);
    };
    for (const auto& binding : lookup.bindings())
        if (binding.kind == frontend::TypeNameBindingKind::Resolved && binding.targetTypeOrdinal)
            append(result.referencedGroups, *binding.targetTypeOrdinal);
    for (const auto& binding : lookup.expressionTypeBindings())
        for (const auto ordinal : binding.candidateTypeOrdinals)
            append(result.expressionCandidateGroups, ordinal);
    auto unique = [](std::vector<std::size_t>& values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    };
    unique(result.referencedGroups);
    unique(result.expressionCandidateGroups);
    return result;
}

CppRecordBindingResult selectCppRecordBindings(
    const frontend::ProjectNameLookup& lookup, const CppRecordGroups& groups,
    std::span<const std::size_t> groupByImportedType)
{
    return selectCppRecordBindingsWithCatalogs(lookup, groups, groupByImportedType, {});
}

CppRecordObservationResult selectExperimentalCppRecordTypes(const CppRecordObservation& observation)
{
    auto prepared = prepareCppRecordObservation(observation.inventory, observation.groups);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&prepared)) return *failure;
    auto inputs = std::move(std::get<CppRecordObservationInputs>(prepared));
    const auto count = observation.groups.groups.size();
    std::vector<bool> collision(count), demanded(count);
    std::set<std::pair<std::size_t, std::string>> identities;
    for (const auto& group : observation.groups.groups)
        if (!identities.emplace(group.projectOrdinal, group.identity.key()).second)
            return CppRecordObservationFailure{"duplicate record group identity"};
    auto mark = [count](const std::vector<std::size_t>& indices, std::vector<bool>& destination) {
        for (const auto index : indices)
        {
            if (index >= count || destination[index]) return false;
            destination[index] = true;
        }
        return true;
    };
    if (!mark(observation.collidingGroups, collision))
        return CppRecordObservationFailure{"invalid or duplicate colliding record group"};
    std::vector<bool> referencedCollision(count), expressionCollision(count);
    if (!mark(observation.referencedCollidingGroups, referencedCollision) ||
        !mark(observation.expressionCollidingGroups, expressionCollision))
        return CppRecordObservationFailure{"invalid or duplicate observed native collision"};
    for (std::size_t i = 0; i < count; ++i)
        if ((referencedCollision[i] || expressionCollision[i]) && !collision[i])
            return CppRecordObservationFailure{"observed native collision is not a colliding group"};
    std::vector<bool> referenced(count), expression(count);
    if (!mark(observation.selection.referencedGroups, referenced) ||
        !mark(observation.selection.expressionCandidateGroups, expression))
        return CppRecordObservationFailure{"invalid or duplicate observed record group"};
    for (std::size_t i = 0; i < count; ++i) demanded[i] = referenced[i] || expression[i];

    CppRecordObservationInputs selected;
    selected.deferredGroups = std::move(inputs.deferredGroups);
    for (const auto groupIndex : selected.deferredGroups)
        if (groupIndex >= count || demanded[groupIndex])
            return CppRecordObservationFailure{"referenced C/C++ record is not representable for name lookup"};
    for (std::size_t i = 0; i < inputs.types.size(); ++i)
    {
        const auto groupIndex = inputs.groupByImportedType[i];
        if (collision[groupIndex])
        {
            if (demanded[groupIndex])
                return CppRecordObservationFailure{"colliding record cannot be selected as an imported type"};
            continue;
        }
        const auto status = observation.groups.groups[groupIndex].status;
        if (status != CppRecordGroupStatus::ForwardOnly && status != CppRecordGroupStatus::OneDefinition &&
            status != CppRecordGroupStatus::MultipleDefinitionsPointerCompatible)
        {
            if (demanded[groupIndex])
                return CppRecordObservationFailure{"referenced C/C++ record has conflicting or unverified definitions: " +
                                                   observation.groups.groups[groupIndex].identity.key()};
            selected.deferredGroups.push_back(groupIndex);
            continue;
        }
        selected.types.push_back(std::move(inputs.types[i]));
        selected.groupByImportedType.push_back(groupIndex);
    }
    return selected;
}

static bool collectVisibleCRecordReconciliations(
    const frontend::ProjectCompilation& compilation, const frontend::ProjectNameLookup& lookup,
    const CppRecordObservation& observation, std::vector<CppRecordObservation::Reconciliation>& output,
    std::string& error)
{
    const auto* graph = compilation.projectGraph();
    if (!graph) { error = "C record reconciliation requires a project graph"; return false; }
    struct Catalog { std::vector<CppCRecordSnapshot> records; std::vector<CppCEnumSnapshot> enums; };
    std::vector<Catalog> catalogs;
    for (const auto& module : compilation.precompiledModules())
    {
        if (module.cppCTypeCatalog.empty()) continue;
        Catalog catalog;
        if (!metadata_detail::decodeVersionedCTypeCatalog(module.cppCTypeCatalog,
                catalog.records, catalog.enums, error, module.cppCTypeCatalogFormatMinor)) return false;
        catalogs.push_back(std::move(catalog));
    }
    if (catalogs.empty()) { output.clear(); error.clear(); return true; }
    // EN: T08-D26: equivalent declarations of several producers are one type; the live one names the canonical.
    // ES: T08-D26: las declaraciones equivalentes de varios productores son un tipo; la viva nombra la canonica.
    std::map<std::string, std::string> producerTypes;
    if (!metadata_detail::reconcilePrecompiledCTypes(compilation.precompiledModules(), *graph, producerTypes, error))
        return false;
    const auto canonicalMatches = [&](const std::set<std::string>& matches) {
        std::set<std::string> canonical;
        for (const auto& match : matches)
        {
            const auto found = producerTypes.find(match);
            canonical.insert(found == producerTypes.end() ? match : found->second);
        }
        return canonical;
    };
    auto enumInventory = collectCppSemanticEnums(*graph, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&enumInventory))
    { error = failure->message; return false; }
    const auto& enumEntries = std::get<std::vector<CppSemanticEnumEntry>>(enumInventory);
    std::vector<CppRecordObservation::Reconciliation> reconciliations;
    std::map<std::pair<std::size_t, std::string>, std::set<std::string>> enumMatches;
    for (const auto& group : observation.groups.groups)
    {
        if (group.projectOrdinal >= lookup.modules().size() || !lookup.modules()[group.projectOrdinal] ||
            group.inventoryIndices.empty() || group.projectOrdinal >= graph->projects().size()) continue;
        const auto first = group.inventoryIndices.front();
        if (first >= observation.inventory.size() || observation.inventory[first].declaration.isCxx) continue;
        const std::vector<std::string> seeds{group.identity.key()};
        auto captured = collectCppCRecordClosure(observation.inventory, observation.groups.groups,
                                                 group.projectOrdinal, seeds);
        const auto* closure = std::get_if<CppCRecordClosure>(&captured);
        if (!closure) continue;
        auto enumCapture = captureCppCEnumCatalog(enumEntries, group.projectOrdinal,
            graph->projects()[group.projectOrdinal].moduleName(), closure->enumIdentityKeys);
        const auto* liveEnums = std::get_if<std::vector<CppCEnumSnapshot>>(&enumCapture);
        if (!liveEnums) continue;
        const auto visible = lookup.modules()[group.projectOrdinal]->visibleProjectOrdinals();
        std::set<std::string> matches;
        for (const auto& catalog : catalogs)
            for (const auto& candidate : catalog.records)
            {
                if (candidate.name != group.identity.name() || candidate.identityKey == group.identity.key()) continue;
                const auto owner = std::find_if(graph->projects().begin(), graph->projects().end(),
                    [&](const auto& project) { return project.moduleName() == candidate.moduleName; });
                if (owner == graph->projects().end()) continue;
                const auto ownerOrdinal = static_cast<std::size_t>(owner - graph->projects().begin());
                if (std::find(visible.begin(), visible.end(), ownerOrdinal) == visible.end()) continue;
                // EN: T08-D16: the consumer's own forward declaration (`struct Opaque;`) names the record that the
                //     producer published, complete or not: C makes an incomplete type compatible with the record of
                //     the same tag in another translation unit.
                // ES: T08-D16: la declaracion adelantada propia del consumidor (`struct Opaque;`) nombra el record que
                //     publico el productor, completo o no: C hace compatible un tipo incompleto con el record de la
                //     misma etiqueta en otra unidad de traduccion.
                const auto live = std::find_if(closure->records.begin(), closure->records.end(),
                    [&](const auto& record) { return record.identityKey == group.identity.key(); });
                if (live != closure->records.end() && !live->complete && candidate.complete &&
                    live->kind == candidate.kind && live->targetTriple == candidate.targetTriple &&
                    live->namespaceSegments == candidate.namespaceSegments &&
                    live->containingRecords == candidate.containingRecords && live->clangUsr &&
                    !live->clangUsr->empty() && live->clangUsr == candidate.clangUsr)
                {
                    matches.insert(candidate.identityKey);
                    continue;
                }
                metadata_detail::CRecordContractComparison verdict{};
                if (!metadata_detail::compareCRecordClosureEvidence(closure->records, *liveEnums,
                        group.identity.key(), catalog.records, catalog.enums, candidate.identityKey,
                        verdict, error)) return false;
                if (verdict == metadata_detail::CRecordContractComparison::Matching)
                    matches.insert(candidate.identityKey);
            }
        matches = canonicalMatches(matches);
        if (matches.size() == 1)
        {
            const auto& matchedRecord = *matches.begin();
            reconciliations.push_back({group.projectOrdinal, group.identity.key(), matchedRecord});
            const auto separator = matchedRecord.find("::");
            const std::string prefix = group.identity.moduleName() + "::";
            if (separator != std::string::npos)
                for (const auto& enumIdentity : closure->enumIdentityKeys)
                    if (enumIdentity.starts_with(prefix))
                        enumMatches[{group.projectOrdinal, enumIdentity}].insert(
                            matchedRecord.substr(0, separator) + enumIdentity.substr(prefix.size() - 2));
        }
    }
    std::set<std::pair<std::size_t, std::string>> observedEnums;
    for (const auto& entry : enumEntries)
    {
        if (entry.declaration.isCxx || entry.declaration.name.empty() ||
            entry.projectOrdinal >= graph->projects().size() ||
            entry.projectOrdinal >= lookup.modules().size() || !lookup.modules()[entry.projectOrdinal] ||
            !observedEnums.emplace(entry.projectOrdinal, entry.declaration.name).second) continue;
        const auto& project = graph->projects()[entry.projectOrdinal];
        const compilation::TypeIdentity identity(project.moduleName(), {}, {}, entry.declaration.name);
        if (!identity.isValid()) continue;
        const std::vector<std::string> requested{identity.key()};
        const auto captured = captureCppCEnumCatalog(enumEntries, entry.projectOrdinal, project.moduleName(), requested);
        const auto* live = std::get_if<std::vector<CppCEnumSnapshot>>(&captured);
        if (!live || live->size() != 1 || !live->front().declaration.clangUsr ||
            live->front().declaration.clangUsr->empty()) continue;
        const auto visible = lookup.modules()[entry.projectOrdinal]->visibleProjectOrdinals();
        for (const auto& catalog : catalogs)
            for (const auto& candidate : catalog.enums)
            {
                if (candidate.identityKey == identity.key() || candidate.declaration.name != entry.declaration.name ||
                    candidate.declaration.clangUsr != live->front().declaration.clangUsr) continue;
                const auto separator = candidate.identityKey.find("::");
                if (separator == std::string::npos) continue;
                const auto owner = std::find_if(graph->projects().begin(), graph->projects().end(), [&](const auto& value) {
                    return value.moduleName() == candidate.identityKey.substr(0, separator);
                });
                if (owner == graph->projects().end() || std::find(visible.begin(), visible.end(),
                        static_cast<std::size_t>(owner - graph->projects().begin())) == visible.end()) continue;
                metadata_detail::CRecordContractComparison verdict{};
                if (!metadata_detail::compareCEnumSourceEvidence(live->front(), candidate, verdict, error, true)) return false;
                if (verdict == metadata_detail::CRecordContractComparison::Matching)
                    enumMatches[{entry.projectOrdinal, identity.key()}].insert(candidate.identityKey);
            }
    }
    for (const auto& [source, candidates] : enumMatches)
    {
        const auto matches = canonicalMatches(candidates);
        if (matches.size() == 1 && source.second != *matches.begin())
            reconciliations.push_back({source.first, source.second, *matches.begin()});
    }
    output = std::move(reconciliations); error.clear(); return true;
}

CppRecordObservationRunResult observeCppRecords(const frontend::ProjectCompilation& compilation)
{
    const auto* graph = compilation.projectGraph();
    if (!graph) return CppRecordObservationFailure{"project syntax has not been prepared"};
    auto collected = collectCppSemanticRecords(*graph, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppSemanticRecordFailure>(&collected))
        return CppRecordObservationFailure{failure->message};
    CppRecordObservation result;
    result.inventory = std::move(std::get<std::vector<CppSemanticRecordEntry>>(collected));
    result.groups = groupCppSemanticRecords(result.inventory);
    auto refined = refineCppRecordGroupsForPointerTransport(result.inventory, result.groups, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppRecordStructuralComparisonFailure>(&refined))
        return CppRecordObservationFailure{failure->message};
    result.groups = std::move(std::get<CppRecordGroups>(refined));
    auto prepared = prepareCppRecordObservation(result.inventory, result.groups);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&prepared)) return *failure;
    auto inputs = std::move(std::get<CppRecordObservationInputs>(prepared));
    result.deferredGroups = inputs.deferredGroups;
    auto namespaceInventory = collectCppNamespaces(*graph, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&namespaceInventory))
        return CppRecordObservationFailure{failure->message};
    auto visible = visibleCppNamespaces(std::get<std::vector<CppNamespaceInventoryEntry>>(namespaceInventory));
    if (const auto* failure = std::get_if<CppNamespaceInventoryFailure>(&visible))
        return CppRecordObservationFailure{failure->message};
    diagnostics::DiagnosticEngine diagnostics;
    std::vector<std::unique_ptr<frontend::ProjectSyntaxUnit>> units;
    std::string error;
    if (!compilation.cloneSyntaxForObservation(units, diagnostics, error))
        return CppRecordObservationFailure{error};
    auto copySnapshots = [&](std::vector<frontend::PrecompiledSnapshotUnit>& snapshots) {
        for (const auto& original : compilation.precompiledSnapshots())
        {
            if (original.unitOrdinal >= units.size() || !units[original.unitOrdinal] ||
                units[original.unitOrdinal]->fileId() != original.fileId)
                return false;
            auto copy = original;
            copy.index.build(units[original.unitOrdinal]->syntaxTree());
            if (copy.index.nodes().size() != original.index.nodes().size() ||
                copy.index.types().size() != original.index.types().size() ||
                copy.index.kindDigest() != original.index.kindDigest())
                return false;
            snapshots.push_back(std::move(copy));
        }
        return true;
    };
    std::vector<frontend::PrecompiledSnapshotUnit> nativeSnapshots;
    if (!copySnapshots(nativeSnapshots))
        return CppRecordObservationFailure{"precompiled snapshot does not match observation syntax"};
    const auto& namespaces = std::get<std::vector<std::pair<std::size_t, std::string>>>(visible);
    std::vector<frontend::CppImportedTypeDeclaration> precompiledTypes;
    if (!metadata_detail::reconstructPrecompiledCImportedTypes(compilation.precompiledModules(), *graph,
            "x86_64-pc-windows-msvc", precompiledTypes, error))
        return CppRecordObservationFailure{std::move(error)};
    frontend::ProjectNameLookup nativeLookup;
    nativeLookup.setPrecompiledSnapshots(nativeSnapshots);
    if (!nativeLookup.setImportedNamespaces(namespaces) || !nativeLookup.setCppImportedTypes(precompiledTypes))
        return CppRecordObservationFailure{"cannot configure native observation lookup"};
    nativeLookup.build(*graph, units, diagnostics);
    if (!nativeLookup.hasTypeIndex())
        return CppRecordObservationFailure{"cannot index NativeSharp declarations for observation"};
    if (!collectVisibleCRecordReconciliations(compilation, nativeLookup, result, result.reconciliations, error))
        return CppRecordObservationFailure{std::move(error)};
    std::vector<frontend::CppImportedTypeDeclaration> filteredTypes;
    std::vector<std::size_t> filteredGroups;
    for (std::size_t index = 0; index < inputs.types.size(); ++index)
    {
        const auto& imported = inputs.types[index];
        const auto nativeOrdinal = nativeLookup.typeOrdinalForIdentity(imported.identity.key());
        const auto* native = nativeOrdinal ? nativeLookup.typeForOrdinal(*nativeOrdinal) : nullptr;
        if (nativeOrdinal && nativeLookup.cppImportedType(*nativeOrdinal))
            return CppRecordObservationFailure{"live and precompiled C/C++ type identity requires reconciliation: " + imported.identity.key()};
        if (native && native->projectOrdinal == imported.projectOrdinal)
            result.collidingGroups.push_back(inputs.groupByImportedType[index]);
        else
        {
            auto projected = imported;
            const auto reconciled = std::find_if(result.reconciliations.begin(), result.reconciliations.end(),
                [&](const auto& item) { return item.projectOrdinal == imported.projectOrdinal &&
                                              item.liveIdentityKey == imported.identity.key(); });
            if (reconciled != result.reconciliations.end())
                projected.reconciledIdentityKey = reconciled->precompiledIdentityKey;
            filteredTypes.push_back(std::move(projected));
            filteredGroups.push_back(inputs.groupByImportedType[index]);
        }
    }
    units.clear();
    diagnostics::DiagnosticEngine finalDiagnostics;
    if (!compilation.cloneSyntaxForObservation(units, finalDiagnostics, error))
        return CppRecordObservationFailure{error};
    std::vector<frontend::PrecompiledSnapshotUnit> finalSnapshots;
    if (!copySnapshots(finalSnapshots))
        return CppRecordObservationFailure{"precompiled snapshot does not match observation syntax"};
    frontend::ProjectNameLookup lookup;
    lookup.setPrecompiledSnapshots(finalSnapshots);
    std::vector<frontend::CppImportedTypeAlias> aliases;
    if (!provideCppTypeAliases(*graph, compilation.cppProjectSessions(), aliases, error))
        return CppRecordObservationFailure{std::move(error)};
    std::vector<frontend::CppImportedTypeDeclaration> enums;
    if (!provideCppEnumTypes(*graph, compilation.cppProjectSessions(), enums, error))
        return CppRecordObservationFailure{std::move(error)};
    filteredTypes.insert(filteredTypes.end(), precompiledTypes.begin(), precompiledTypes.end());
    std::set<std::string> collidingEnumIdentities;
    for (auto& enumeration : enums)
    {
        const auto reconciled = std::find_if(result.reconciliations.begin(), result.reconciliations.end(),
            [&](const auto& item) { return item.projectOrdinal == enumeration.projectOrdinal &&
                                          item.liveIdentityKey == enumeration.identity.key(); });
        if (reconciled != result.reconciliations.end())
            enumeration.reconciledIdentityKey = reconciled->precompiledIdentityKey;
        const auto ordinal = nativeLookup.typeOrdinalForIdentity(enumeration.identity.key());
        const auto* native = ordinal ? nativeLookup.typeForOrdinal(*ordinal) : nullptr;
        if (ordinal && nativeLookup.cppImportedType(*ordinal))
            return CppRecordObservationFailure{"live and precompiled C/C++ type identity requires reconciliation: " + enumeration.identity.key()};
        const bool collidesWithRecord = std::any_of(filteredTypes.begin(), filteredTypes.end(),
            [&](const auto& record) { return record.identity.key() == enumeration.identity.key(); });
        if (native || collidesWithRecord) collidingEnumIdentities.insert(enumeration.identity.key());
        else filteredTypes.push_back(std::move(enumeration));
    }
    result.collidingEnumIdentities.assign(collidingEnumIdentities.begin(), collidingEnumIdentities.end());
    if (!lookup.setImportedNamespaces(std::move(std::get<std::vector<std::pair<std::size_t, std::string>>>(visible))) ||
        !lookup.setCppImportedTypes(std::move(filteredTypes)) ||
        !lookup.setCppImportedTypeAliases(std::move(aliases)))
        return CppRecordObservationFailure{"cannot configure observation lookup"};
    if (!lookup.build(*graph, units, finalDiagnostics))
    {
        error = "record observation name lookup failed";
        if (!finalDiagnostics.diagnostics().empty()) error += ": " + finalDiagnostics.diagnostics().front().message();
        return CppRecordObservationFailure{std::move(error)};
    }
    const auto enumCollision = [&](std::size_t ordinal) {
        return ordinal < lookup.namedTypes().size() &&
            collidingEnumIdentities.contains(lookup.namedTypes()[ordinal].identity.key());
    };
    for (const auto& binding : lookup.bindings())
        if (binding.targetTypeOrdinal && enumCollision(*binding.targetTypeOrdinal))
            return CppRecordObservationFailure{"C enum conflicts with another used type: " + binding.writtenName};
    for (const auto& binding : lookup.expressionTypeBindings())
        for (const auto ordinal : binding.candidateTypeOrdinals)
            if (enumCollision(ordinal))
                return CppRecordObservationFailure{"C enum conflicts with another used expression type"};
    std::unordered_map<std::size_t, std::size_t> collisionByOrdinal;
    for (const auto groupIndex : result.collidingGroups)
    {
        const auto ordinal = lookup.typeOrdinalForIdentity(result.groups.groups[groupIndex].identity.key());
        if (!ordinal || !collisionByOrdinal.emplace(*ordinal, groupIndex).second)
            return CppRecordObservationFailure{"colliding NativeSharp type is missing or maps to multiple groups"};
    }
    for (const auto& binding : lookup.bindings())
        if (binding.kind == frontend::TypeNameBindingKind::Resolved && binding.targetTypeOrdinal)
            if (const auto found = collisionByOrdinal.find(*binding.targetTypeOrdinal); found != collisionByOrdinal.end())
                result.referencedCollidingGroups.push_back(found->second);
    for (const auto& binding : lookup.expressionTypeBindings())
        for (const auto ordinal : binding.candidateTypeOrdinals)
            if (const auto found = collisionByOrdinal.find(ordinal); found != collisionByOrdinal.end())
                result.expressionCollidingGroups.push_back(found->second);
    auto deduplicate = [](std::vector<std::size_t>& values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    };
    deduplicate(result.referencedCollidingGroups);
    deduplicate(result.expressionCollidingGroups);
    auto selected = selectCppRecordBindingsWithCatalogs(lookup, result.groups, filteredGroups, precompiledTypes);
    if (const auto* failure = std::get_if<CppRecordBindingFailure>(&selected))
        return CppRecordObservationFailure{failure->message};
    result.selection = std::move(std::get<CppRecordBindingSelection>(selected));
    std::set<std::string> enumIdentities;
    const auto addEnum = [&](std::size_t ordinal) {
        const auto* type = lookup.cppImportedType(ordinal);
        const auto index = lookup.cppImportedTypeIndex(ordinal);
        if (index && *index >= filteredGroups.size() && *index < filteredGroups.size() + precompiledTypes.size()) return;
        if (type && type->kind == frontend::NamedTypeDeclarationKind::Enum &&
            enumIdentities.insert(type->identity.key()).second)
            result.referencedEnumTypes.push_back(*type);
    };
    for (const auto& binding : lookup.bindings())
        if (binding.targetTypeOrdinal) addEnum(*binding.targetTypeOrdinal);
    for (const auto& binding : lookup.expressionTypeBindings())
        for (const auto ordinal : binding.candidateTypeOrdinals) addEnum(ordinal);
    std::vector<frontend::CppImportedTypeDeclaration> signatureEnums;
    if (!provideCppEnumTypes(*graph, compilation.cppProjectSessions(), signatureEnums, error))
        return CppRecordObservationFailure{std::move(error)};
    auto functionInventory = collectCppSemanticFunctions(*graph, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&functionInventory))
        return CppRecordObservationFailure{failure->message};
    const auto& functions = std::get<std::vector<CppSemanticInventoryEntry>>(functionInventory);
    auto dependencies = collectCppSignatureEnumDependencies(*graph, functions, signatureEnums);
    if (const auto* failure = std::get_if<CppSignatureEnumFailure>(&dependencies))
        return CppRecordObservationFailure{failure->message};
    const auto& enumDependencies = std::get<std::vector<CppSignatureEnumDependency>>(dependencies);
    auto globalInventory = collectCppSemanticGlobals(*graph, compilation.cppProjectSessions());
    if (const auto* failure = std::get_if<CppSemanticInventoryFailure>(&globalInventory))
        return CppRecordObservationFailure{failure->message};
    const auto& globals = std::get<std::vector<CppSemanticGlobalEntry>>(globalInventory);
    const auto globalEnum = [](const CppType& root) -> const CppType* {
        const CppType* type = &root;
        std::size_t depth = 0;
        while (type && (type->kind == CppTypeKind::Pointer || type->kind == CppTypeKind::Array ||
                        type->kind == CppTypeKind::Atomic) && depth++ < 64)
            type = type->element.get();
        return type && type->kind == CppTypeKind::Enumeration ? type : nullptr;
    };
    const bool hasEnumGlobals = std::any_of(globals.begin(), globals.end(), [&](const auto& entry) {
        return (entry.declaration.externalLinkage || entry.cLanguage) && globalEnum(entry.declaration.type);
    });
    const bool hasCppEnums = std::any_of(signatureEnums.begin(), signatureEnums.end(), [](const auto& type) {
        return type.origin == frontend::CppImportedTypeOrigin::CppEnum;
    });
    if (!enumDependencies.empty() || hasEnumGlobals || hasCppEnums)
    {
        frontend::ProjectMemberModel members;
        members.setPrecompiledSnapshots(finalSnapshots);
        diagnostics::DiagnosticEngine demandDiagnostics;
        if (!members.build(lookup, units, demandDiagnostics, compilation.genericModel()))
            return CppRecordObservationFailure{"cannot build preliminary members for C enum function demands"};
        frontend::ProjectNamespaceFunctionModel demands;
        demands.setCaptureCppFunctionDemands(true);
        demands.setPrecompiledSnapshots(finalSnapshots);
        if (!demands.build(lookup, members, units, demandDiagnostics, compilation.genericModel()))
            return CppRecordObservationFailure{"cannot observe C enum function demands"};
        if (hasCppEnums)
        {
            std::vector<frontend::NamespaceStateSymbol> enumValues;
            if (!provideCppSemanticEnumConstants(*graph, compilation.cppProjectSessions(),
                    demands.cppFunctionDemands(), enumValues, error))
                return CppRecordObservationFailure{std::move(error)};
            for (const auto& value : enumValues)
            {
                if (!value.isCppEnumConstant || !value.declaredTypeKey.starts_with("type:")) continue;
                const auto candidate = std::find_if(signatureEnums.begin(), signatureEnums.end(), [&](const auto& type) {
                    return type.origin == frontend::CppImportedTypeOrigin::CppEnum &&
                        type.projectOrdinal == value.projectOrdinal && "type:" + type.identity.key() == value.declaredTypeKey;
                });
                if (candidate == signatureEnums.end() || !candidate->deferredImportError.empty() ||
                    collidingEnumIdentities.contains(candidate->identity.key())) continue;
                if (enumIdentities.insert(candidate->identity.key()).second)
                    result.referencedEnumTypes.push_back(*candidate);
            }
        }
        auto selections = selectCppSemanticDemands(functions, demands.cppFunctionDemands(), true);
        if (const auto* failure = std::get_if<CppSemanticDemandFailure>(&selections))
            return CppRecordObservationFailure{failure->conflict.message};
        auto selectedEnums = selectCppDemandedEnumCandidates(enumDependencies,
            std::get<std::vector<CppSemanticDemandCandidates>>(selections), functions.size(), signatureEnums.size());
        if (const auto* failure = std::get_if<CppSignatureEnumFailure>(&selectedEnums))
            return CppRecordObservationFailure{failure->message};
        for (const auto index : std::get<std::vector<std::size_t>>(selectedEnums))
        {
            const auto& type = signatureEnums[index];
            if (!type.deferredImportError.empty()) return CppRecordObservationFailure{type.deferredImportError};
            if (collidingEnumIdentities.contains(type.identity.key()))
                return CppRecordObservationFailure{"C enum function signature conflicts with another type: " + type.identity.name()};
            if (enumIdentities.insert(type.identity.key()).second) result.referencedEnumTypes.push_back(type);
        }
        for (const auto& demand : demands.cppFunctionDemands())
            for (const auto owner : demand.visibleProjects)
                for (const auto& ns : demand.lookupNamespaces)
                {
                    if (owner >= graph->projects().size() || demand.lookupName.empty()) continue;
                    std::string qualified;
                    for (const char c : ns) qualified += c == '.' ? "::" : std::string(1, c);
                    if (!qualified.empty()) qualified += "::";
                    qualified += demand.lookupName;
                    for (const auto& entry : globals)
                    {
                        const auto& global = entry.declaration;
                        const auto* enumType = globalEnum(global.type);
                        if (entry.projectOrdinal != owner || (!global.externalLinkage && !entry.cLanguage) ||
                            global.qualifiedName != qualified || !enumType ||
                            !enumType->enumIdentity || enumType->enumIdentity->anonymousContext ||
                            enumType->enumIdentity->name.empty() ||
                            enumType->enumIdentity->qualifiedName != enumType->enumIdentity->name) continue;
                        const compilation::TypeIdentity identity(graph->projects()[owner].moduleName(), {}, {},
                                                                 enumType->enumIdentity->name);
                        const auto candidate = std::find_if(signatureEnums.begin(), signatureEnums.end(), [&](const auto& type) {
                            return type.projectOrdinal == owner && type.identity.key() == identity.key();
                        });
                        if (candidate == signatureEnums.end()) continue;
                        // A captured name may resolve to a NativeSharp local or parameter. Do not
                        // diagnose an unused imported global during this speculative observation.
                        if (!candidate->deferredImportError.empty() ||
                            collidingEnumIdentities.contains(candidate->identity.key())) continue;
                        if (enumIdentities.insert(candidate->identity.key()).second)
                            result.referencedEnumTypes.push_back(*candidate);
                    }
                }
    }
    return result;
}

bool selectExperimentalCppRecordTypesForCompilation(
    const frontend::ProjectCompilation& compilation,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error)
{
    auto observed = observeCppRecords(compilation);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&observed))
    {
        error = failure->message;
        return false;
    }
    auto selected = selectExperimentalCppRecordTypes(std::get<CppRecordObservation>(observed));
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&selected))
    {
        error = failure->message;
        return false;
    }
    output = std::move(std::get<CppRecordObservationInputs>(selected).types);
    const auto& observation = std::get<CppRecordObservation>(observed);
    for (auto& type : output)
    {
        const auto reconciled = std::find_if(observation.reconciliations.begin(), observation.reconciliations.end(),
            [&](const auto& item) { return item.projectOrdinal == type.projectOrdinal &&
                                          item.liveIdentityKey == type.identity.key(); });
        if (reconciled != observation.reconciliations.end())
            type.reconciledIdentityKey = reconciled->precompiledIdentityKey;
        if (type.origin != frontend::CppImportedTypeOrigin::CppRecord || !type.objectLayout ||
            !type.deferredImportError.empty() || type.fromPrecompiledCatalog) continue;
        const auto group = std::find_if(observation.groups.groups.begin(), observation.groups.groups.end(),
            [&](const auto& item) { return item.projectOrdinal == type.projectOrdinal &&
                                           item.identity.key() == type.identity.key(); });
        if (group == observation.groups.groups.end() || group->status != CppRecordGroupStatus::OneDefinition)
            continue;
        const auto selected = selectCppRecordMemberDefinition(observation.inventory, *group);
        if (!selected || *selected >= observation.inventory.size()) continue;
        const auto value = observeCppRecordValueForEntry(observation.inventory[*selected], compilation.cppProjectSessions());
        if (!value) continue;
        type.cppValueTraits = std::make_shared<const CppRecordValueTraits>(value->traits);
        type.cppValueOperations = std::make_shared<const CppRecordValueOperations>(value->operations);
    }
    std::vector<frontend::CppImportedTypeDeclaration> enumCandidates;
    const auto* graph = compilation.projectGraph();
    if (!graph) { error = "project graph is unavailable for C enum dependencies"; return false; }
    if (!provideCppEnumTypes(*graph, compilation.cppProjectSessions(), enumCandidates, error)) return false;
    std::set<std::string> selectedIdentities;
    for (const auto& type : output) selectedIdentities.insert(type.identity.key());
    auto allPrepared = prepareCppRecordObservation(observation.inventory, observation.groups);
    if (const auto* failure = std::get_if<CppRecordObservationFailure>(&allPrepared))
    { error = failure->message; return false; }
    const auto& candidates = std::get<CppRecordObservationInputs>(allPrepared);
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const auto& group : observation.groups.groups)
        {
            if (!selectedIdentities.contains(group.identity.key())) continue;
            const auto definition = selectCppRecordMemberDefinition(observation.inventory, group);
            if (!definition || *definition >= observation.inventory.size()) continue;
            const auto& record = observation.inventory[*definition].declaration;
            if (record.isCxx) continue;
            for (const auto& field : record.fields)
            {
                const CppType* element = &field.type;
                for (std::size_t depth = 0; element && depth < 64 &&
                     (element->kind == CppTypeKind::Pointer || element->kind == CppTypeKind::Array); ++depth)
                    element = element->element.get();
                if (!element || element->kind != CppTypeKind::Record) continue;
                const auto identity = projectCppRecordTypeIdentity(*element, group.identity.moduleName());
                if (!identity) continue;
                if (selectedIdentities.contains(identity->key()))
                {
                    const auto selected = std::find_if(output.begin(), output.end(), [&](const auto& item) {
                        return item.projectOrdinal == group.projectOrdinal && item.identity.key() == identity->key();
                    });
                    if (selected != output.end() && element->recordIdentity && element->recordIdentity->clangUsr &&
                        selected->clangUsr && element->recordIdentity->clangUsr != selected->clangUsr)
                    { error = "C field record dependency has a different Clang USR: " + identity->key(); return false; }
                    continue;
                }
                const auto target = std::find_if(candidates.types.begin(), candidates.types.end(), [&](const auto& item) {
                    return item.projectOrdinal == group.projectOrdinal && item.identity.key() == identity->key();
                });
                if (target == candidates.types.end()) { error = "C field record dependency is unavailable: " + identity->key(); return false; }
                if (element->recordIdentity && element->recordIdentity->clangUsr && target->clangUsr &&
                    element->recordIdentity->clangUsr != target->clangUsr)
                { error = "C field record dependency has a different Clang USR: " + identity->key(); return false; }
                const auto index = static_cast<std::size_t>(target - candidates.types.begin());
                const auto groupIndex = candidates.groupByImportedType[index];
                const auto status = observation.groups.groups[groupIndex].status;
                if (std::find(observation.collidingGroups.begin(), observation.collidingGroups.end(), groupIndex) != observation.collidingGroups.end() ||
                    (status != CppRecordGroupStatus::ForwardOnly && status != CppRecordGroupStatus::OneDefinition &&
                     status != CppRecordGroupStatus::MultipleDefinitionsPointerCompatible))
                { error = "C field record dependency conflicts or is unverified: " + identity->key(); return false; }
                selectedIdentities.insert(identity->key());
                output.push_back(*target);
                changed = true;
            }
        }
    }
    const auto selectedRecordIdentities = selectedIdentities;
    const auto addEnum = [&](const frontend::CppImportedTypeDeclaration& type) {
        if (selectedIdentities.insert(type.identity.key()).second) output.push_back(type);
    };
    for (const auto& enumeration : observation.referencedEnumTypes) addEnum(enumeration);
    for (const auto& reconciliation : observation.reconciliations)
    {
        const auto candidate = std::find_if(enumCandidates.begin(), enumCandidates.end(), [&](const auto& item) {
            return item.projectOrdinal == reconciliation.projectOrdinal && item.identity.key() == reconciliation.liveIdentityKey;
        });
        if (candidate == enumCandidates.end()) continue;
        if (!candidate->deferredImportError.empty()) { error = candidate->deferredImportError; return false; }
        if (std::find(observation.collidingEnumIdentities.begin(), observation.collidingEnumIdentities.end(),
                      candidate->identity.key()) != observation.collidingEnumIdentities.end())
        { error = "reconciled C enum conflicts with another type: " + candidate->identity.name(); return false; }
        if (selectedRecordIdentities.contains(candidate->identity.key()))
        { error = "reconciled C enum conflicts with a selected record: " + candidate->identity.name(); return false; }
        addEnum(*candidate);
    }
    for (const auto& group : observation.groups.groups)
    {
        if (!selectedIdentities.contains(group.identity.key())) continue;
        const auto definition = selectCppRecordMemberDefinition(observation.inventory, group);
        if (!definition || *definition >= observation.inventory.size()) continue;
        const auto& record = observation.inventory[*definition].declaration;
        if (record.isCxx)
        {
            const auto owner = std::find_if(output.begin(), output.end(), [&](const auto& item) {
                return item.projectOrdinal == group.projectOrdinal && item.identity.key() == group.identity.key();
            });
            if (owner == output.end() || owner->origin != frontend::CppImportedTypeOrigin::CppRecord ||
                !owner->cppValueTraits || !owner->cppValueOperations ||
                !cppRecordValueOperationsEligible(*owner->cppValueTraits, *owner->cppValueOperations)) continue;
        }
        for (const auto& field : record.fields)
        {
            const CppType* type = &field.type;
            if (record.isCxx && (type->kind != CppTypeKind::Enumeration || type->isVolatile)) continue;
            for (std::size_t depth = 0; type && depth < 64 &&
                 (type->kind == CppTypeKind::Pointer || type->kind == CppTypeKind::Array); ++depth)
                type = type->element.get();
            if (!type || type->kind != CppTypeKind::Enumeration || !type->enumIdentity ||
                type->enumIdentity->anonymousContext) continue;
            const auto identity = record.isCxx ? projectCppEnumTypeIdentity(*type, group.identity.moduleName()) :
                std::optional<compilation::TypeIdentity>{compilation::TypeIdentity(
                    group.identity.moduleName(), {}, {}, type->enumIdentity->name)};
            if (!identity) continue;
            const auto candidate = std::find_if(enumCandidates.begin(), enumCandidates.end(), [&](const auto& item) {
                return item.projectOrdinal == group.projectOrdinal && item.identity.key() == identity->key();
            });
            if (candidate == enumCandidates.end()) continue;
            if (record.isCxx && (candidate->origin != frontend::CppImportedTypeOrigin::CppEnum ||
                !type->enumIdentity->clangUsr || type->enumIdentity->clangUsr != candidate->clangUsr)) continue;
            if (type->enumIdentity->clangUsr && candidate->clangUsr && type->enumIdentity->clangUsr != candidate->clangUsr)
            { error = "C enum field dependency has a different Clang USR: " + candidate->identity.name(); return false; }
            if (!candidate->deferredImportError.empty()) { error = candidate->deferredImportError; return false; }
            if (std::find(observation.collidingEnumIdentities.begin(), observation.collidingEnumIdentities.end(),
                          candidate->identity.key()) != observation.collidingEnumIdentities.end())
            { error = "C enum field conflicts with another type: " + candidate->identity.name(); return false; }
            if (selectedRecordIdentities.contains(candidate->identity.key()))
            { error = "C enum field conflicts with a selected record: " + candidate->identity.name(); return false; }
            addEnum(*candidate);
        }
    }
    for (auto& type : output)
    {
        const auto reconciled = std::find_if(observation.reconciliations.begin(), observation.reconciliations.end(),
            [&](const auto& item) { return item.projectOrdinal == type.projectOrdinal &&
                                          item.liveIdentityKey == type.identity.key(); });
        if (reconciled != observation.reconciliations.end())
            type.reconciledIdentityKey = reconciled->precompiledIdentityKey;
    }
    error.clear();
    return true;
}
}
