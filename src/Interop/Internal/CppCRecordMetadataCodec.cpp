#include "CppCRecordMetadataCodec.h"
#include "ByteReader.h"
#include "ByteWriter.h"

#include <limits>
#include <memory>
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

bool validateRecords(std::span<const CppCRecordSnapshot> records, std::string& error)
{
    for (std::size_t i = 0; i < records.size(); ++i)
    {
        const auto& record = records[i];
        if (record.identityKey.empty() || record.targetTriple.empty() ||
            (i && records[i - 1].identityKey >= record.identityKey))
        { error = "C record identities are empty, duplicated or not ordered"; return false; }
        if (!record.complete)
        {
            if (record.sizeBits || record.alignmentBits || !record.fields.empty())
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
            !putText(writer, record.targetTriple) || !putOptionalText(writer, record.clangUsr))
        { error = "invalid C record identity"; return false; }
        writer.u8(record.microsoftVolatileSemantics ? (*record.microsoftVolatileSemantics ? 2 : 1) : 0);
        writer.u8(static_cast<std::uint8_t>(record.kind)); writer.u8(record.complete);
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
            !reader.text(record.targetTriple) || record.targetTriple.empty() ||
            !getOptionalText(reader, record.clangUsr) || !reader.u8(volatileMode) || volatileMode > 2 || !reader.u8(kind) ||
            (kind != static_cast<std::uint8_t>(CppRecordKind::Struct) && kind != static_cast<std::uint8_t>(CppRecordKind::Union)) ||
            !getFlag(reader, record.complete) || !getOptionalNumber(reader, record.sizeBits) ||
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
}
