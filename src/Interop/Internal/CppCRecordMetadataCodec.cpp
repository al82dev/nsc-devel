#include "CppCRecordMetadataCodec.h"
#include "ByteReader.h"
#include "ByteWriter.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Frontend/CppAliasBuiltinType.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"

#include <algorithm>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>

namespace nativesharp::interop::metadata_detail
{
namespace
{
constexpr std::uint32_t maxItems = 1000000;
constexpr unsigned maxDepth = 64;

bool putText(ByteWriter& writer, const std::string& value)
{
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) return false;
    writer.text(value);
    return true;
}
bool putOptionalText(ByteWriter& writer, const std::optional<std::string>& value)
{
    writer.u8(value ? 1 : 0);
    return !value || putText(writer, *value);
}
bool getFlag(ByteReader& reader, bool& value)
{
    std::uint8_t byte = 0;
    if (!reader.u8(byte) || byte > 1) return false;
    value = byte != 0;
    return true;
}
bool getOptionalText(ByteReader& reader, std::optional<std::string>& value)
{
    bool present = false;
    if (!getFlag(reader, present)) return false;
    if (!present) { value.reset(); return true; }
    std::string text;
    if (!reader.text(text)) return false;
    value = std::move(text);
    return true;
}
void putOptionalNumber(ByteWriter& writer, const std::optional<std::uint64_t>& value)
{
    writer.u8(value ? 1 : 0);
    if (value) writer.u64(*value);
}
bool getOptionalNumber(ByteReader& reader, std::optional<std::uint64_t>& value)
{
    bool present = false;
    if (!getFlag(reader, present)) return false;
    if (!present) { value.reset(); return true; }
    std::uint64_t number = 0;
    if (!reader.u64(number)) return false;
    value = number;
    return true;
}
bool putStrings(ByteWriter& writer, const std::vector<std::string>& values)
{
    if (values.size() > maxItems) return false;
    writer.u32(static_cast<std::uint32_t>(values.size()));
    for (const auto& value : values) if (!putText(writer, value)) return false;
    return true;
}
bool getStrings(ByteReader& reader, std::vector<std::string>& values)
{
    std::uint32_t count = 0;
    if (!reader.u32(count) || count > maxItems || count > reader.remaining() / 4) return false;
    values.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::string value;
        if (!reader.text(value)) return false;
        values.push_back(std::move(value));
    }
    return true;
}

bool putType(ByteWriter& writer, const CppType& type, unsigned depth)
{
    if (depth > maxDepth || static_cast<unsigned>(type.kind) > static_cast<unsigned>(CppTypeKind::Unsupported)) return false;
    writer.u8(static_cast<std::uint8_t>(type.kind));
    if (!putText(writer, type.spelling) || !putText(writer, type.canonicalSpelling) ||
        !putText(writer, type.unqualifiedCanonicalSpelling)) return false;
    writer.u8(type.isConst); writer.u8(type.isVolatile); writer.u8(type.isRestrict);
    putOptionalNumber(writer, type.sizeBits); putOptionalNumber(writer, type.alignmentBits);
    putOptionalNumber(writer, type.arrayCount); writer.u8(type.isIncompleteArray);
    writer.u8(type.recordIdentity ? 1 : 0);
    if (type.recordIdentity)
    {
        const auto& id = *type.recordIdentity;
        if (!putText(writer, id.name) || !putStrings(writer, id.namespaceSegments) ||
            !putStrings(writer, id.containingRecords)) return false;
        writer.u8(id.anonymousContext); writer.u8(id.specializationContext);
        if (!putOptionalText(writer, id.clangUsr)) return false;
    }
    writer.u8(type.enumIdentity ? 1 : 0);
    if (type.enumIdentity)
    {
        const auto& id = *type.enumIdentity;
        if (!putText(writer, id.name) || !putText(writer, id.qualifiedName)) return false;
        writer.u8(id.anonymousContext);
        if (!putOptionalText(writer, id.clangUsr)) return false;
    }
    writer.u8(type.enumUnderlying ? 1 : 0);
    if (type.enumUnderlying && !putType(writer, *type.enumUnderlying, depth + 1)) return false;
    writer.u8(type.element ? 1 : 0);
    if (type.element && !putType(writer, *type.element, depth + 1)) return false;
    writer.u8(type.function ? 1 : 0);
    if (type.function)
    {
        const auto& function = *type.function;
        if (!putType(writer, function.returnType, depth + 1) || function.parameterTypes.size() > maxItems) return false;
        writer.u32(static_cast<std::uint32_t>(function.parameterTypes.size()));
        for (const auto& parameter : function.parameterTypes)
            if (!putType(writer, parameter, depth + 1)) return false;
        if (!putText(writer, function.callingConvention)) return false;
        writer.u8(function.hasPrototype); writer.u8(function.variadic);
    }
    return true;
}

