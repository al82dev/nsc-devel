#pragma once

#include "nativesharp/Interop/CppSemanticRecord.h"

#include <algorithm>
#include <optional>
#include <limits>
#include <vector>
#include <functional>
#include <string>
#include <string_view>

namespace nativesharp::interop
{
using CppReplayRecordResolver = std::function<std::optional<std::string>(const CppType&)>;
// EN: T09-003. `const` and `volatile` do not change how Clang classifies a field; the replay writes them
//     back so that the reconstructed field type is the observed one, qualifiers included.
// ES: T09-003. `const` y `volatile` no cambian como Clang clasifica un campo; el replay los reescribe para
//     que el tipo reconstruido sea el observado, con sus calificadores.
inline std::string cppReplayQualifiers(const CppType& type)
{
    return std::string(type.isConst ? "const " : "") + (type.isVolatile ? "volatile " : "");
}
// EN: Spelling of a type without its const/volatile prefix; empty when the prefix does not match the
//     flags or the type is restrict-qualified, so that no comparison can succeed.
// ES: Grafia de un tipo sin su prefijo const/volatile; vacia si el prefijo no coincide con los
//     indicadores o el tipo es restrict, para que ninguna comparacion pueda tener exito.
inline std::string_view cppReplayUnqualifiedSpelling(const CppType& type)
{
    const auto qualifiers = cppReplayQualifiers(type);
    const std::string_view spelling(type.canonicalSpelling);
    return !type.isRestrict && spelling.starts_with(qualifiers) ? spelling.substr(qualifiers.size()) : std::string_view{};
}
// Preliminary reconstruction for asking Clang about a persisted C record.
// This is not an ABI proof: the caller must also establish that all attributes
// affecting parameter passing were captured and bind the resulting plan to
// the original function and type identities.
inline std::optional<std::string> makeSimpleCRecordAbiReplay(const CppCRecordSnapshot& record,
                                                            std::size_t index = 0,
                                                            const CppReplayRecordResolver& resolve = {})
{
    if (!record.complete || !record.valueLayoutUsable || !record.sizeBits || !record.alignmentBits ||
        !record.targetTriple.starts_with("x86_64-pc-windows-msvc") || record.fields.empty() ||
        record.fields.size() > 256 || (record.plainReplaySyntax && record.packedReplaySyntax) ||
        (record.packedReplaySyntax && *record.alignmentBits != 8) ||
        (record.kind != CppRecordKind::Struct && record.kind != CppRecordKind::Union)) return std::nullopt;
    const auto tag = index == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(index);
    std::string result = std::string(record.kind == CppRecordKind::Struct ? "struct " : "union ") +
        (record.packedReplaySyntax ? "__attribute__((packed)) " : "") + tag + " {\n";
    // EN: T09-003. A plain record may carry alignment attributes on its fields or on itself, which the producer
    //     checked to be the only attributes. They are not stored, so the replay rebuilds the smallest
    //     alignment that yields each observed offset and the record alignment; the caller still compares the
    //     whole replayed layout with the catalog. Records with bitfields, packed records and byte-aligned
    //     records (a packed record reaches the producer guard before it is flagged) are not inferred.
    // ES: T09-003. Un record plano puede llevar atributos de alineacion en sus campos o en si mismo, que el
    //     productor comprobo que son los unicos. No se guardan, asi que el replay reconstruye la menor
    //     alineacion que da cada desplazamiento observado y la alineacion del record; quien llama sigue
    //     comparando todo el layout reproducido con el catalogo. No se infiere con campos de bits ni empaquetado, ni
    //     con alineacion de un byte (un record empaquetado aun sin marcar llega asi desde el productor).
    const bool inferAlignment = !record.packedReplaySyntax && *record.alignmentBits > 8 && std::none_of(record.fields.begin(), record.fields.end(),
        [](const auto& field) { return field.bitWidth.has_value(); });
    std::vector<std::uint64_t> alignedFields(record.fields.size(), 0);
    if (inferAlignment)
    {
        std::uint64_t end = 0, alignment = 8;
        for (std::size_t i = 0; i < record.fields.size(); ++i)
        {
            const auto& field = record.fields[i];
            if (!field.offsetBits || !field.type.sizeBits || !field.type.alignmentBits || !*field.type.alignmentBits ||
                (*field.type.alignmentBits & (*field.type.alignmentBits - 1))) return std::nullopt;
            const auto roundUp = [](std::uint64_t value, std::uint64_t to) { return (value + to - 1) / to * to; };
            std::uint64_t fieldAlignment = *field.type.alignmentBits;
            const std::uint64_t start = record.kind == CppRecordKind::Union ? 0 : end;
            if (*field.offsetBits != roundUp(start, fieldAlignment))
            {
                while (fieldAlignment <= 8192 * 8 && roundUp(start, fieldAlignment) < *field.offsetBits) fieldAlignment *= 2;
                if (fieldAlignment > 8192 * 8 || roundUp(start, fieldAlignment) != *field.offsetBits) return std::nullopt;
                alignedFields[i] = fieldAlignment;
            }
            alignment = std::max(alignment, fieldAlignment);
            end = std::max(end, *field.offsetBits + *field.type.sizeBits);
        }
        if (*record.alignmentBits > alignment)
        {
            if ((*record.alignmentBits & (*record.alignmentBits - 1)) || *record.alignmentBits > 8192 * 8) return std::nullopt;
            alignedFields[0] = std::max(alignedFields[0], *record.alignmentBits);
        }
    }
    for (std::size_t i = 0; i < record.fields.size(); ++i)
    {
        const auto& field = record.fields[i];
        const auto& type = field.type;
        if (!field.offsetBits || field.isAnonymousCarrier || !type.sizeBits ||
            !type.alignmentBits || type.isRestrict) return std::nullopt;
        if (field.bitWidth && ((*field.bitWidth == 0 && !field.name.empty()) || *field.bitWidth > *type.sizeBits ||
            (type.kind == CppTypeKind::Boolean && *field.bitWidth > 1) ||
            (type.kind != CppTypeKind::SignedInteger && type.kind != CppTypeKind::UnsignedInteger &&
             type.kind != CppTypeKind::Boolean))) return std::nullopt;
        const CppType* element = &type;
        std::string dimensions;
        unsigned depth = 0;
        while (element->kind == CppTypeKind::Array)
        {
            // An array of qualified elements carries the element's qualifiers, never others.
            if (++depth > 16 || !element->arrayCount || *element->arrayCount == 0 ||
                *element->arrayCount > 1024 || element->isIncompleteArray || !element->element ||
                !element->sizeBits || !element->alignmentBits || element->isRestrict ||
                (element->isConst && !element->element->isConst) ||
                (element->isVolatile && !element->element->isVolatile)) return std::nullopt;
            if (!element->element->sizeBits || !element->element->alignmentBits ||
                *element->element->sizeBits > std::numeric_limits<std::uint64_t>::max() / *element->arrayCount ||
                *element->element->sizeBits * *element->arrayCount != *element->sizeBits ||
                element->element->alignmentBits != element->alignmentBits) return std::nullopt;
            dimensions += "[" + std::to_string(*element->arrayCount) + "]";
            element = element->element.get();
        }
        const auto qualifiers = cppReplayQualifiers(*element);
        if (!element->sizeBits || !element->alignmentBits || element->isRestrict ||
            !element->canonicalSpelling.starts_with(qualifiers)) return std::nullopt;
        const auto spelling = std::string_view(element->canonicalSpelling).substr(qualifiers.size());
        std::string emittedType;
        const bool signedType = element->kind == CppTypeKind::SignedInteger &&
            (spelling == "signed char" || spelling == "short" || spelling == "int" ||
             spelling == "long" || spelling == "long long");
        // T09-003: plain char keeps the signedness of the target; the replay compiles for the same target and the
        // caller compares each field type, so it is written back as char.
        const bool plainChar = (element->kind == CppTypeKind::SignedInteger || element->kind == CppTypeKind::UnsignedInteger) &&
            spelling == "char" && element->sizeBits == 8 && element->alignmentBits == 8;
        const bool unsignedType = element->kind == CppTypeKind::UnsignedInteger &&
            (spelling == "unsigned char" || spelling == "unsigned short" || spelling == "unsigned int" ||
             spelling == "unsigned long" || spelling == "unsigned long long");
        const bool realType = element->kind == CppTypeKind::Floating && (spelling == "float" || spelling == "double");
        const bool booleanType = element->kind == CppTypeKind::Boolean && spelling == "_Bool";
        if (signedType || unsignedType || realType || booleanType || plainChar) emittedType = std::string(spelling);
        else if (element->kind == CppTypeKind::Record && element->recordIdentity &&
                 element->recordIdentity->clangUsr && !element->recordIdentity->clangUsr->empty() &&
                 !element->recordIdentity->name.empty() && !element->recordIdentity->anonymousContext &&
                 !element->recordIdentity->specializationContext && element->recordIdentity->namespaceSegments.empty() &&
                 element->recordIdentity->containingRecords.empty())
        {
            if (resolve)
            {
                const auto resolved = resolve(*element);
                if (!resolved) return std::nullopt;
                emittedType = *resolved;
            }
            else emittedType = "struct __nsc_abi_dependency";
        }
        else return std::nullopt;
        result += qualifiers + emittedType;
        if (!field.bitWidth || !field.name.empty()) result += " __nsc_f" + std::to_string(i);
        result += dimensions;
        if (alignedFields[i]) result += " __attribute__((aligned(" + std::to_string(alignedFields[i] / 8) + ")))";
        if (field.bitWidth) result += " : " + std::to_string(*field.bitWidth);
        result += ";\n";
    }
    result += "};\n";
    return result;
}

// Clang must parse the reconstruction with the original target; this checks
// every physical field against the persisted data, not merely total size.
inline bool matchesSimpleCRecordAbiReplay(const CppCRecordSnapshot& expected,
                                          const CppRecordDeclaration& actual, std::size_t index = 0,
                                          const CppReplayRecordResolver& resolve = {}, bool replayAsCpp = false)
{
    const auto tag = index == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(index);
    if (!makeSimpleCRecordAbiReplay(expected, index, resolve) || actual.isCxx != replayAsCpp || !actual.complete ||
        actual.name != tag || actual.kind != expected.kind ||
        actual.sizeBits != expected.sizeBits || actual.alignmentBits != expected.alignmentBits ||
        actual.fields.size() != expected.fields.size()) return false;
    const auto sameType = [&](const auto& self, const CppType& left, const CppType& right, unsigned depth) -> bool {
        if (depth > 16 || left.kind != right.kind ||
            left.sizeBits != right.sizeBits || left.alignmentBits != right.alignmentBits ||
            left.isConst != right.isConst || left.isVolatile != right.isVolatile || left.isRestrict != right.isRestrict ||
            left.arrayCount != right.arrayCount || left.isIncompleteArray != right.isIncompleteArray) return false;
        if (left.kind == CppTypeKind::Record)
        {
            const auto spelling = resolve ? resolve(left) : std::nullopt;
            return spelling && cppReplayQualifiers(left) + *spelling == right.canonicalSpelling;
        }
        if (left.kind != CppTypeKind::Array && left.canonicalSpelling != right.canonicalSpelling) return false;
        if (left.kind == CppTypeKind::Array)
            return left.element && right.element && self(self, *left.element, *right.element, depth + 1);
        return true;
    };
    for (std::size_t i = 0; i < expected.fields.size(); ++i)
    {
        const auto& left = expected.fields[i];
        const auto& right = actual.fields[i];
        const auto expectedName = left.bitWidth && left.name.empty() ? std::string{} : "__nsc_f" + std::to_string(i);
        if (right.name != expectedName || left.bitWidth != right.bitWidth || right.isAnonymousCarrier ||
            left.offsetBits != right.offsetBits || !sameType(sameType, left.type, right.type, 0))
            return false;
    }
    return true;
}
}
