#pragma once

#include "nativesharp/Interop/CppSemanticRecord.h"

#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::interop
{
// Preliminary reconstruction for asking Clang about a persisted C record.
// This is not an ABI proof: the caller must also establish that all attributes
// affecting parameter passing were captured and bind the resulting plan to
// the original function and type identities.
inline std::optional<std::string> makeSimpleCRecordAbiReplay(const CppCRecordSnapshot& record,
                                                            std::size_t index = 0)
{
    if (!record.complete || !record.valueLayoutUsable || !record.sizeBits || !record.alignmentBits ||
        !record.targetTriple.starts_with("x86_64-pc-windows-msvc") || record.fields.empty() ||
        record.fields.size() > 256 ||
        (record.kind != CppRecordKind::Struct && record.kind != CppRecordKind::Union)) return std::nullopt;
    const auto tag = index == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(index);
    std::string result = (record.kind == CppRecordKind::Struct ? "struct " : "union ") + tag + " {\n";
    for (std::size_t i = 0; i < record.fields.size(); ++i)
    {
        const auto& field = record.fields[i];
        const auto& type = field.type;
        if (!field.offsetBits || field.bitWidth || field.isAnonymousCarrier || !type.sizeBits ||
            !type.alignmentBits || type.isConst || type.isVolatile || type.isRestrict) return std::nullopt;
        const auto spelling = std::string_view(type.canonicalSpelling);
        const bool signedType = type.kind == CppTypeKind::SignedInteger &&
            (spelling == "signed char" || spelling == "short" || spelling == "int" ||
             spelling == "long" || spelling == "long long");
        const bool unsignedType = type.kind == CppTypeKind::UnsignedInteger &&
            (spelling == "unsigned char" || spelling == "unsigned short" || spelling == "unsigned int" ||
             spelling == "unsigned long" || spelling == "unsigned long long");
        const bool realType = type.kind == CppTypeKind::Floating && (spelling == "float" || spelling == "double");
        if (!signedType && !unsignedType && !realType) return std::nullopt;
        result += type.canonicalSpelling + " __nsc_f" + std::to_string(i) + ";\n";
    }
    result += "};\n";
    return result;
}

// Clang must parse the reconstruction with the original target; this checks
// every physical field against the persisted data, not merely total size.
inline bool matchesSimpleCRecordAbiReplay(const CppCRecordSnapshot& expected,
                                          const CppRecordDeclaration& actual, std::size_t index = 0)
{
    const auto tag = index == 0 ? std::string("__nsc_abi_record") : "__nsc_abi_record_" + std::to_string(index);
    if (!makeSimpleCRecordAbiReplay(expected, index) || actual.isCxx || !actual.complete ||
        actual.name != tag || actual.kind != expected.kind ||
        actual.sizeBits != expected.sizeBits || actual.alignmentBits != expected.alignmentBits ||
        actual.fields.size() != expected.fields.size()) return false;
    for (std::size_t i = 0; i < expected.fields.size(); ++i)
    {
        const auto& left = expected.fields[i];
        const auto& right = actual.fields[i];
        if (right.name != "__nsc_f" + std::to_string(i) || right.bitWidth || right.isAnonymousCarrier ||
            left.offsetBits != right.offsetBits || left.type.kind != right.type.kind ||
            left.type.canonicalSpelling != right.type.canonicalSpelling ||
            left.type.sizeBits != right.type.sizeBits || left.type.alignmentBits != right.type.alignmentBits)
            return false;
    }
    return true;
}
}