bool getType(ByteReader& reader, CppType& type, unsigned depth)
{
    std::uint8_t kind = 0;
    if (depth > maxDepth || !reader.u8(kind) || kind > static_cast<std::uint8_t>(CppTypeKind::Unsupported)) return false;
    type.kind = static_cast<CppTypeKind>(kind);
    if (!reader.text(type.spelling) || !reader.text(type.canonicalSpelling) ||
        !reader.text(type.unqualifiedCanonicalSpelling) || !getFlag(reader, type.isConst) ||
        !getFlag(reader, type.isVolatile) || !getFlag(reader, type.isRestrict) ||
        !getOptionalNumber(reader, type.sizeBits) || !getOptionalNumber(reader, type.alignmentBits) ||
        !getOptionalNumber(reader, type.arrayCount) || !getFlag(reader, type.isIncompleteArray)) return false;
    bool present = false;
    if (!getFlag(reader, present)) return false;
    if (present)
    {
        CppRecordTypeIdentity id;
        if (!reader.text(id.name) || !getStrings(reader, id.namespaceSegments) ||
            !getStrings(reader, id.containingRecords) || !getFlag(reader, id.anonymousContext) ||
            !getFlag(reader, id.specializationContext) || !getOptionalText(reader, id.clangUsr)) return false;
        type.recordIdentity = std::move(id);
    }
    if (!getFlag(reader, present)) return false;
    if (present)
    {
        CppEnumTypeIdentity id;
        if (!reader.text(id.name) || !reader.text(id.qualifiedName) ||
            !getFlag(reader, id.anonymousContext) || !getOptionalText(reader, id.clangUsr)) return false;
        type.enumIdentity = std::move(id);
    }
    if (!getFlag(reader, present)) return false;
    if (present)
    {
        auto child = std::make_shared<CppType>();
        if (!getType(reader, *child, depth + 1)) return false;
        type.enumUnderlying = std::move(child);
    }
    if (!getFlag(reader, present)) return false;
    if (present)
    {
        auto child = std::make_shared<CppType>();
        if (!getType(reader, *child, depth + 1)) return false;
        type.element = std::move(child);
    }
    if (!getFlag(reader, present)) return false;
    if (present)
    {
        auto function = std::make_shared<CppFunctionType>();
        std::uint32_t count = 0;
        if (!getType(reader, function->returnType, depth + 1) || !reader.u32(count) ||
            count > maxItems || count > reader.remaining()) return false;
        function->parameterTypes.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            CppType parameter;
            if (!getType(reader, parameter, depth + 1)) return false;
            function->parameterTypes.push_back(std::move(parameter));
        }
        if (!reader.text(function->callingConvention) || !getFlag(reader, function->hasPrototype) ||
            !getFlag(reader, function->variadic)) return false;
        type.function = std::move(function);
    }
    return true;
}

bool collectFloatFormats(const CppType& type, std::vector<std::uint8_t>& flags, unsigned depth)
{
    if (depth > maxDepth || flags.size() > maxItems) return false;
    const bool candidate = type.kind == CppTypeKind::Floating &&
        type.unqualifiedCanonicalSpelling == "long double";
    if (type.ieeeBinary64 && (!candidate || type.sizeBits != 64)) return false;
    if (candidate) flags.push_back(type.ieeeBinary64 ? 1 : 0);
    if (type.enumUnderlying && !collectFloatFormats(*type.enumUnderlying, flags, depth + 1)) return false;
    if (type.element && !collectFloatFormats(*type.element, flags, depth + 1)) return false;
    if (type.function)
    {
        if (!collectFloatFormats(type.function->returnType, flags, depth + 1)) return false;
        for (const auto& parameter : type.function->parameterTypes)
            if (!collectFloatFormats(parameter, flags, depth + 1)) return false;
    }
    return flags.size() <= maxItems;
}

bool restoreFloatFormats(CppType& type, std::span<const std::uint8_t> flags, std::size_t& index, unsigned depth)
{
    if (depth > maxDepth) return false;
    if (type.kind == CppTypeKind::Floating && type.unqualifiedCanonicalSpelling == "long double")
    {
        if (index >= flags.size() || flags[index] > 1 || (flags[index] && type.sizeBits != 64)) return false;
        type.ieeeBinary64 = flags[index++] != 0;
    }
    if (type.enumUnderlying)
    {
        auto child = std::make_shared<CppType>(*type.enumUnderlying);
        if (!restoreFloatFormats(*child, flags, index, depth + 1)) return false;
        type.enumUnderlying = std::move(child);
    }
    if (type.element)
    {
        auto child = std::make_shared<CppType>(*type.element);
        if (!restoreFloatFormats(*child, flags, index, depth + 1)) return false;
        type.element = std::move(child);
    }
    if (type.function)
    {
        auto function = std::make_shared<CppFunctionType>(*type.function);
        if (!restoreFloatFormats(function->returnType, flags, index, depth + 1)) return false;
        for (auto& parameter : function->parameterTypes)
            if (!restoreFloatFormats(parameter, flags, index, depth + 1)) return false;
        type.function = std::move(function);
    }
    return true;
}

bool collectCatalogueFloatFormats(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, std::vector<std::uint8_t>& flags)
{
    flags.clear();
    for (const auto& record : records)
        for (const auto& field : record.fields)
            if (!collectFloatFormats(field.type, flags, 0)) return false;
    for (const auto& item : enums)
    {
        if (item.declaration.underlyingType && !collectFloatFormats(*item.declaration.underlyingType, flags, 0)) return false;
        for (const auto& value : item.declaration.enumerators)
            if (!collectFloatFormats(value.valueType, flags, 0)) return false;
    }
    return true;
}

bool restoreCatalogueFloatFormats(std::vector<CppCRecordSnapshot>& records,
    std::vector<CppCEnumSnapshot>& enums, std::span<const std::uint8_t> flags)
{
    std::size_t index = 0;
    for (auto& record : records)
        for (auto& field : record.fields)
            if (!restoreFloatFormats(field.type, flags, index, 0)) return false;
    for (auto& item : enums)
    {
        if (item.declaration.underlyingType && !restoreFloatFormats(*item.declaration.underlyingType, flags, index, 0)) return false;
        for (auto& value : item.declaration.enumerators)
            if (!restoreFloatFormats(value.valueType, flags, index, 0)) return false;
    }
    return index == flags.size();
}

