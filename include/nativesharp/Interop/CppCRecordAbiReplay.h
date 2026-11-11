#pragma once

#include "nativesharp/Interop/CppSemanticRecord.h"

#include <optional>
#include <limits>
#include <functional>
#include <string>
#include <string_view>

namespace nativesharp::interop
{
using CppReplayRecordResolver = std::function<std::optional<std::string>(const CppType&)>;
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
    for (std::size_t i = 0; i < record.fields.size(); ++i)
    {
        const auto& field = record.fields[i];
        const auto& type = field.type;
        if (!field.offsetBits || field.isAnonymousCarrier || !type.sizeBits ||
            !type.alignmentBits || type.isConst || type.isVolatile || type.isRestrict) return std::nullopt;
        if (field.bitWidth && (field.name.empty() || *field.bitWidth == 0 || *field.bitWidth > *type.sizeBits ||
            (type.kind != CppTypeKind::SignedInteger && type.kind != CppTypeKind::UnsignedInteger))) return std::nullopt;
        const CppType* element = &type;
        std::string dimensions;
        unsigned depth = 0;
        while (element->kind == CppTypeKind::Array)
        {
            if (++depth > 16 || !element->arrayCount || *element->arrayCount == 0 ||
                *element->arrayCount > 1024 || element->isIncompleteArray || !element->element ||
                !element->sizeBits || !element->alignmentBits || element->isConst ||
                element->isVolatile || element->isRestrict) return std::nullopt;
            if (!element->element->sizeBits || !element->element->alignmentBits ||
                *element->element->sizeBits > std::numeric_limits<std::uint64_t>::max() / *element->arrayCount ||
                *element->element->sizeBits * *element->arrayCount != *element->sizeBits ||
                element->element->alignmentBits != element->alignmentBits) return std::nullopt;
            dimensions += "[" + std::to_string(*element->arrayCount) + "]";
            element = element->element.get();
        }
        if (!element->sizeBits || !element->alignmentBits || element->isConst ||
            element->isVolatile || element->isRestrict) return std::nullopt;
        const auto spelling = std::string_view(element->canonicalSpelling);
        std::string emittedType;
        const bool signedType = element->kind == CppTypeKind::SignedInteger &&
            (spelling == "signed char" || spelling == "short" || spelling == "int" ||
             spelling == "long" || spelling == "long long");
        const bool unsignedType = element->kind == CppTypeKind::UnsignedInteger &&
            (spelling == "unsigned char" || spelling == "unsigned short" || spelling == "unsigned int" ||
             spelling == "unsigned long" || spelling == "unsigned long long");
        const bool realType = element->kind == CppTypeKind::Floating && (spelling == "float" || spelling == "double");
        if (signedType || unsignedType || realType) emittedType = element->canonicalSpelling;
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
        result += emittedType + " __nsc_f" + std::to_string(i) + dimensions;
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
                                          const CppReplayRecordResolver& resolve = {})
{
    const auto tag = index == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(index);
    if (!makeSimpleCRecordAbiReplay(expected, index, resolve) || actual.isCxx || !actual.complete ||
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
            return spelling && *spelling == right.canonicalSpelling;
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
        if (right.name != "__nsc_f" + std::to_string(i) || left.bitWidth != right.bitWidth || right.isAnonymousCarrier ||
            left.offsetBits != right.offsetBits || !sameType(sameType, left.type, right.type, 0))
            return false;
    }
    return true;
}
}