bool validateRecords(std::span<const CppCRecordSnapshot> records, std::string& error)
{
    for (std::size_t i = 0; i < records.size(); ++i)
    {
        const auto& record = records[i];
        std::vector<compilation::TypeNameSegment> containers;
        containers.reserve(record.containingRecords.size());
        for (const auto& name : record.containingRecords) containers.push_back({name, 0});
        const compilation::TypeIdentity identity(record.moduleName, record.namespaceSegments,
                                                  std::move(containers), record.name);
        if (record.identityKey.empty() || record.targetTriple.empty() ||
            !identity.isValid() || identity.key() != record.identityKey ||
            (i && records[i - 1].identityKey >= record.identityKey))
        { error = "C record identities are empty, duplicated or not ordered"; return false; }
        if (!record.complete)
        {
            if (record.valueLayoutUsable || record.sizeBits || record.alignmentBits || !record.fields.empty())
            { error = "incomplete C record has a layout"; return false; }
            continue;
        }
        if (!record.sizeBits || !record.alignmentBits || !*record.sizeBits || !*record.alignmentBits ||
            *record.sizeBits % 8 != 0 || *record.alignmentBits % 8 != 0 ||
            *record.sizeBits % *record.alignmentBits != 0)
        { error = "invalid complete C record dimensions"; return false; }
        for (const auto& field : record.fields)
        {
            if (!field.offsetBits || *field.offsetBits > *record.sizeBits ||
                (field.bitWidth && *field.bitWidth > *record.sizeBits - *field.offsetBits) ||
                (!field.bitWidth && field.type.sizeBits &&
                 *field.type.sizeBits > *record.sizeBits - *field.offsetBits))
            { error = "C record field is outside its layout"; return false; }
        }
    }
    return true;
}
}

bool encodeCRecordCatalog(std::span<const CppCRecordSnapshot> records,
                          std::vector<std::uint8_t>& output, std::string& error)
{
    ByteWriter writer;
    if (records.size() > maxItems) { error = "too many C records"; return false; }
    if (!validateRecords(records, error)) return false;
    writer.u32(static_cast<std::uint32_t>(records.size()));
    for (const auto& record : records)
    {
        if (record.identityKey.empty() || record.targetTriple.empty() ||
            (record.kind != CppRecordKind::Struct && record.kind != CppRecordKind::Union) ||
            record.fields.size() > maxItems || !putText(writer, record.identityKey) ||
            !putText(writer, record.moduleName) || !putStrings(writer, record.namespaceSegments) ||
            !putStrings(writer, record.containingRecords) || !putText(writer, record.name) ||
            !putText(writer, record.targetTriple) || !putOptionalText(writer, record.clangUsr))
        { error = "invalid C record identity"; return false; }
        writer.u8(record.microsoftVolatileSemantics ? (*record.microsoftVolatileSemantics ? 2 : 1) : 0);
        writer.u8(static_cast<std::uint8_t>(record.kind)); writer.u8(record.complete); writer.u8(record.valueLayoutUsable);
        putOptionalNumber(writer, record.sizeBits); putOptionalNumber(writer, record.alignmentBits);
        writer.u32(static_cast<std::uint32_t>(record.fields.size()));
        for (const auto& field : record.fields)
        {
            if (!putText(writer, field.name) || !putType(writer, field.type, 0) ||
                static_cast<unsigned>(field.access) > static_cast<unsigned>(CppAccess::None))
            { error = "invalid C record field"; return false; }
            writer.u8(static_cast<std::uint8_t>(field.access));
            putOptionalNumber(writer, field.offsetBits);
            putOptionalNumber(writer, field.bitWidth ? std::optional<std::uint64_t>(*field.bitWidth) : std::nullopt);
            if (!putOptionalText(writer, field.clangUsr)) { error = "invalid C field identity"; return false; }
        }
    }
    output = writer.takeBytes(); error.clear(); return true;
}

bool compareCRecordContracts(const CppCRecordSnapshot& left, const CppCRecordSnapshot& right,
                             CRecordContractComparison& result, std::string& error)
{
    std::vector<std::uint8_t> leftBytes, rightBytes;
    if (!encodeCRecordCatalog(std::span(&left, 1), leftBytes, error) ||
        !encodeCRecordCatalog(std::span(&right, 1), rightBytes, error)) return false;
    if (left.name != right.name || left.namespaceSegments != right.namespaceSegments ||
        left.containingRecords != right.containingRecords || left.targetTriple != right.targetTriple ||
        left.kind != right.kind || (left.clangUsr && right.clangUsr && left.clangUsr != right.clangUsr))
    {
        result = CRecordContractComparison::Different;
        error.clear();
        return true;
    }
    if (!left.complete || !right.complete || !left.clangUsr || left.clangUsr->empty() ||
        !right.clangUsr || right.clangUsr->empty())
    {
        result = CRecordContractComparison::Insufficient;
        error.clear();
        return true;
    }
    auto normalized = right;
    normalized.moduleName = left.moduleName;
    normalized.identityKey = left.identityKey;
    if (!encodeCRecordCatalog(std::span(&normalized, 1), rightBytes, error)) return false;
    result = leftBytes == rightBytes ? CRecordContractComparison::Matching : CRecordContractComparison::Different;
    if (result == CRecordContractComparison::Matching)
    {
        std::vector<std::uint8_t> leftFormats, rightFormats;
        if (!collectCatalogueFloatFormats(std::span(&left, 1), {}, leftFormats) ||
            !collectCatalogueFloatFormats(std::span(&right, 1), {}, rightFormats))
        { error = "invalid C record floating format"; return false; }
        if (leftFormats != rightFormats ||
            std::find(leftFormats.begin(), leftFormats.end(), 0) != leftFormats.end())
            result = CRecordContractComparison::Insufficient;
    }
    error.clear();
    return true;
}

bool compareCRecordSourceEvidence(const CppCRecordSnapshot& left, const CppCRecordSnapshot& right,
                                  CRecordContractComparison& result, std::string& error)
{
    CRecordContractComparison contract{};
    if (!left.complete && !right.complete)
    {
        if (!left.clangUsr || left.clangUsr->empty() || !right.clangUsr || right.clangUsr->empty())
        { result = CRecordContractComparison::Insufficient; error.clear(); return true; }
        auto normalized = right;
        normalized.moduleName = left.moduleName;
        normalized.identityKey = left.identityKey;
        std::vector<std::uint8_t> leftBytes, rightBytes;
        if (!encodeCRecordCatalog(std::span(&left, 1), leftBytes, error) ||
            !encodeCRecordCatalog(std::span(&normalized, 1), rightBytes, error)) return false;
        contract = leftBytes == rightBytes ? CRecordContractComparison::Matching : CRecordContractComparison::Different;
    }
    else if (!compareCRecordContracts(left, right, contract, error)) return false;
    if (contract != CRecordContractComparison::Matching)
    { result = contract; return true; }
    if (!left.sourceProvenance || !right.sourceProvenance)
    { result = CRecordContractComparison::Insufficient; return true; }
    const auto& first = *left.sourceProvenance;
    const auto& second = *right.sourceProvenance;
    if (first.bufferSha256 != second.bufferSha256 || first.byteOffset != second.byteOffset)
    { result = CRecordContractComparison::Different; return true; }
    std::error_code ec;
    if (!first.sourcePath.is_absolute() || !second.sourcePath.is_absolute() ||
        !std::filesystem::is_regular_file(first.sourcePath, ec) || ec)
    { result = CRecordContractComparison::Insufficient; return true; }
    const bool sameFile = std::filesystem::equivalent(first.sourcePath, second.sourcePath, ec);
    if (ec) { result = CRecordContractComparison::Insufficient; return true; }
    if (!sameFile) { result = CRecordContractComparison::Different; return true; }
    basic::Sha256Digest digest;
    std::uintmax_t size = 0;
    std::string hashError;
    if (!basic::sha256File(first.sourcePath, digest, size, hashError) || first.byteOffset >= size ||
        basic::sha256Hex(digest) != first.bufferSha256)
    { result = CRecordContractComparison::Insufficient; return true; }
    result = CRecordContractComparison::Matching;
    error.clear();
    return true;
}

bool decodeCRecordCatalog(std::span<const std::uint8_t> bytes,
                          std::vector<CppCRecordSnapshot>& output, std::string& error)
{
    ByteReader reader(bytes);
    std::uint32_t count = 0;
    if (!reader.u32(count) || count > maxItems || count > reader.remaining())
    { error = "invalid C record count"; return false; }
    std::vector<CppCRecordSnapshot> records;
    records.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        CppCRecordSnapshot record;
        std::uint8_t kind = 0;
        std::uint8_t volatileMode = 0;
        std::uint32_t fields = 0;
        if (!reader.text(record.identityKey) || record.identityKey.empty() ||
            !reader.text(record.moduleName) || !getStrings(reader, record.namespaceSegments) ||
            !getStrings(reader, record.containingRecords) || !reader.text(record.name) ||
            !reader.text(record.targetTriple) || record.targetTriple.empty() ||
            !getOptionalText(reader, record.clangUsr) || !reader.u8(volatileMode) || volatileMode > 2 || !reader.u8(kind) ||
            (kind != static_cast<std::uint8_t>(CppRecordKind::Struct) && kind != static_cast<std::uint8_t>(CppRecordKind::Union)) ||
            !getFlag(reader, record.complete) || !getFlag(reader, record.valueLayoutUsable) ||
            !getOptionalNumber(reader, record.sizeBits) ||
            !getOptionalNumber(reader, record.alignmentBits) || !reader.u32(fields) ||
            fields > maxItems || fields > reader.remaining())
        { error = "invalid C record"; return false; }
        if (volatileMode != 0) record.microsoftVolatileSemantics = volatileMode == 2;
        record.kind = static_cast<CppRecordKind>(kind);
        record.fields.reserve(fields);
        for (std::uint32_t j = 0; j < fields; ++j)
        {
            CppRecordField field;
            std::uint8_t access = 0;
            std::optional<std::uint64_t> width;
            if (!reader.text(field.name) || !getType(reader, field.type, 0) || !reader.u8(access) ||
                access > static_cast<std::uint8_t>(CppAccess::None) || !getOptionalNumber(reader, field.offsetBits) ||
                !getOptionalNumber(reader, width) || (width && *width > std::numeric_limits<unsigned>::max()) ||
                !getOptionalText(reader, field.clangUsr))
            { error = "invalid C record field"; return false; }
            field.access = static_cast<CppAccess>(access);
            if (width) field.bitWidth = static_cast<unsigned>(*width);
            record.fields.push_back(std::move(field));
        }
        records.push_back(std::move(record));
    }
    if (!reader.atEnd()) { error = "trailing C record data"; return false; }
    if (!validateRecords(records, error)) return false;
    output = std::move(records); error.clear(); return true;
}

bool encodeCTypeCatalog(std::span<const CppCRecordSnapshot> records,
                        std::span<const CppCEnumSnapshot> enums,
                        std::vector<std::uint8_t>& output, std::string& error)
{
    std::vector<std::uint8_t> recordBytes;
    if (!encodeCRecordCatalog(records, recordBytes, error)) return false;
    if (enums.size() > maxItems) { error = "too many C enums"; return false; }
    ByteWriter writer;
    writer.u64(static_cast<std::uint64_t>(recordBytes.size()));
    writer.raw(recordBytes);
    writer.u32(static_cast<std::uint32_t>(enums.size()));
    std::set<std::string_view> recordIdentities;
    for (const auto& record : records) recordIdentities.insert(record.identityKey);
    for (std::size_t i = 0; i < enums.size(); ++i)
    {
        const auto& item = enums[i]; const auto& value = item.declaration;
        if (item.identityKey.empty() || item.targetTriple.empty() ||
            (i && enums[i - 1].identityKey >= item.identityKey) || value.isCxx || value.scoped ||
            !value.complete || value.name.empty() || !value.underlyingType || !value.sizeBits ||
            !value.alignmentBits || !*value.sizeBits || !*value.alignmentBits ||
            *value.sizeBits % 8 || *value.alignmentBits % 8 ||
            *value.sizeBits % *value.alignmentBits || value.enumerators.size() > maxItems)
        { error = "invalid C enum metadata"; return false; }
        if (recordIdentities.contains(item.identityKey))
        { error = "record and enum identities collide"; return false; }
        if (!putText(writer, item.identityKey) || !putText(writer, item.targetTriple) ||
            !putText(writer, value.name) || !putText(writer, value.qualifiedName) ||
            !putOptionalText(writer, value.clangUsr) || !putType(writer, *value.underlyingType, 0))
        { error = "invalid C enum identity or underlying type"; return false; }
        writer.u64(*value.sizeBits); writer.u64(*value.alignmentBits);
        writer.u32(static_cast<std::uint32_t>(value.enumerators.size()));
        std::set<std::string> names;
        for (const auto& member : value.enumerators)
        {
            if (member.name.empty() || member.decimalValue.empty() || !names.insert(member.name).second ||
                !putText(writer, member.name) || !putText(writer, member.decimalValue))
            { error = "invalid C enumerator"; return false; }
            writer.u32(member.valueBits); writer.u8(member.valueIsUnsigned);
            if (!putType(writer, member.valueType, 0) || !putOptionalText(writer, member.clangUsr))
            { error = "invalid C enumerator type"; return false; }
        }
    }
    output = writer.takeBytes(); error.clear(); return true;
}

bool decodeCTypeCatalog(std::span<const std::uint8_t> bytes,
                        std::vector<CppCRecordSnapshot>& records,
                        std::vector<CppCEnumSnapshot>& enums, std::string& error)
{
    ByteReader reader(bytes);
    std::uint64_t recordSize = 0;
    if (!reader.u64(recordSize) || recordSize > reader.remaining())
    { error = "invalid C record catalogue size"; return false; }
    std::vector<std::uint8_t> recordBytes(static_cast<std::size_t>(recordSize));
    std::vector<CppCRecordSnapshot> decodedRecords;
    if (!reader.raw(recordBytes) || !decodeCRecordCatalog(recordBytes, decodedRecords, error)) return false;
    std::uint32_t count = 0;
    if (!reader.u32(count) || count > maxItems || count > reader.remaining())
    { error = "invalid C enum count"; return false; }
    std::vector<CppCEnumSnapshot> decodedEnums;
    decodedEnums.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        CppCEnumSnapshot item;
        auto& value = item.declaration;
        CppType underlying;
        std::uint64_t size = 0, alignment = 0;
        std::uint32_t members = 0;
        if (!reader.text(item.identityKey) || !reader.text(item.targetTriple) ||
            !reader.text(value.name) || !reader.text(value.qualifiedName) ||
            !getOptionalText(reader, value.clangUsr) || !getType(reader, underlying, 0) ||
            !reader.u64(size) || !reader.u64(alignment) || !reader.u32(members) ||
            members > maxItems || members > reader.remaining())
        { error = "invalid C enum metadata"; return false; }
        value.isCxx = false; value.scoped = false; value.complete = true;
        value.underlyingType = std::move(underlying); value.sizeBits = size; value.alignmentBits = alignment;
        value.enumerators.reserve(members);
        for (std::uint32_t j = 0; j < members; ++j)
        {
            CppEnumeratorDeclaration member;
            if (!reader.text(member.name) || !reader.text(member.decimalValue) ||
                !reader.u32(member.valueBits) || !getFlag(reader, member.valueIsUnsigned) ||
                !getType(reader, member.valueType, 0) || !getOptionalText(reader, member.clangUsr))
            { error = "invalid C enumerator metadata"; return false; }
            value.enumerators.push_back(std::move(member));
        }
        decodedEnums.push_back(std::move(item));
    }
    if (!reader.atEnd()) { error = "trailing C type catalogue data"; return false; }
    std::vector<std::uint8_t> canonical;
    if (!encodeCTypeCatalog(decodedRecords, decodedEnums, canonical, error) || canonical.size() != bytes.size() ||
        !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
    { if (error.empty()) error = "noncanonical C type catalogue"; return false; }
    records = std::move(decodedRecords); enums = std::move(decodedEnums); error.clear(); return true;
}

bool encodeCTypeCatalogWithProvenance(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, std::vector<std::uint8_t>& output, std::string& error)
{
    std::vector<std::uint8_t> base;
    if (!encodeCTypeCatalog(records, enums, base, error)) return false;
    std::vector<std::uint8_t> formats;
    if (!collectCatalogueFloatFormats(records, enums, formats))
    { error = "invalid floating format in C catalogue"; return false; }
    const bool hasFloatFormats = std::any_of(formats.begin(), formats.end(), [](auto value) { return value != 0; });
    const bool hasIncompleteProvenance = std::any_of(records.begin(), records.end(),
        [](const auto& record) { return !record.complete && record.sourceProvenance.has_value(); });
    ByteWriter writer;
    writer.u32(hasIncompleteProvenance ? (hasFloatFormats ? 4 : 3) : (hasFloatFormats ? 2 : 1));
    writer.u64(static_cast<std::uint64_t>(base.size()));
    writer.raw(base);
    writer.u32(static_cast<std::uint32_t>(records.size()));
    for (const auto& record : records)
    {
        writer.u8(record.sourceProvenance.has_value());
        if (!record.sourceProvenance) continue;
        const auto& source = *record.sourceProvenance;
        std::string path;
        try
        {
            const auto utf8 = source.sourcePath.generic_u8string();
            path.assign(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        }
        catch (const std::exception&) { error = "invalid C provenance path encoding"; return false; }
        const auto validHex = [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); };
        if (path.empty() || source.bufferSha256.size() != 64 ||
            !std::all_of(source.bufferSha256.begin(), source.bufferSha256.end(), validHex) ||
            !putText(writer, path) || !putText(writer, source.bufferSha256))
        { error = "invalid C record source provenance"; return false; }
        writer.u32(source.byteOffset);
    }
    if (hasFloatFormats)
    {
        writer.u32(static_cast<std::uint32_t>(formats.size()));
        for (const auto format : formats) writer.u8(format);
    }
    output = writer.takeBytes(); error.clear(); return true;
}

bool decodeCTypeCatalogWithProvenance(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums, std::string& error)
{
    ByteReader reader(bytes);
    std::uint32_t version = 0, count = 0;
    std::uint64_t baseSize = 0;
    if (!reader.u32(version) || (version < 1 || version > 4) || !reader.u64(baseSize) || baseSize > reader.remaining())
    { error = "invalid C provenance catalogue envelope"; return false; }
    std::vector<std::uint8_t> base(static_cast<std::size_t>(baseSize));
    std::vector<CppCRecordSnapshot> decodedRecords;
    std::vector<CppCEnumSnapshot> decodedEnums;
    if (!reader.raw(base) || !decodeCTypeCatalog(base, decodedRecords, decodedEnums, error)) return false;
    if (!reader.u32(count) || count != decodedRecords.size())
    { error = "C provenance record count mismatch"; return false; }
    for (auto& record : decodedRecords)
    {
        bool present = false;
        if (!getFlag(reader, present)) { error = "invalid C provenance flag"; return false; }
        if (!present) continue;
        std::string path, digest;
        std::uint32_t offset = 0;
        if (!reader.text(path) || !reader.text(digest) || !reader.u32(offset))
        { error = "truncated C source provenance"; return false; }
        try
        {
            const std::u8string utf8(reinterpret_cast<const char8_t*>(path.data()), path.size());
            record.sourceProvenance = CppRecordDeclaration::SourceProvenance{
                std::filesystem::path(utf8), std::move(digest), offset};
        }
        catch (const std::exception&) { error = "invalid C provenance path encoding"; return false; }
    }
    if (version == 2 || version == 4)
    {
        std::uint32_t formatCount = 0;
        if (!reader.u32(formatCount) || formatCount > maxItems || formatCount > reader.remaining())
        { error = "invalid C floating format count"; return false; }
        std::vector<std::uint8_t> formats(formatCount);
        for (auto& format : formats)
            if (!reader.u8(format) || format > 1) { error = "invalid C floating format"; return false; }
        if (!restoreCatalogueFloatFormats(decodedRecords, decodedEnums, formats))
        { error = "C floating format does not match catalogue types"; return false; }
    }
    if (!reader.atEnd()) { error = "trailing C provenance data"; return false; }
    std::vector<std::uint8_t> canonical;
    if (!encodeCTypeCatalogWithProvenance(decodedRecords, decodedEnums, canonical, error) ||
        canonical.size() != bytes.size() || !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
    { if (error.empty()) error = "noncanonical C provenance catalogue"; return false; }
    records = std::move(decodedRecords); enums = std::move(decodedEnums); error.clear(); return true;
}

bool isCTypeCatalogWithProvenance(std::span<const std::uint8_t> bytes) noexcept
{
    // A legacy catalogue begins with a record-block length of at least four.
    return bytes.size() >= 4 && (bytes[0] >= 1 && bytes[0] <= 4) &&
        bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0;
}

bool decodeVersionedCTypeCatalog(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums, std::string& error)
{
    return isCTypeCatalogWithProvenance(bytes) ?
        decodeCTypeCatalogWithProvenance(bytes, records, enums, error) :
        decodeCTypeCatalog(bytes, records, enums, error);
}

bool compareCRecordClosureEvidence(std::span<const CppCRecordSnapshot> leftRecords,
    std::span<const CppCEnumSnapshot> leftEnums, std::string_view leftRoot,
    std::span<const CppCRecordSnapshot> rightRecords, std::span<const CppCEnumSnapshot> rightEnums,
    std::string_view rightRoot, CRecordContractComparison& result, std::string& error)
{
    std::vector<std::uint8_t> checked;
    if (!encodeCTypeCatalog(leftRecords, leftEnums, checked, error) ||
        !encodeCTypeCatalog(rightRecords, rightEnums, checked, error)) return false;
    const auto findRecord = [](auto records, std::string_view identity) -> const CppCRecordSnapshot* {
        const auto found = std::find_if(records.begin(), records.end(),
            [&](const auto& item) { return item.identityKey == identity; });
        return found == records.end() ? nullptr : &*found;
    };
    const auto findEnum = [](auto enums, std::string_view identity) -> const CppCEnumSnapshot* {
        const auto found = std::find_if(enums.begin(), enums.end(),
            [&](const auto& item) { return item.identityKey == identity; });
        return found == enums.end() ? nullptr : &*found;
    };
    const auto* left = findRecord(leftRecords, leftRoot);
    const auto* right = findRecord(rightRecords, rightRoot);
    if (!left || !right) { result = CRecordContractComparison::Insufficient; error.clear(); return true; }
    std::vector<std::pair<const CppCRecordSnapshot*, const CppCRecordSnapshot*>> pending{{left, right}};
    std::set<std::pair<std::string, std::string>> seenRecords, seenEnums;
    CRecordContractComparison verdict = CRecordContractComparison::Matching;
    while (!pending.empty() && verdict == CRecordContractComparison::Matching)
    {
        const auto [a, b] = pending.back(); pending.pop_back();
        if (!seenRecords.emplace(a->identityKey, b->identityKey).second) continue;
        if (!compareCRecordSourceEvidence(*a, *b, verdict, error)) return false;
        if (verdict != CRecordContractComparison::Matching) break;
        std::function<bool(const CppType&, unsigned)> visit = [&](const CppType& type, unsigned depth) {
            if (verdict != CRecordContractComparison::Matching) return true;
            if (depth > 64) { verdict = CRecordContractComparison::Insufficient; return true; }
            if (type.kind == CppTypeKind::Record)
            {
                if (!type.recordIdentity || type.recordIdentity->anonymousContext ||
                    type.recordIdentity->specializationContext || type.recordIdentity->name.empty())
                { verdict = CRecordContractComparison::Insufficient; return true; }
                std::vector<compilation::TypeNameSegment> containers;
                for (const auto& name : type.recordIdentity->containingRecords) containers.push_back({name, 0});
                const compilation::TypeIdentity leftIdentity(a->moduleName, type.recordIdentity->namespaceSegments,
                    containers, type.recordIdentity->name);
                const compilation::TypeIdentity rightIdentity(b->moduleName, type.recordIdentity->namespaceSegments,
                    std::move(containers), type.recordIdentity->name);
                const auto* leftDependency = findRecord(leftRecords, leftIdentity.key());
                const auto* rightDependency = findRecord(rightRecords, rightIdentity.key());
                if (!leftIdentity.isValid() || !rightIdentity.isValid() || !leftDependency || !rightDependency)
                { verdict = CRecordContractComparison::Insufficient; return true; }
                if (!type.recordIdentity->clangUsr || type.recordIdentity->clangUsr->empty())
                { verdict = CRecordContractComparison::Insufficient; return true; }
                if (leftDependency->clangUsr != type.recordIdentity->clangUsr ||
                    rightDependency->clangUsr != type.recordIdentity->clangUsr)
                { verdict = CRecordContractComparison::Different; return true; }
                pending.emplace_back(leftDependency, rightDependency);
            }
            if (type.kind == CppTypeKind::Enumeration)
            {
                if (!type.enumIdentity || type.enumIdentity->anonymousContext || type.enumIdentity->name.empty() ||
                    type.enumIdentity->qualifiedName != type.enumIdentity->name)
                { verdict = CRecordContractComparison::Insufficient; return true; }
                const compilation::TypeIdentity leftIdentity(a->moduleName, {}, {}, type.enumIdentity->name);
                const compilation::TypeIdentity rightIdentity(b->moduleName, {}, {}, type.enumIdentity->name);
                const auto* leftEnum = findEnum(leftEnums, leftIdentity.key());
                const auto* rightEnum = findEnum(rightEnums, rightIdentity.key());
                if (!leftEnum || !rightEnum || !leftEnum->declaration.clangUsr ||
                    !rightEnum->declaration.clangUsr || leftEnum->declaration.clangUsr->empty() ||
                    rightEnum->declaration.clangUsr->empty())
                { verdict = CRecordContractComparison::Insufficient; return true; }
                if (!type.enumIdentity->clangUsr || type.enumIdentity->clangUsr->empty())
                { verdict = CRecordContractComparison::Insufficient; return true; }
                if (leftEnum->declaration.clangUsr != type.enumIdentity->clangUsr ||
                    rightEnum->declaration.clangUsr != type.enumIdentity->clangUsr)
                { verdict = CRecordContractComparison::Different; return true; }
                if (seenEnums.emplace(leftIdentity.key(), rightIdentity.key()).second)
                {
                    auto normalized = *rightEnum;
                    normalized.identityKey = leftEnum->identityKey;
                    std::vector<std::uint8_t> leftBytes, rightBytes;
                    if (!encodeCTypeCatalog({}, std::span(leftEnum, 1), leftBytes, error) ||
                        !encodeCTypeCatalog({}, std::span(&normalized, 1), rightBytes, error)) return false;
                    if (leftBytes != rightBytes) { verdict = CRecordContractComparison::Different; return true; }
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
        for (const auto& field : a->fields)
        {
            if (!visit(field.type, 0)) return false;
            if (verdict != CRecordContractComparison::Matching) break;
        }
    }
    result = verdict; error.clear(); return true;
}

bool reconstructCImportedTypes(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, std::size_t projectOrdinal,
    std::string_view moduleName, std::string_view targetTriple, std::string_view sourcePath,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error)
{
    if (moduleName.empty() || targetTriple != "x86_64-pc-windows-msvc" || sourcePath.empty())
    { error = "C type reconstruction requires module, target and source"; return false; }
    std::vector<std::uint8_t> verified;
    if (!encodeCTypeCatalog(records, enums, verified, error)) return false;
    std::vector<frontend::CppImportedTypeDeclaration> result;
    result.reserve(records.size() + enums.size());
    for (const auto& record : records)
    {
        if (record.moduleName != moduleName || record.targetTriple != targetTriple)
        { error = "C record module or target does not match consumer"; return false; }
        std::vector<compilation::TypeNameSegment> containers;
        for (const auto& name : record.containingRecords) containers.push_back({name, 0});
        compilation::TypeIdentity identity(record.moduleName, record.namespaceSegments, std::move(containers), record.name);
        frontend::CppImportedTypeDeclaration type{std::move(identity), frontend::NamedTypeDeclarationKind::Struct,
            frontend::NamedTypeAccessibility::Public, projectOrdinal, std::string(sourcePath)};
        type.origin = record.kind == CppRecordKind::Union ? frontend::CppImportedTypeOrigin::CUnion :
                                                          frontend::CppImportedTypeOrigin::CStruct;
        type.targetTriple = record.targetTriple;
        type.clangUsr = record.clangUsr;
        type.isIncomplete = !record.complete;
        type.hasCFlexibleArrayMember = record.complete && record.kind == CppRecordKind::Struct &&
            !record.fields.empty() && record.fields.back().type.kind == CppTypeKind::Array &&
            record.fields.back().type.isIncompleteArray;
        if (record.complete)
        {
            const auto alignment = *record.alignmentBits / 8;
            if ((alignment & (alignment - 1)) != 0)
            { error = "C record alignment is not a power of two"; return false; }
            const frontend::CppImportedObjectLayout layout{*record.sizeBits / 8, alignment};
            if (record.valueLayoutUsable) type.objectLayout = layout;
            else type.memberAccessLayout = layout;
        }
        result.push_back(std::move(type));
    }
    for (const auto& item : enums)
    {
        const auto& enumeration = item.declaration;
        compilation::TypeIdentity identity(std::string(moduleName), {}, {}, enumeration.name);
        const auto base = enumeration.underlyingType ? frontend::cppAliasBuiltinType(*enumeration.underlyingType) : std::nullopt;
        if (!identity.isValid() || identity.key() != item.identityKey || item.targetTriple != targetTriple ||
            !base || base->pointerDepth != 0 || !enumeration.underlyingType->sizeBits ||
            *enumeration.underlyingType->sizeBits != *enumeration.sizeBits ||
            (enumeration.underlyingType->kind != CppTypeKind::SignedInteger &&
             enumeration.underlyingType->kind != CppTypeKind::UnsignedInteger))
        { error = "C enum identity, target or underlying type is invalid"; return false; }
        const auto alignment = *enumeration.alignmentBits / 8;
        if ((alignment & (alignment - 1)) != 0)
        { error = "C enum alignment is not a power of two"; return false; }
        frontend::CppImportedTypeDeclaration type{std::move(identity), frontend::NamedTypeDeclarationKind::Enum,
            frontend::NamedTypeAccessibility::Public, projectOrdinal, std::string(sourcePath)};
        type.origin = frontend::CppImportedTypeOrigin::CEnum;
        type.targetTriple = item.targetTriple;
        type.clangUsr = enumeration.clangUsr;
        type.objectLayout = frontend::CppImportedObjectLayout{*enumeration.sizeBits / 8, alignment};
        type.enumBase = frontend::CppImportedEnumBase{base->token, std::string(base->name)};
        result.push_back(std::move(type));
    }
    output = std::move(result); error.clear(); return true;
}

bool reconstructPrecompiledCImportedTypes(std::span<const frontend::PrecompiledSyntaxModule> modules,
    const compilation::ProjectGraph& graph, std::string_view targetTriple,
    std::vector<frontend::CppImportedTypeDeclaration>& output, std::string& error)
{
    std::map<std::string, std::size_t> projectByModule;
    std::set<std::string> ambiguousModules;
    for (std::size_t i = 0; i < graph.projects().size(); ++i)
    {
        const auto& name = graph.projects()[i].moduleName();
        if (!projectByModule.emplace(name, i).second) ambiguousModules.insert(name);
    }
    std::map<std::string, std::vector<std::uint8_t>> seen;
    std::vector<frontend::CppImportedTypeDeclaration> result;
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        if (!decodeVersionedCTypeCatalog(module.cppCTypeCatalog, records, enums, error)) return false;
        const auto add = [&](std::string_view identity, std::string_view owner,
                             std::span<const CppCRecordSnapshot> oneRecord,
                             std::span<const CppCEnumSnapshot> oneEnum) {
            const auto project = projectByModule.find(std::string(owner));
            if (project == projectByModule.end() || ambiguousModules.contains(std::string(owner)))
            { error = "C type owner is missing or ambiguous: " + std::string(owner); return false; }
            std::vector<std::uint8_t> canonical;
            if (!encodeCTypeCatalogWithProvenance(oneRecord, oneEnum, canonical, error)) return false;
            const auto [previous, inserted] = seen.emplace(std::string(identity), canonical);
            if (!inserted)
            {
                if (previous->second != canonical)
                { error = "precompiled C type definitions conflict: " + std::string(identity); return false; }
                return true;
            }
            std::vector<frontend::CppImportedTypeDeclaration> reconstructed;
            if (!reconstructCImportedTypes(oneRecord, oneEnum, project->second, owner, targetTriple,
                    module.metadataPath.generic_string(), reconstructed, error)) return false;
            result.insert(result.end(), std::make_move_iterator(reconstructed.begin()),
                          std::make_move_iterator(reconstructed.end()));
            return true;
        };
        for (const auto& record : records)
            if (!add(record.identityKey, record.moduleName, std::span(&record, 1), {})) return false;
        for (const auto& item : enums)
        {
            const auto separator = item.identityKey.find("::");
            if (separator == std::string::npos || separator == 0)
            { error = "precompiled C enum has invalid identity"; return false; }
            if (!add(item.identityKey, std::string_view(item.identityKey).substr(0, separator), {},
                     std::span(&item, 1))) return false;
        }
    }
    output = std::move(result); error.clear(); return true;
}
}
