#include "nativesharp/Interop/CppAbiPlanQueries.h"
#include "CppCRecordMetadataCodec.h"
#include "nativesharp/Interop/CppCRecordAbiReplay.h"
#include "nativesharp/Interop/CppVariadicAbiCodec.h"
#include "nativesharp/Interop/CppVariadicAbiValidation.h"
#include "nativesharp/Interop/CppRegCallAbiValidation.h"
#include "ByteReader.h"
#include "ByteWriter.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Frontend/CppAliasBuiltinType.h"
#include "nativesharp/Frontend/CppImportedTypeAlias.h"
#include "nativesharp/Frontend/PrecompiledSyntaxModule.h"
#include "nativesharp/Frontend/NamespaceStateSymbol.h"

#include <algorithm>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
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

bool putType(ByteWriter& writer, const CppType& type, unsigned depth, bool allowAtomic = false)
{
    const auto maximumKind = allowAtomic ? CppTypeKind::Atomic : CppTypeKind::Unsupported;
    if (depth > maxDepth || static_cast<unsigned>(type.kind) > static_cast<unsigned>(maximumKind)) return false;
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
    if (type.enumUnderlying && !putType(writer, *type.enumUnderlying, depth + 1, allowAtomic)) return false;
    writer.u8(type.element ? 1 : 0);
    if (type.element && !putType(writer, *type.element, depth + 1, allowAtomic)) return false;
    writer.u8(type.function ? 1 : 0);
    if (type.function)
    {
        const auto& function = *type.function;
        if (!putType(writer, function.returnType, depth + 1, allowAtomic) || function.parameterTypes.size() > maxItems) return false;
        writer.u32(static_cast<std::uint32_t>(function.parameterTypes.size()));
        for (const auto& parameter : function.parameterTypes)
            if (!putType(writer, parameter, depth + 1, allowAtomic)) return false;
        if (!putText(writer, function.callingConvention)) return false;
        writer.u8(function.hasPrototype); writer.u8(function.variadic);
    }
    return true;
}

bool getType(ByteReader& reader, CppType& type, unsigned depth, bool allowAtomic = false)
{
    std::uint8_t kind = 0;
    const auto maximumKind = allowAtomic ? CppTypeKind::Atomic : CppTypeKind::Unsupported;
    if (depth > maxDepth || !reader.u8(kind) || kind > static_cast<std::uint8_t>(maximumKind)) return false;
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
        if (!getType(reader, *child, depth + 1, allowAtomic)) return false;
        type.enumUnderlying = std::move(child);
    }
    if (!getFlag(reader, present)) return false;
    if (present)
    {
        auto child = std::make_shared<CppType>();
        if (!getType(reader, *child, depth + 1, allowAtomic)) return false;
        type.element = std::move(child);
    }
    if (!getFlag(reader, present)) return false;
    if (present)
    {
        auto function = std::make_shared<CppFunctionType>();
        std::uint32_t count = 0;
        if (!getType(reader, function->returnType, depth + 1, allowAtomic) || !reader.u32(count) ||
            count > maxItems || count > reader.remaining()) return false;
        function->parameterTypes.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            CppType parameter;
            if (!getType(reader, parameter, depth + 1, allowAtomic)) return false;
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
        if (record.cppValueObservation &&
            (!record.complete || !record.valueLayoutUsable || record.plainReplaySyntax || record.packedReplaySyntax ||
             record.targetTriple != "x86_64-pc-windows-msvc" || !record.clangUsr || record.clangUsr->empty() ||
             record.cppValueObservation->operations.clangUsr != record.clangUsr ||
             !cppRecordValueOperationsEligible(record.cppValueObservation->traits,
                                               record.cppValueObservation->operations)))
        { error = "invalid C++ record value observation"; return false; }
        if (!record.complete)
        {
            if (record.valueLayoutUsable || record.plainReplaySyntax || record.packedReplaySyntax ||
                record.sizeBits || record.alignmentBits || !record.fields.empty())
            { error = "incomplete C record has a layout"; return false; }
            continue;
        }
        if (!record.sizeBits || !record.alignmentBits || !*record.sizeBits || !*record.alignmentBits ||
            *record.sizeBits % 8 != 0 || *record.alignmentBits % 8 != 0 ||
            *record.sizeBits % *record.alignmentBits != 0)
        { error = "invalid complete C record dimensions"; return false; }
        if ((record.plainReplaySyntax || record.packedReplaySyntax) &&
            (!record.clangUsr || record.clangUsr->empty() || !makeSimpleCRecordAbiReplay(record)))
        { error = "invalid plain C record replay observation"; return false; }
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

namespace
{
bool putCppValueObservation(ByteWriter& writer, const std::optional<CppRecordValueObservation>& value)
{
    writer.u8(value.has_value());
    if (!value) return true;
    const auto& traits = value->traits;
    writer.u8(traits.triviallyCopyable); writer.u8(traits.standardLayout);
    writer.u8(traits.trivialDefaultConstructor); writer.u8(traits.trivialDestructor);
    writer.u8(traits.plainOldData); writer.u8(traits.hasNonLiteralOrVolatileFieldsOrBases);
    const auto putSelection = [&](const CppSpecialMemberSelection& member) {
        writer.u8(static_cast<std::uint8_t>(member.status));
        writer.u8(member.access ? static_cast<std::uint8_t>(*member.access) + 1 : 0);
        writer.u8(member.isDeleted ? (*member.isDeleted ? 2 : 1) : 0);
        writer.u8(member.isTrivial ? (*member.isTrivial ? 2 : 1) : 0);
    };
    const auto& operations = value->operations;
    putSelection(operations.copyFromConst); putSelection(operations.copyFromMutable);
    putSelection(operations.destructor); putSelection(operations.defaultConstructor);
    putSelection(operations.assignFromConst); putSelection(operations.assignFromMutable);
    return putOptionalText(writer, operations.clangUsr);
}

bool getCppValueObservation(ByteReader& reader, std::optional<CppRecordValueObservation>& value)
{
    bool present = false;
    if (!getFlag(reader, present)) return false;
    if (!present) { value.reset(); return true; }
    CppRecordValueObservation decoded;
    auto& traits = decoded.traits;
    if (!getFlag(reader, traits.triviallyCopyable) || !getFlag(reader, traits.standardLayout) ||
        !getFlag(reader, traits.trivialDefaultConstructor) || !getFlag(reader, traits.trivialDestructor) ||
        !getFlag(reader, traits.plainOldData) || !getFlag(reader, traits.hasNonLiteralOrVolatileFieldsOrBases)) return false;
    const auto getSelection = [&](CppSpecialMemberSelection& member) {
        std::uint8_t status = 0, access = 0, deleted = 0, trivial = 0;
        if (!reader.u8(status) || status > static_cast<std::uint8_t>(CppSpecialMemberSelectionStatus::Ambiguous) ||
            !reader.u8(access) || access > static_cast<std::uint8_t>(CppAccess::None) + 1 ||
            !reader.u8(deleted) || deleted > 2 || !reader.u8(trivial) || trivial > 2) return false;
        member.status = static_cast<CppSpecialMemberSelectionStatus>(status);
        if (access) member.access = static_cast<CppAccess>(access - 1);
        if (deleted) member.isDeleted = deleted == 2;
        if (trivial) member.isTrivial = trivial == 2;
        return true;
    };
    auto& operations = decoded.operations;
    if (!getSelection(operations.copyFromConst) || !getSelection(operations.copyFromMutable) ||
        !getSelection(operations.destructor) || !getSelection(operations.defaultConstructor) ||
        !getSelection(operations.assignFromConst) || !getSelection(operations.assignFromMutable) ||
        !getOptionalText(reader, operations.clangUsr)) return false;
    value = std::move(decoded);
    return true;
}
}

namespace
{
bool validDependencyAtomicTypes(const CppType& type, unsigned depth)
{
    if (depth > maxDepth) return false;
    if (type.kind == CppTypeKind::Atomic && !type.element) return false;
    if (type.enumUnderlying && !validDependencyAtomicTypes(*type.enumUnderlying, depth + 1)) return false;
    if (type.element && !validDependencyAtomicTypes(*type.element, depth + 1)) return false;
    if (type.function)
    {
        if (!validDependencyAtomicTypes(type.function->returnType, depth + 1)) return false;
        for (const auto& parameter : type.function->parameterTypes)
            if (!validDependencyAtomicTypes(parameter, depth + 1)) return false;
    }
    return true;
}
}

bool encodeCDependencyType(const CppType& type, std::vector<std::uint8_t>& output, std::string& error)
{
    std::vector<std::uint8_t> formats;
    ByteWriter writer;
    if (!validDependencyAtomicTypes(type, 0) || !collectFloatFormats(type, formats, 0) ||
        !putType(writer, type, 0, true))
    { error = "invalid C dependency type"; return false; }
    writer.u32(static_cast<std::uint32_t>(formats.size()));
    writer.raw(formats);
    output = writer.takeBytes();
    error.clear();
    return true;
}

bool decodeCDependencyType(std::span<const std::uint8_t> bytes, CppType& output, std::string& error)
{
    ByteReader reader(bytes);
    CppType type;
    std::uint32_t count = 0;
    if (!getType(reader, type, 0, true) || !reader.u32(count) || count > maxItems || count > reader.remaining())
    { error = "invalid C dependency type"; return false; }
    std::vector<std::uint8_t> formats(count);
    std::size_t index = 0;
    if (!reader.raw(formats) || !reader.atEnd() || !restoreFloatFormats(type, formats, index, 0) ||
        index != formats.size() || !validDependencyAtomicTypes(type, 0))
    { error = "invalid C dependency type floating formats"; return false; }
    output = std::move(type);
    error.clear();
    return true;
}

bool captureCDependencyFunction(std::span<const CppSemanticInventoryEntry> inventory,
    const frontend::NamespaceFunctionSymbol& selected, std::size_t referenceIndex,
    std::string_view referenceIdentity, CDependencyFunctionSnapshot& output, std::string& error)
{
    if (selected.moduleName.empty() || selected.mangledName.empty() || selected.cppCanonicalIdentity.empty() ||
        !selected.cppClangUsr || selected.cppClangUsr->empty() || referenceIdentity.empty())
    { error = "selected C function has incomplete identity"; return false; }
    const CppSemanticInventoryEntry* match = nullptr;
    for (const auto& entry : inventory)
    {
        if (entry.projectOrdinal != selected.projectOrdinal || entry.referenceIndex != referenceIndex ||
            entry.targetTriple != "x86_64-pc-windows-msvc" ||
            (entry.declaration.languageLinkage != CppLanguageLinkage::C &&
             entry.declaration.languageLinkage != CppLanguageLinkage::Cpp &&
             !(entry.declaration.languageLinkage == CppLanguageLinkage::None &&
               entry.declaration.isCStaticInlineDefinition && !entry.declaration.externalLinkage)) ||
            entry.declaration.clangUsr != selected.cppClangUsr ||
            entry.declaration.externalSymbol != std::optional<std::string>{selected.mangledName}) continue;
        const auto* projection = std::get_if<frontend::NamespaceFunctionSymbol>(&entry.projection);
        if (!projection || projection->cppCanonicalIdentity != selected.cppCanonicalIdentity ||
            projection->moduleName != selected.moduleName || projection->name != selected.name) continue;
        if (match) { error = "selected C function has multiple matching observations"; return false; }
        match = &entry;
    }
    if (!match) { error = "selected C function has no matching observation"; return false; }
    CDependencyFunctionSnapshot value;
    value.moduleName = selected.moduleName;
    value.referenceIdentity = std::string(referenceIdentity);
    value.linkSymbol = selected.mangledName;
    value.declaration = match->declaration;
    value.declaration.location.reset();
    if (const auto* projection = std::get_if<frontend::NamespaceFunctionSymbol>(&match->projection))
        value.microsoftVolatileSemantics = projection->cppMicrosoftVolatileSemantics;
    output = std::move(value);
    error.clear();
    return true;
}

bool encodeCDependencyFunction(const CDependencyFunctionSnapshot& value,
    std::vector<std::uint8_t>& output, std::string& error)
{
    const auto& f = value.declaration;
    if (value.moduleName.empty() || value.referenceIdentity.empty() || value.linkSymbol.empty() ||
        f.name.empty() || f.qualifiedName.empty() || f.callingConvention.empty() ||
        (f.isCStaticInlineDefinition ?
            (f.languageLinkage != CppLanguageLinkage::None || f.externalLinkage) :
            (f.languageLinkage != CppLanguageLinkage::C && f.languageLinkage != CppLanguageLinkage::Cpp)) || f.parameters.size() > maxItems)
    { error = "invalid C dependency function identity"; return false; }
    ByteWriter writer;
    writer.u8(1);
    if (!putText(writer, value.moduleName) || !putText(writer, value.referenceIdentity) ||
        !putText(writer, value.linkSymbol) || !putText(writer, f.name) || !putText(writer, f.qualifiedName) ||
        !putText(writer, f.canonicalFunctionType) || !putText(writer, f.callingConvention) ||
        !putText(writer, f.returnTypeSpelling) || !putText(writer, f.canonicalReturnTypeSpelling) ||
        !putOptionalText(writer, f.externalSymbol) || !putOptionalText(writer, f.clangUsr))
    { error = "invalid C dependency function text"; return false; }
    writer.u8(f.variadic); writer.u8(f.externalLinkage); writer.u8(f.isNonThrowing);
    writer.u8(f.dllImport); writer.u8(f.hasPrototype);
    writer.u8(f.isCStaticInlineDefinition ? 1 : f.languageLinkage == CppLanguageLinkage::Cpp ? 2 : 0);
    std::optional<std::string> variadicBase;
    if (value.variadicBasePlan)
    {
        if (!f.variadic || !f.hasPrototype || !f.externalLinkage || f.callingConvention != "sysv_abi" ||
            !validateCppVariadicSysVCallPlan(*value.variadicBasePlan, value.linkSymbol, f.parameters.size()) ||
            value.variadicBasePlan->arguments.size() != f.parameters.size() ||
            !(variadicBase = abi_plan::encodeVariadicCallPlanForCache(*value.variadicBasePlan)))
        { error = "invalid C variadic base plan"; return false; }
    }
    if (!putOptionalText(writer, variadicBase)) { error = "C variadic base plan is too large"; return false; }
    // EN: T08-D13b. Volatile mode of the reference: 0 unknown, 1 ISO, 2 Microsoft.
    // ES: T08-D13b. Modo volatile de la referencia: 0 desconocido, 1 ISO, 2 Microsoft.
    writer.u8(!value.microsoftVolatileSemantics ? 0 : *value.microsoftVolatileSemantics ? 2 : 1);
    const auto putDependencyType = [&](const CppType& type) {
        std::vector<std::uint8_t> encoded;
        if (!encodeCDependencyType(type, encoded, error) ||
            encoded.size() > std::numeric_limits<std::uint32_t>::max()) return false;
        writer.u32(static_cast<std::uint32_t>(encoded.size()));
        writer.raw(encoded);
        return true;
    };
    if (!putDependencyType(f.returnType)) return false;
    writer.u32(static_cast<std::uint32_t>(f.parameters.size()));
    for (const auto& parameter : f.parameters)
    {
        if (!putText(writer, parameter.name) || !putText(writer, parameter.typeSpelling) ||
            !putText(writer, parameter.canonicalTypeSpelling) || !putDependencyType(parameter.type))
        { if (error.empty()) error = "invalid C dependency function parameter"; return false; }
        writer.u8(parameter.hasDefaultArgument);
    }
    // The current contract stores the direct aggregate plans the backend can consume.
    // A missing observation is never interpreted as an ABI classification.
    const bool directRecord = f.abiPlan && abi_plan::isSysVSmallRecordRoundTrip(*f.abiPlan, value.linkSymbol);
    const bool manyRecords = f.abiPlan && abi_plan::isSysVSmallRecordsRoundTrip(*f.abiPlan, value.linkSymbol);
    const bool singleVector = f.abiPlan && abi_plan::isSysVSingleVector2FRecordRoundTrip(*f.abiPlan, value.linkSymbol);
    const bool singleFloating = f.abiPlan && abi_plan::isSysVSingleFloatingRecordRoundTrip(*f.abiPlan, value.linkSymbol);
    const bool singleFloat = singleFloating && f.abiPlan->physicalReturn.kind == abi_plan::TypeKind::Float;
    const bool pairRecord = f.abiPlan && abi_plan::isSysVTwoIntegerRecordRoundTrip(*f.abiPlan, value.linkSymbol);
    const bool pairWithScalars = f.abiPlan && abi_plan::isSysVTwoIntegerRecordWithScalars(*f.abiPlan, value.linkSymbol);
    const bool indirectRecord = f.abiPlan && abi_plan::isSysVIndirectRecordRoundTrip(*f.abiPlan, value.linkSymbol);
    const bool indirectMixed = f.abiPlan && abi_plan::isSysVIndirectRecordWithArguments(*f.abiPlan, value.linkSymbol);
    const bool scalarMixed = f.abiPlan && abi_plan::isSysVScalarReturnWithRecordArguments(*f.abiPlan, value.linkSymbol) &&
        (f.returnType.kind == CppTypeKind::Void || f.returnType.kind == CppTypeKind::Boolean || f.returnType.kind == CppTypeKind::Pointer ||
         f.returnType.kind == CppTypeKind::SignedInteger || f.returnType.kind == CppTypeKind::UnsignedInteger ||
         f.returnType.kind == CppTypeKind::Floating || f.returnType.kind == CppTypeKind::Enumeration) &&
        std::any_of(f.parameters.begin(), f.parameters.end(), [](const auto& parameter) { return parameter.type.kind == CppTypeKind::Record; });
    const bool indirectDetailed = indirectMixed && [&] {
        if (f.parameters.size() != f.abiPlan->logicalParameters.size()) return false;
        for (std::size_t i = 0; i < f.parameters.size(); ++i)
        {
            const auto& logical = f.abiPlan->logicalParameters[i];
            if (f.parameters[i].type.kind != CppTypeKind::Record) continue;
            if (logical.passing == abi_plan::Passing::Indirect && f.parameters[i].type.sizeBits && *f.parameters[i].type.sizeBits <= 128) return true;
            if (logical.passing != abi_plan::Passing::Direct) continue;
            if (logical.physicalCount == 1 && f.abiPlan->physicalParameters[logical.firstPhysical].type.kind != abi_plan::TypeKind::Integer) return true;
            if (logical.physicalCount == 2 && logical.coercion &&
                (logical.coercion->elements[0].kind != abi_plan::TypeKind::Integer ||
                 logical.coercion->elements[1].kind != abi_plan::TypeKind::Integer)) return true;
        }
        return false;
    }();
    const bool mixedRecord = f.abiPlan && abi_plan::isSysVMixedRecordRoundTrip(*f.abiPlan, value.linkSymbol);
    const bool directMixed = f.abiPlan && !pairRecord && !pairWithScalars && !mixedRecord &&
        f.returnType.kind == CppTypeKind::Record && abi_plan::isSysVDirectPairReturnWithArguments(*f.abiPlan, value.linkSymbol);
    const bool directSingleMixed = f.abiPlan && !directRecord && !manyRecords && !singleVector && !singleFloating &&
        f.returnType.kind == CppTypeKind::Record && abi_plan::isSysVDirectSingleReturnWithArguments(*f.abiPlan, value.linkSymbol);
    const bool mixedDoubleFirst = mixedRecord && f.abiPlan->physicalReturn.elements[0].kind == abi_plan::TypeKind::Double;
    const bool mixedFloatFirst = mixedRecord && f.abiPlan->physicalReturn.elements[0].kind == abi_plan::TypeKind::Float;
    const bool mixedBothDouble = mixedDoubleFirst && f.abiPlan->physicalReturn.elements[1].kind == abi_plan::TypeKind::Double;
    const bool mixedFloatSecond = mixedRecord && f.abiPlan->physicalReturn.elements[1].kind == abi_plan::TypeKind::Float;
    const bool mixedDoubleFloat = mixedDoubleFirst && mixedFloatSecond;
    const bool mixedVectorSecond = mixedRecord && f.abiPlan->physicalReturn.elements[1].kind == abi_plan::TypeKind::Vector;
    const bool mixedVectorFirst = mixedRecord && f.abiPlan->physicalReturn.elements[0].kind == abi_plan::TypeKind::Vector;
    const bool mixedBothVectors = mixedVectorFirst && mixedVectorSecond;
    const bool mixedVectorDouble = mixedRecord &&
        ((mixedVectorFirst && f.abiPlan->physicalReturn.elements[1].kind == abi_plan::TypeKind::Double) ||
         (mixedVectorSecond && mixedDoubleFirst));
    const bool mixedVectorFirstFloat = mixedVectorFirst && mixedFloatSecond;
    const bool mixedVectorFirstI32 = mixedVectorFirst && f.abiPlan->physicalReturn.elements[1].widthOrCount == 32;
    writer.u8(directSingleMixed ? 26 : directMixed ? 25 : scalarMixed ? 24 : indirectDetailed ? 23 : indirectMixed ? 22 : pairWithScalars ? 21 : manyRecords ? 20 : directRecord ? 1 : singleVector ? 12 : singleFloating ? (singleFloat ? 13 : 14) : pairRecord ? 2 : indirectRecord ? 3 :
              mixedRecord ? (mixedVectorDouble ? (mixedVectorFirst ? 18 : 19) : mixedFloatFirst ? 17 : mixedBothDouble ? 15 : mixedDoubleFloat ? 16 : mixedDoubleFirst ? 4 : mixedVectorFirstFloat ? 11 : mixedFloatSecond ? 6 : mixedBothVectors ? 10 : mixedVectorSecond ? 7 :
                             mixedVectorFirst ? (mixedVectorFirstI32 ? 9 : 8) : 5) : 0);
    if (directRecord || manyRecords || singleVector || singleFloating || pairRecord || pairWithScalars || indirectRecord || indirectMixed || scalarMixed || directMixed || directSingleMixed || mixedRecord)
    {
        const auto secondBits = (pairRecord || pairWithScalars) ? f.abiPlan->physicalReturn.elements[1].widthOrCount : 0;
        const auto bits = singleVector ? 64 : singleFloating ? (singleFloat ? 32 : 64) : mixedRecord ? ((mixedFloatSecond && !mixedDoubleFloat) || mixedVectorFirstI32 ? 96 : 128) : (indirectRecord || indirectMixed) ? f.returnType.sizeBits.value_or(0) :
                          (pairRecord || pairWithScalars) ? 64 + secondBits : f.abiPlan->physicalReturn.widthOrCount;
        if (f.callingConvention != "sysv_abi" || f.variadic || !f.hasPrototype ||
            f.externalSymbol != value.linkSymbol ||
            (scalarMixed ? !((f.returnType.kind == CppTypeKind::Void && f.abiPlan->physicalReturn.kind == abi_plan::TypeKind::Void) ||
                (f.returnType.kind == CppTypeKind::Pointer && f.abiPlan->physicalReturn.kind == abi_plan::TypeKind::Pointer) ||
                (f.returnType.kind == CppTypeKind::Boolean && f.abiPlan->physicalReturn.kind == abi_plan::TypeKind::Integer &&
                 f.abiPlan->physicalReturn.widthOrCount == 1 && f.abiPlan->returnZeroExtend) ||
                (f.returnType.kind == CppTypeKind::Enumeration && f.returnType.enumUnderlying && f.returnType.sizeBits == 32 &&
                 f.returnType.enumUnderlying->sizeBits == 32 &&
                 (f.returnType.enumUnderlying->kind == CppTypeKind::SignedInteger || f.returnType.enumUnderlying->kind == CppTypeKind::UnsignedInteger) &&
                 f.abiPlan->physicalReturn.kind == abi_plan::TypeKind::Integer && f.abiPlan->physicalReturn.widthOrCount == 32) ||
                ((f.returnType.kind == CppTypeKind::SignedInteger || f.returnType.kind == CppTypeKind::UnsignedInteger) &&
                 f.abiPlan->physicalReturn.kind == abi_plan::TypeKind::Integer && f.returnType.sizeBits == f.abiPlan->physicalReturn.widthOrCount &&
                 ((f.returnType.sizeBits == 32 || f.returnType.sizeBits == 64) ||
                  (f.abiPlan->returnSignExtend == (f.returnType.kind == CppTypeKind::SignedInteger) &&
                   f.abiPlan->returnZeroExtend == (f.returnType.kind == CppTypeKind::UnsignedInteger)))) ||
                (f.returnType.kind == CppTypeKind::Floating &&
                 ((f.returnType.sizeBits == 32 && f.abiPlan->physicalReturn.kind == abi_plan::TypeKind::Float) ||
                  (f.returnType.sizeBits == 64 && f.abiPlan->physicalReturn.kind == abi_plan::TypeKind::Double)))) :
                           directMixed ? (f.returnType.kind != CppTypeKind::Record || !f.returnType.sizeBits || !f.returnType.alignmentBits ||
                               !abi_plan::isSysVDirectPairLayout(f.abiPlan->physicalReturn, *f.returnType.sizeBits, *f.returnType.alignmentBits)) :
                           directSingleMixed ? (f.returnType.kind != CppTypeKind::Record || !f.returnType.sizeBits || !f.returnType.alignmentBits ||
                               !abi_plan::isSysVDirectSingleRecordLayout(f.abiPlan->physicalReturn, *f.returnType.sizeBits, *f.returnType.alignmentBits)) :
                           (f.returnType.kind != CppTypeKind::Record || f.returnType.sizeBits != bits)) ||
            f.parameters.size() != ((manyRecords || pairWithScalars || indirectMixed || scalarMixed || directMixed || directSingleMixed) ? f.abiPlan->logicalParameters.size() : 1U) ||
            (!manyRecords && !pairWithScalars && !indirectMixed && !scalarMixed && !directMixed && !directSingleMixed && (f.parameters[0].type.kind != CppTypeKind::Record || f.parameters[0].type.sizeBits != bits)) ||
            (manyRecords && [&] {
                bool hasRecord = false;
                for (std::size_t i = 0; i < f.parameters.size(); ++i)
                {
                    if (f.parameters[i].type.kind == CppTypeKind::Record) hasRecord = true;
                    if (!((f.parameters[i].type.kind == CppTypeKind::Record &&
                           f.parameters[i].type.sizeBits == f.abiPlan->physicalParameters[i].type.widthOrCount) ||
                          ((f.parameters[i].type.kind == CppTypeKind::SignedInteger || f.parameters[i].type.kind == CppTypeKind::UnsignedInteger) &&
                           (f.parameters[i].type.sizeBits == 32 || f.parameters[i].type.sizeBits == 64) &&
                           f.abiPlan->physicalParameters[i].type.kind == abi_plan::TypeKind::Integer &&
                           f.abiPlan->physicalParameters[i].type.widthOrCount == f.parameters[i].type.sizeBits) ||
                          (f.parameters[i].type.kind == CppTypeKind::Pointer &&
                           f.abiPlan->physicalParameters[i].type.kind == abi_plan::TypeKind::Pointer &&
                           f.abiPlan->physicalParameters[i].type.widthOrCount == 0) ||
                          (f.parameters[i].type.kind == CppTypeKind::Floating &&
                           ((f.parameters[i].type.sizeBits == 32 && f.abiPlan->physicalParameters[i].type.kind == abi_plan::TypeKind::Float) ||
                            (f.parameters[i].type.sizeBits == 64 && f.abiPlan->physicalParameters[i].type.kind == abi_plan::TypeKind::Double))))) return true;
                }
                return !hasRecord;
            }()) ||
            (pairWithScalars && [&] {
                for (std::size_t i = 0; i < f.parameters.size(); ++i)
                {
                    const auto& type = f.parameters[i].type;
                    const auto physicalIndex = f.abiPlan->logicalParameters[i].firstPhysical;
                    const auto kind = f.abiPlan->physicalParameters[physicalIndex].type.kind;
                    if (!((type.kind == CppTypeKind::Record &&
                           ((f.abiPlan->logicalParameters[i].physicalCount == 2 &&
                             type.sizeBits == 64 + f.abiPlan->physicalParameters[physicalIndex + 1].type.widthOrCount) ||
                            (f.abiPlan->logicalParameters[i].physicalCount == 1 &&
                             type.sizeBits == f.abiPlan->physicalParameters[physicalIndex].type.widthOrCount))) ||
                          ((type.kind == CppTypeKind::SignedInteger || type.kind == CppTypeKind::UnsignedInteger) &&
                           (type.sizeBits == 32 || type.sizeBits == 64) && kind == abi_plan::TypeKind::Integer &&
                           type.sizeBits == f.abiPlan->physicalParameters[physicalIndex].type.widthOrCount) ||
                          (type.kind == CppTypeKind::Pointer && kind == abi_plan::TypeKind::Pointer) ||
                          (type.kind == CppTypeKind::Floating && ((type.sizeBits == 32 && kind == abi_plan::TypeKind::Float) ||
                                                                 (type.sizeBits == 64 && kind == abi_plan::TypeKind::Double))))) return true;
                }
                return false;
            }()) ||
            ((indirectMixed || scalarMixed || directMixed || directSingleMixed) && [&] {
                if (indirectMixed && (bits == 0 || bits % 8 != 0 || f.returnType.alignmentBits != f.abiPlan->logicalReturn.alignment * 8)) return true;
                for (std::size_t i = 0; i < f.parameters.size(); ++i)
                {
                    const auto& type = f.parameters[i].type;
                    const auto& logical = f.abiPlan->logicalParameters[i];
                    const auto& physical = f.abiPlan->physicalParameters[logical.firstPhysical].type;
                    if (logical.passing == abi_plan::Passing::Indirect)
                    {
                        if (type.kind != CppTypeKind::Record || !type.sizeBits || *type.sizeBits == 0 ||
                            *type.sizeBits % 8 != 0 || !type.alignmentBits || *type.alignmentBits == 0 || *type.alignmentBits % 8 != 0 ||
                            logical.alignment != (*type.alignmentBits < 64 ? 8 : *type.alignmentBits / 8)) return true;
                        continue;
                    }
                    if (!((type.kind == CppTypeKind::Record &&
                           ((logical.physicalCount == 1 && type.sizeBits && type.alignmentBits &&
                             abi_plan::isSysVDirectSingleRecordLayout(physical, *type.sizeBits, *type.alignmentBits)) ||
                            (logical.physicalCount == 2 && logical.coercion && type.sizeBits && type.alignmentBits &&
                             abi_plan::isSysVDirectPairLayout(*logical.coercion, *type.sizeBits, *type.alignmentBits)))) ||
                          ((type.kind == CppTypeKind::SignedInteger || type.kind == CppTypeKind::UnsignedInteger) &&
                           physical.kind == abi_plan::TypeKind::Integer && type.sizeBits == physical.widthOrCount &&
                           ((type.sizeBits == 32 || type.sizeBits == 64) ||
                            (logical.passing == abi_plan::Passing::Extend &&
                             logical.signExtend == (type.kind == CppTypeKind::SignedInteger) &&
                             logical.zeroExtend == (type.kind == CppTypeKind::UnsignedInteger)))) ||
                          (type.kind == CppTypeKind::Pointer && physical.kind == abi_plan::TypeKind::Pointer) ||
                          (type.kind == CppTypeKind::Boolean && physical.kind == abi_plan::TypeKind::Integer && physical.widthOrCount == 1 &&
                           logical.passing == abi_plan::Passing::Extend && logical.zeroExtend) ||
                          (type.kind == CppTypeKind::Enumeration && type.enumUnderlying && type.sizeBits == 32 && type.enumUnderlying->sizeBits == 32 &&
                           (type.enumUnderlying->kind == CppTypeKind::SignedInteger || type.enumUnderlying->kind == CppTypeKind::UnsignedInteger) &&
                           physical.kind == abi_plan::TypeKind::Integer && physical.widthOrCount == 32) ||
                          (type.kind == CppTypeKind::Floating && ((type.sizeBits == 32 && physical.kind == abi_plan::TypeKind::Float) ||
                                                                 (type.sizeBits == 64 && physical.kind == abi_plan::TypeKind::Double))))) return true;
                }
                return false;
            }()) ||
            (indirectRecord && (bits == 0 || bits % 8 != 0 ||
                f.returnType.alignmentBits != f.abiPlan->logicalReturn.alignment * 8 ||
                f.parameters[0].type.alignmentBits != f.abiPlan->logicalReturn.alignment * 8)) ||
            (singleVector && (f.returnType.alignmentBits != 32 || f.parameters[0].type.alignmentBits != 32)) ||
            (singleFloating && (f.returnType.alignmentBits != bits || f.parameters[0].type.alignmentBits != bits)) ||
            (mixedRecord && ((f.returnType.alignmentBits != (!mixedVectorDouble && ((mixedFloatSecond && !mixedDoubleFloat) || mixedVectorSecond || mixedVectorFirst) ? 32 : 64) &&
                              !(mixedBothVectors && f.returnType.alignmentBits == 128)) ||
                             f.parameters[0].type.alignmentBits != f.returnType.alignmentBits)))
        { error = "invalid C dependency SysV record ABI"; return false; }
        if (indirectRecord || indirectMixed) writer.u64(f.abiPlan->logicalReturn.alignment);
        else if (!scalarMixed && !directMixed && !directSingleMixed && !mixedRecord && !singleVector && !singleFloating)
            writer.u8(static_cast<std::uint8_t>((pairRecord || pairWithScalars) ? secondBits : bits));
        if (!putText(writer, f.abiPlan->targetTriple) || !putText(writer, f.abiPlan->dataLayout))
        { error = "invalid C dependency ABI target"; return false; }
        if (directSingleMixed)
        {
            const auto& physical = f.abiPlan->physicalReturn;
            writer.u8(physical.kind == abi_plan::TypeKind::Integer ? 1 : physical.kind == abi_plan::TypeKind::Double ? 2 :
                physical.kind == abi_plan::TypeKind::Float ? 3 : 4);
            writer.u8(static_cast<std::uint8_t>(physical.widthOrCount));
        }
        if (directMixed)
        {
            for (const auto& physical : f.abiPlan->physicalReturn.elements)
            {
                writer.u8(physical.kind == abi_plan::TypeKind::Integer ? 1 : physical.kind == abi_plan::TypeKind::Double ? 2 :
                    physical.kind == abi_plan::TypeKind::Float ? 3 : 4);
                writer.u8(static_cast<std::uint8_t>(physical.widthOrCount));
            }
        }
        if (indirectDetailed || scalarMixed || directMixed || directSingleMixed)
        {
            for (std::size_t i = 0; i < f.parameters.size(); ++i)
            {
                const auto& logical = f.abiPlan->logicalParameters[i];
                const bool pair = f.parameters[i].type.kind == CppTypeKind::Record && logical.passing == abi_plan::Passing::Direct && logical.physicalCount == 2;
                const bool single = f.parameters[i].type.kind == CppTypeKind::Record && logical.passing == abi_plan::Passing::Direct &&
                    logical.physicalCount == 1 && f.abiPlan->physicalParameters[logical.firstPhysical].type.kind != abi_plan::TypeKind::Integer;
                const bool indirectSmall = f.parameters[i].type.kind == CppTypeKind::Record && logical.passing == abi_plan::Passing::Indirect &&
                    f.parameters[i].type.sizeBits && *f.parameters[i].type.sizeBits <= 128;
                writer.u8(pair ? 1 : single ? 2 : indirectSmall ? 3 : 0);
                if (!pair && !single) continue;
                for (std::size_t piece = 0; piece < (pair ? 2U : 1U); ++piece)
                {
                    const auto& physical = f.abiPlan->physicalParameters[logical.firstPhysical + piece].type;
                    writer.u8(physical.kind == abi_plan::TypeKind::Integer ? 1 : physical.kind == abi_plan::TypeKind::Double ? 2 :
                        physical.kind == abi_plan::TypeKind::Float ? 3 : 4);
                    writer.u8(static_cast<std::uint8_t>(physical.widthOrCount));
                }
            }
        }
    }
    std::optional<std::string> regCallPlan;
    if (f.abiPlan && f.callingConvention == "regcall")
    {
        const bool expanded = std::any_of(f.abiPlan->logicalParameters.begin(), f.abiPlan->logicalParameters.end(),
            [](const auto& logical) { return logical.passing == abi_plan::Passing::Expand; });
        bool tailMatches = f.parameters.size() == f.abiPlan->logicalParameters.size();
        for (std::size_t i = 0; tailMatches && i < f.parameters.size(); ++i)
        {
            const auto& logical = f.abiPlan->logicalParameters[i];
            if (!expanded && i == 0)
                tailMatches = abi_plan::regCallSameRecord(f.returnType, f.parameters[i].type);
            else if (logical.passing == abi_plan::Passing::Expand)
                tailMatches = abi_plan::regCallRecordMatchesExpansion(f.returnType, f.parameters[i].type, logical);
            else
                tailMatches = logical.firstPhysical < f.abiPlan->physicalParameters.size() &&
                    abi_plan::regCallFloatRecordScalarMatches(f.abiPlan->physicalParameters[logical.firstPhysical].type, f.parameters[i].type);
        }
        const bool validRecordReturn = f.returnType.kind == CppTypeKind::Record && f.returnType.sizeBits &&
            f.returnType.alignmentBits && *f.returnType.sizeBits % 8 == 0 && *f.returnType.alignmentBits % 8 == 0 &&
            f.returnType.recordIdentity && f.returnType.recordIdentity->clangUsr &&
            !f.returnType.recordIdentity->clangUsr->empty() &&
            (abi_plan::isRegCallWin64RecordRoundTrip(*f.abiPlan, value.linkSymbol,
                *f.returnType.sizeBits / 8, *f.returnType.alignmentBits / 8) ||
             abi_plan::isRegCallWin64FloatRecordInterleaved(*f.abiPlan, value.linkSymbol,
                *f.returnType.sizeBits / 8, *f.returnType.alignmentBits / 8));
        const bool validIntReturn = f.returnType.kind == CppTypeKind::SignedInteger &&
            abi_plan::isRegCallWin64FloatRecordsReturningInt(*f.abiPlan, value.linkSymbol) &&
            abi_plan::regCallFloatRecordScalarMatches(f.abiPlan->physicalReturn, f.returnType);
        const bool validDoubleReturn = f.returnType.kind == CppTypeKind::Floating &&
            abi_plan::isRegCallWin64FloatRecordsReturningDouble(*f.abiPlan, value.linkSymbol) &&
            abi_plan::regCallFloatRecordScalarMatches(f.abiPlan->physicalReturn, f.returnType);
        const bool validVoidReturn = f.returnType.kind == CppTypeKind::Void &&
            f.returnType.canonicalSpelling == "void" && !f.returnType.isConst &&
            !f.returnType.isVolatile && !f.returnType.isRestrict &&
            abi_plan::isRegCallWin64FloatRecordsReturningVoid(*f.abiPlan, value.linkSymbol);
        if (f.variadic || !f.hasPrototype || !f.externalLinkage || f.externalSymbol != value.linkSymbol ||
            f.parameters.empty() || (!validRecordReturn && !validIntReturn && !validDoubleReturn && !validVoidReturn) ||
            !tailMatches ||
            !(regCallPlan = abi_plan::encodeFunctionPointerPlanForCache(*f.abiPlan)))
        { error = "invalid C dependency regcall record ABI"; return false; }
    }
    if (!putOptionalText(writer, regCallPlan)) { error = "C dependency regcall plan is too large"; return false; }
    output = writer.takeBytes();
    error.clear();
    return true;
}

bool decodeCDependencyFunction(std::span<const std::uint8_t> bytes,
    CDependencyFunctionSnapshot& output, std::string& error)
{
    ByteReader reader(bytes);
    CDependencyFunctionSnapshot value;
    auto& f = value.declaration;
    std::uint8_t version = 0;
    if (!reader.u8(version) || version != 1 || !reader.text(value.moduleName) ||
        !reader.text(value.referenceIdentity) || !reader.text(value.linkSymbol) || !reader.text(f.name) ||
        !reader.text(f.qualifiedName) || !reader.text(f.canonicalFunctionType) ||
        !reader.text(f.callingConvention) || !reader.text(f.returnTypeSpelling) ||
        !reader.text(f.canonicalReturnTypeSpelling) || !getOptionalText(reader, f.externalSymbol) ||
        !getOptionalText(reader, f.clangUsr) || !getFlag(reader, f.variadic) ||
        !getFlag(reader, f.externalLinkage) || !getFlag(reader, f.isNonThrowing) ||
        !getFlag(reader, f.dllImport) || !getFlag(reader, f.hasPrototype))
    { error = "invalid C dependency function"; return false; }
    std::uint8_t linkageKind = 0;
    if (!reader.u8(linkageKind) || linkageKind > 2)
    { error = "invalid C/C++ dependency function linkage"; return false; }
    f.isCStaticInlineDefinition = linkageKind == 1;
    std::optional<std::string> variadicBase;
    if (!getOptionalText(reader, variadicBase)) { error = "invalid C variadic base plan"; return false; }
    if (variadicBase)
    {
        auto decoded = abi_plan::decodeVariadicCallPlanFromCache(*variadicBase);
        if (!decoded) { error = "invalid C variadic base plan"; return false; }
        value.variadicBasePlan = std::make_shared<const abi_plan::VariadicCallPlan>(std::move(*decoded));
    }
    std::uint8_t volatileMode = 0;
    if (!reader.u8(volatileMode) || volatileMode > 2)
    { error = "invalid C dependency function volatile mode"; return false; }
    if (volatileMode != 0) value.microsoftVolatileSemantics = volatileMode == 2;
    f.languageLinkage = linkageKind == 1 ? CppLanguageLinkage::None :
        linkageKind == 2 ? CppLanguageLinkage::Cpp : CppLanguageLinkage::C;
    const auto getDependencyType = [&](CppType& type) {
        std::uint32_t size = 0;
        if (!reader.u32(size) || size > reader.remaining()) return false;
        std::vector<std::uint8_t> encoded(size);
        return reader.raw(encoded) && decodeCDependencyType(encoded, type, error);
    };
    std::uint32_t count = 0;
    if (!getDependencyType(f.returnType) || !reader.u32(count) || count > maxItems || count > reader.remaining())
    { error = "invalid C dependency function type"; return false; }
    f.parameters.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        CppFunctionParameter parameter;
        if (!reader.text(parameter.name) || !reader.text(parameter.typeSpelling) ||
            !reader.text(parameter.canonicalTypeSpelling) || !getDependencyType(parameter.type) ||
            !getFlag(reader, parameter.hasDefaultArgument))
        { error = "invalid C dependency function parameter"; return false; }
        f.parameters.push_back(std::move(parameter));
    }
    std::uint8_t recordPlan = 0;
    if (!reader.u8(recordPlan) || recordPlan > 26) { error = "invalid C dependency ABI marker"; return false; }
    if (recordPlan != 0)
    {
        auto plan = std::make_shared<abi_plan::Plan>();
        plan->symbol = value.linkSymbol;
        plan->callingConvention = 78;
        std::uint8_t bits = 0;
        std::uint64_t indirectAlignment = 0;
        if (((recordPlan == 3 || recordPlan == 22 || recordPlan == 23) && !reader.u64(indirectAlignment)) ||
            ((recordPlan < 3 || recordPlan == 20 || recordPlan == 21) && !reader.u8(bits)) ||
            !reader.text(plan->targetTriple) || !reader.text(plan->dataLayout))
        { error = "invalid C dependency ABI target"; return false; }
        if (recordPlan == 22 || recordPlan == 23 || recordPlan == 24 || recordPlan == 25 || recordPlan == 26)
        {
            if (recordPlan == 26)
            {
                std::uint8_t kind = 0, width = 0;
                if (!reader.u8(kind) || !reader.u8(width) || kind < 1 || kind > 4 ||
                    ((kind == 2 || kind == 3) && width != 0) || (kind == 4 && width != 2))
                { error = "invalid C dependency return piece descriptor"; return false; }
                plan->physicalReturn = abi_plan::Type{kind == 1 ? abi_plan::TypeKind::Integer : kind == 2 ? abi_plan::TypeKind::Double :
                    kind == 3 ? abi_plan::TypeKind::Float : abi_plan::TypeKind::Vector, width};
                if (kind == 4) plan->physicalReturn.elements.push_back(abi_plan::Type{abi_plan::TypeKind::Float});
                plan->logicalReturn.passing = abi_plan::Passing::Direct;
                plan->logicalReturn.coercion = plan->physicalReturn;
            }
            else if (recordPlan == 25)
            {
                plan->physicalReturn.kind = abi_plan::TypeKind::Structure;
                for (unsigned piece = 0; piece < 2; ++piece)
                {
                    std::uint8_t kind = 0, width = 0;
                    if (!reader.u8(kind) || !reader.u8(width) || kind < 1 || kind > 4 ||
                        ((kind == 2 || kind == 3) && width != 0) || (kind == 4 && width != 2))
                    { error = "invalid C dependency return piece descriptor"; return false; }
                    abi_plan::Type value{kind == 1 ? abi_plan::TypeKind::Integer : kind == 2 ? abi_plan::TypeKind::Double :
                        kind == 3 ? abi_plan::TypeKind::Float : abi_plan::TypeKind::Vector, width};
                    if (kind == 4) value.elements.push_back(abi_plan::Type{abi_plan::TypeKind::Float});
                    plan->physicalReturn.elements.push_back(std::move(value));
                }
                plan->logicalReturn.passing = abi_plan::Passing::Direct;
                plan->logicalReturn.coercion = plan->physicalReturn;
            }
            else if (recordPlan == 24)
            {
                if (f.returnType.kind == CppTypeKind::Void)
                {
                    plan->physicalReturn = abi_plan::Type{abi_plan::TypeKind::Void};
                    plan->logicalReturn.passing = abi_plan::Passing::Ignore;
                }
                else if (f.returnType.kind == CppTypeKind::Pointer)
                {
                    plan->physicalReturn = abi_plan::Type{abi_plan::TypeKind::Pointer};
                    plan->logicalReturn.passing = abi_plan::Passing::Direct;
                    plan->logicalReturn.coercion = plan->physicalReturn;
                }
                else if (f.returnType.kind == CppTypeKind::Boolean)
                {
                    plan->physicalReturn = abi_plan::Type{abi_plan::TypeKind::Integer, 1};
                    plan->logicalReturn.passing = abi_plan::Passing::Extend;
                    plan->logicalReturn.coercion = plan->physicalReturn;
                    plan->returnZeroExtend = plan->logicalReturn.zeroExtend = true;
                }
                else if (f.returnType.kind == CppTypeKind::Enumeration && f.returnType.enumUnderlying &&
                         f.returnType.sizeBits == 32 && f.returnType.enumUnderlying->sizeBits == 32 &&
                         (f.returnType.enumUnderlying->kind == CppTypeKind::SignedInteger ||
                          f.returnType.enumUnderlying->kind == CppTypeKind::UnsignedInteger))
                {
                    plan->physicalReturn = abi_plan::Type{abi_plan::TypeKind::Integer, 32};
                    plan->logicalReturn.passing = abi_plan::Passing::Direct;
                    plan->logicalReturn.coercion = plan->physicalReturn;
                }
                else
                {
                    if (!f.returnType.sizeBits ||
                        !((f.returnType.kind == CppTypeKind::SignedInteger || f.returnType.kind == CppTypeKind::UnsignedInteger ||
                           f.returnType.kind == CppTypeKind::Floating) &&
                          ((*f.returnType.sizeBits == 32 || *f.returnType.sizeBits == 64) ||
                           (f.returnType.kind != CppTypeKind::Floating && (*f.returnType.sizeBits == 8 || *f.returnType.sizeBits == 16)))))
                    { error = "invalid C dependency scalar return"; return false; }
                    plan->physicalReturn = f.returnType.kind == CppTypeKind::Floating ?
                        abi_plan::Type{*f.returnType.sizeBits == 32 ? abi_plan::TypeKind::Float : abi_plan::TypeKind::Double} :
                        abi_plan::Type{abi_plan::TypeKind::Integer, *f.returnType.sizeBits};
                    const bool narrow = f.returnType.kind != CppTypeKind::Floating && *f.returnType.sizeBits < 32;
                    plan->logicalReturn.passing = narrow ? abi_plan::Passing::Extend : abi_plan::Passing::Direct;
                    plan->logicalReturn.coercion = plan->physicalReturn;
                    if (narrow)
                    {
                        plan->returnSignExtend = plan->logicalReturn.signExtend = f.returnType.kind == CppTypeKind::SignedInteger;
                        plan->returnZeroExtend = plan->logicalReturn.zeroExtend = f.returnType.kind == CppTypeKind::UnsignedInteger;
                    }
                }
            }
            else
            {
                plan->physicalReturn.kind = abi_plan::TypeKind::Void;
                plan->logicalReturn.passing = abi_plan::Passing::Indirect;
                plan->logicalReturn.alignment = indirectAlignment;
                abi_plan::PhysicalParameter result;
                result.type.kind = abi_plan::TypeKind::Pointer;
                result.pointee = abi_plan::Type{};
                result.structureReturn = true;
                result.alignment = indirectAlignment;
                plan->physicalParameters.push_back(result);
            }
            for (std::size_t i = 0; i < f.parameters.size(); ++i)
            {
                const auto& type = f.parameters[i].type;
                if (!type.sizeBits) { error = "invalid C dependency indirect argument"; return false; }
                abi_plan::LogicalValue logical;
                logical.firstPhysical = plan->physicalParameters.size();
                bool twoPiece = type.kind == CppTypeKind::Record && *type.sizeBits > 64 && *type.sizeBits <= 128;
                bool indirectSmall = false;
                abi_plan::Type storedPair;
                std::optional<abi_plan::Type> storedSingle;
                if (recordPlan == 23 || recordPlan == 24 || recordPlan == 25 || recordPlan == 26)
                {
                    std::uint8_t shape = 0;
                    if (!reader.u8(shape) || shape > 3 ||
                        (shape == 3 && (type.kind != CppTypeKind::Record || *type.sizeBits == 0 || *type.sizeBits > 128)) ||
                        (shape != 3 && (shape == 1) != twoPiece) ||
                        (shape == 2 && (type.kind != CppTypeKind::Record || *type.sizeBits > 64)))
                    { error = "invalid C dependency indirect pair descriptor"; return false; }
                    indirectSmall = shape == 3;
                    if (indirectSmall) twoPiece = false;
                    if (shape == 1 || shape == 2)
                    {
                        storedPair.kind = abi_plan::TypeKind::Structure;
                        for (unsigned piece = 0; piece < (shape == 1 ? 2U : 1U); ++piece)
                        {
                            std::uint8_t kind = 0, width = 0;
                            if (!reader.u8(kind) || !reader.u8(width) || kind < 1 || kind > 4 ||
                                ((kind == 2 || kind == 3) && width != 0) || (kind == 4 && width != 2))
                            { error = "invalid C dependency indirect piece descriptor"; return false; }
                            abi_plan::Type value{kind == 1 ? abi_plan::TypeKind::Integer : kind == 2 ? abi_plan::TypeKind::Double :
                                kind == 3 ? abi_plan::TypeKind::Float : abi_plan::TypeKind::Vector, width};
                            if (kind == 4) value.elements.push_back(abi_plan::Type{abi_plan::TypeKind::Float});
                            if (shape == 1) storedPair.elements.push_back(std::move(value));
                            else storedSingle = std::move(value);
                        }
                    }
                }
                logical.physicalCount = twoPiece ? 2 : 1;
                abi_plan::PhysicalParameter physical;
                if (type.kind == CppTypeKind::Record && (*type.sizeBits > 128 || indirectSmall))
                {
                    if (!type.alignmentBits || *type.alignmentBits == 0 || *type.alignmentBits % 8 != 0)
                    { error = "invalid C dependency indirect argument alignment"; return false; }
                    logical.passing = abi_plan::Passing::Indirect;
                    logical.byValue = true;
                    logical.alignment = *type.alignmentBits < 64 ? 8 : *type.alignmentBits / 8;
                    physical.type.kind = abi_plan::TypeKind::Pointer;
                    physical.pointee = abi_plan::Type{};
                    physical.byValue = true;
                    physical.alignment = logical.alignment;
                }
                else
                {
                    logical.passing = abi_plan::Passing::Direct;
                    logical.coercion = type.kind == CppTypeKind::Pointer ? abi_plan::Type{abi_plan::TypeKind::Pointer} :
                        type.kind == CppTypeKind::Floating ? abi_plan::Type{*type.sizeBits == 64 ? abi_plan::TypeKind::Double : abi_plan::TypeKind::Float} :
                        type.kind == CppTypeKind::Boolean ? abi_plan::Type{abi_plan::TypeKind::Integer, 1} :
                        abi_plan::Type{abi_plan::TypeKind::Integer, *type.sizeBits};
                    if (storedSingle) logical.coercion = *storedSingle;
                    if (twoPiece)
                    {
                        if (recordPlan == 23 || recordPlan == 24 || recordPlan == 25 || recordPlan == 26) logical.coercion = storedPair;
                        else
                        {
                            logical.coercion = abi_plan::Type{abi_plan::TypeKind::Structure};
                            logical.coercion->elements = {{abi_plan::TypeKind::Integer, 64}, {abi_plan::TypeKind::Integer, *type.sizeBits - 64}};
                        }
                        physical.type = logical.coercion->elements[0];
                    }
                    else physical.type = *logical.coercion;
                    if ((type.kind == CppTypeKind::SignedInteger || type.kind == CppTypeKind::UnsignedInteger) &&
                        (*type.sizeBits == 8 || *type.sizeBits == 16))
                    {
                        logical.passing = abi_plan::Passing::Extend;
                        logical.signExtend = physical.signExtend = type.kind == CppTypeKind::SignedInteger;
                        logical.zeroExtend = physical.zeroExtend = type.kind == CppTypeKind::UnsignedInteger;
                    }
                    if (type.kind == CppTypeKind::Boolean)
                    {
                        logical.passing = abi_plan::Passing::Extend;
                        logical.zeroExtend = physical.zeroExtend = true;
                    }
                }
                plan->logicalParameters.push_back(logical);
                plan->physicalParameters.push_back(physical);
                if (twoPiece)
                {
                    physical.type = logical.coercion->elements[1];
                    plan->physicalParameters.push_back(physical);
                }
            }
            if (!(recordPlan == 26 ? abi_plan::isSysVDirectSingleReturnWithArguments(*plan, value.linkSymbol) :
                  recordPlan == 25 ? abi_plan::isSysVDirectPairReturnWithArguments(*plan, value.linkSymbol) :
                  recordPlan == 24 ? abi_plan::isSysVScalarReturnWithRecordArguments(*plan, value.linkSymbol) :
                                     abi_plan::isSysVIndirectRecordWithArguments(*plan, value.linkSymbol)))
            { error = "invalid C dependency indirect ABI plan"; return false; }
            f.abiPlan = std::move(plan);
        }
        else if (recordPlan == 3)
        {
            plan->physicalReturn.kind = abi_plan::TypeKind::Void;
            plan->logicalReturn.passing = abi_plan::Passing::Indirect;
            plan->logicalReturn.alignment = indirectAlignment;
            abi_plan::LogicalValue logical;
            logical.passing = abi_plan::Passing::Indirect;
            logical.alignment = indirectAlignment < 8 ? 8 : indirectAlignment;
            logical.byValue = true;
            logical.firstPhysical = 1;
            logical.physicalCount = 1;
            plan->logicalParameters.push_back(logical);
            abi_plan::PhysicalParameter result;
            result.type.kind = abi_plan::TypeKind::Pointer;
            result.pointee = abi_plan::Type{};
            result.structureReturn = true;
            result.alignment = indirectAlignment;
            plan->physicalParameters.push_back(result);
            abi_plan::PhysicalParameter argument;
            argument.type.kind = abi_plan::TypeKind::Pointer;
            argument.pointee = abi_plan::Type{};
            argument.byValue = true;
            argument.alignment = indirectAlignment < 8 ? 8 : indirectAlignment;
            plan->physicalParameters.push_back(argument);
            if (!abi_plan::isSysVIndirectRecordRoundTrip(*plan, value.linkSymbol))
            { error = "invalid C dependency ABI plan"; return false; }
            f.abiPlan = std::move(plan);
        }
        else
        {
            abi_plan::Type integer{recordPlan == 5 || recordPlan == 15 || recordPlan == 17 || recordPlan == 18 ? abi_plan::TypeKind::Double :
                                   recordPlan == 6 || recordPlan == 11 || recordPlan == 13 || recordPlan == 16 ? abi_plan::TypeKind::Float :
                                   recordPlan == 14 ? abi_plan::TypeKind::Double :
                                   recordPlan == 7 || recordPlan == 10 || recordPlan == 12 || recordPlan == 19 ? abi_plan::TypeKind::Vector : abi_plan::TypeKind::Integer,
                                   recordPlan == 4 || recordPlan == 9 ? 32U : recordPlan == 7 || recordPlan == 10 || recordPlan == 12 || recordPlan == 19 ? 2U : recordPlan == 8 ? 64U : recordPlan == 20 || recordPlan == 21 ? bits : recordPlan >= 5 ? 0U : bits};
            if (recordPlan == 7 || recordPlan == 10 || recordPlan == 12 || recordPlan == 19) integer.elements.push_back(abi_plan::Type{abi_plan::TypeKind::Float});
            abi_plan::Type firstInteger{recordPlan == 4 || recordPlan == 14 || recordPlan == 15 || recordPlan == 16 || recordPlan == 19 ? abi_plan::TypeKind::Double :
                                        recordPlan == 13 || recordPlan == 17 ? abi_plan::TypeKind::Float :
                                        recordPlan == 8 || recordPlan == 9 || recordPlan == 10 || recordPlan == 11 || recordPlan == 12 || recordPlan == 18 ? abi_plan::TypeKind::Vector : abi_plan::TypeKind::Integer,
                                        recordPlan == 8 || recordPlan == 9 || recordPlan == 10 || recordPlan == 11 || recordPlan == 12 || recordPlan == 18 ? 2U : recordPlan == 13 || recordPlan == 14 || recordPlan == 15 || recordPlan == 16 || recordPlan == 17 || recordPlan == 19 ? 0U : recordPlan == 20 ? bits : recordPlan == 2 || recordPlan >= 5 ? 64U : recordPlan == 4 ? 0U : bits};
            if (recordPlan == 8 || recordPlan == 9 || recordPlan == 10 || recordPlan == 11 || recordPlan == 12 || recordPlan == 18) firstInteger.elements.push_back(abi_plan::Type{abi_plan::TypeKind::Float});
            abi_plan::Type physicalReturn = integer;
            if (recordPlan == 2 || (recordPlan >= 4 && recordPlan <= 11) || (recordPlan >= 15 && recordPlan != 20))
            {
                physicalReturn = {};
                physicalReturn.kind = abi_plan::TypeKind::Structure;
                physicalReturn.elements = {firstInteger, integer};
            }
            plan->physicalReturn = physicalReturn;
            plan->logicalReturn.passing = abi_plan::Passing::Direct;
            plan->logicalReturn.coercion = physicalReturn;
            abi_plan::LogicalValue logical;
            logical.passing = abi_plan::Passing::Direct;
            logical.coercion = physicalReturn;
            if (recordPlan == 20)
            {
                if (f.parameters.size() < 2 || !f.parameters[0].type.sizeBits)
                { error = "invalid C dependency record arguments"; return false; }
                logical.coercion = f.parameters[0].type.kind == CppTypeKind::Pointer ?
                    abi_plan::Type{abi_plan::TypeKind::Pointer} : f.parameters[0].type.kind == CppTypeKind::Floating ?
                    abi_plan::Type{*f.parameters[0].type.sizeBits == 64 ? abi_plan::TypeKind::Double : abi_plan::TypeKind::Float} :
                    abi_plan::Type{abi_plan::TypeKind::Integer, *f.parameters[0].type.sizeBits};
            }
            logical.physicalCount = recordPlan == 2 || (recordPlan >= 4 && recordPlan <= 11) || (recordPlan >= 15 && recordPlan != 20) ? 2 : 1;
            plan->logicalParameters.push_back(std::move(logical));
            abi_plan::PhysicalParameter physical;
            physical.type = firstInteger;
            if (recordPlan == 20) physical.type = *plan->logicalParameters[0].coercion;
            plan->physicalParameters.push_back(physical);
            if (recordPlan == 20)
            {
                for (std::size_t i = 1; i < f.parameters.size(); ++i)
                {
                    if (!f.parameters[i].type.sizeBits) { error = "invalid C dependency record argument"; return false; }
                    auto nextLogical = plan->logicalParameters[0];
                    nextLogical.firstPhysical = i;
                    nextLogical.coercion = f.parameters[i].type.kind == CppTypeKind::Pointer ?
                        abi_plan::Type{abi_plan::TypeKind::Pointer} : f.parameters[i].type.kind == CppTypeKind::Floating ?
                        abi_plan::Type{*f.parameters[i].type.sizeBits == 64 ? abi_plan::TypeKind::Double : abi_plan::TypeKind::Float} :
                        abi_plan::Type{abi_plan::TypeKind::Integer, *f.parameters[i].type.sizeBits};
                    plan->logicalParameters.push_back(std::move(nextLogical));
                    physical.type = *plan->logicalParameters.back().coercion;
                    plan->physicalParameters.push_back(physical);
                }
            }
            else if (recordPlan == 2 || (recordPlan >= 4 && recordPlan <= 11) || recordPlan >= 15) { physical.type = integer; plan->physicalParameters.push_back(physical); }
            if (recordPlan == 21)
            {
                if (f.parameters.size() < 2) { error = "invalid C dependency pair arguments"; return false; }
                plan->logicalParameters.clear();
                plan->physicalParameters.clear();
                for (std::size_t i = 0; i < f.parameters.size(); ++i)
                {
                    const auto& type = f.parameters[i].type;
                    if (!type.sizeBits) { error = "invalid C dependency scalar argument"; return false; }
                    abi_plan::LogicalValue scalar;
                    scalar.passing = abi_plan::Passing::Direct;
                    scalar.firstPhysical = plan->physicalParameters.size();
                    const bool twoPiece = type.kind == CppTypeKind::Record && *type.sizeBits > 64;
                    scalar.physicalCount = twoPiece ? 2 : 1;
                    scalar.coercion = twoPiece ? physicalReturn :
                        type.kind == CppTypeKind::Pointer ? abi_plan::Type{abi_plan::TypeKind::Pointer} :
                        type.kind == CppTypeKind::Floating ? abi_plan::Type{*type.sizeBits == 64 ? abi_plan::TypeKind::Double : abi_plan::TypeKind::Float} :
                        abi_plan::Type{abi_plan::TypeKind::Integer, *type.sizeBits};
                    if (twoPiece)
                    {
                        if (*type.sizeBits <= 64 || *type.sizeBits > 128 || *type.sizeBits % 8 != 0)
                        { error = "invalid C dependency pair argument size"; return false; }
                        scalar.coercion->elements[1].widthOrCount = *type.sizeBits - 64;
                    }
                    plan->logicalParameters.push_back(scalar);
                    abi_plan::PhysicalParameter argument;
                    argument.type = twoPiece ? scalar.coercion->elements[0] : *scalar.coercion;
                    plan->physicalParameters.push_back(argument);
                    if (twoPiece)
                    {
                        argument.type = scalar.coercion->elements[1];
                        plan->physicalParameters.push_back(argument);
                    }
                }
            }
            if (!(recordPlan == 1 ? abi_plan::isSysVSmallRecordRoundTrip(*plan, value.linkSymbol) :
                  recordPlan == 2 ? abi_plan::isSysVTwoIntegerRecordRoundTrip(*plan, value.linkSymbol) :
                  recordPlan == 20 ? abi_plan::isSysVSmallRecordsRoundTrip(*plan, value.linkSymbol) :
                  recordPlan == 21 ? abi_plan::isSysVTwoIntegerRecordWithScalars(*plan, value.linkSymbol) :
                  recordPlan == 12 ? abi_plan::isSysVSingleVector2FRecordRoundTrip(*plan, value.linkSymbol) :
                  recordPlan == 13 || recordPlan == 14 ? abi_plan::isSysVSingleFloatingRecordRoundTrip(*plan, value.linkSymbol) :
                                    abi_plan::isSysVMixedRecordRoundTrip(*plan, value.linkSymbol)))
            { error = "invalid C dependency ABI plan"; return false; }
            f.abiPlan = std::move(plan);
        }
    }
    std::optional<std::string> regCallPlan;
    if (!getOptionalText(reader, regCallPlan)) { error = "invalid C dependency regcall plan"; return false; }
    if (regCallPlan)
    {
        auto decoded = abi_plan::decodeFunctionPointerPlanFromCache(*regCallPlan);
        if (!decoded || f.abiPlan) { error = "invalid C dependency regcall plan"; return false; }
        f.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*decoded));
    }
    std::vector<std::uint8_t> canonical;
    if (!reader.atEnd() || !encodeCDependencyFunction(value, canonical, error) ||
        canonical.size() != bytes.size() || !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
    { error = "noncanonical C dependency function"; return false; }
    output = std::move(value);
    error.clear();
    return true;
}

bool captureCDependencyGlobal(std::span<const CppSemanticGlobalEntry> inventory,
    const frontend::NamespaceStateSymbol& selected, std::size_t referenceIndex,
    std::string_view referenceIdentity, CDependencyGlobalSnapshot& output, std::string& error)
{
    if (!selected.isCppExternal || !selected.cppCGlobal || !selected.cppImportError.empty() ||
        selected.moduleName.empty() || selected.storageName.empty() || !selected.cppClangUsr ||
        selected.cppClangUsr->empty() || referenceIdentity.empty())
    { error = "selected C global has incomplete identity"; return false; }
    const CppSemanticGlobalEntry* match = nullptr;
    for (const auto& entry : inventory)
    {
        const auto& declaration = entry.declaration;
        if (!entry.cLanguage || entry.projectOrdinal != selected.projectOrdinal ||
            entry.referenceIndex != referenceIndex || entry.targetTriple != "x86_64-pc-windows-msvc" ||
            declaration.name != selected.name || declaration.clangUsr != selected.cppClangUsr ||
            declaration.threadLocal != selected.cppThreadLocal ||
            (declaration.dllImport && !selected.cppDllImport) ||
            (!declaration.externalLinkage && selected.cppDllImport) ||
            entry.microsoftVolatileSemantics != selected.cppMicrosoftVolatileSemantics) continue;
        const auto symbol = declaration.externalLinkage ? declaration.externalSymbol :
            std::optional<std::string>{cppCInlineLinkSymbol(entry.projectFile, entry.referenceIndex,
                                                            "global:" + declaration.name)};
        if (!symbol || *symbol != selected.storageName) continue;
        if (match) { error = "selected C global has multiple matching observations"; return false; }
        match = &entry;
    }
    if (!match) { error = "selected C global has no matching observation"; return false; }
    CDependencyGlobalSnapshot value;
    value.moduleName = selected.moduleName;
    value.referenceIdentity = std::string(referenceIdentity);
    value.linkSymbol = selected.storageName;
    value.microsoftVolatileSemantics = selected.cppMicrosoftVolatileSemantics;
    value.effectiveDllImport = selected.cppDllImport;
    value.declaration = match->declaration;
    value.declaration.location.reset();
    output = std::move(value);
    error.clear();
    return true;
}

bool encodeCDependencyGlobal(const CDependencyGlobalSnapshot& value,
    std::vector<std::uint8_t>& output, std::string& error)
{
    const auto& global = value.declaration;
    if (value.moduleName.empty() || value.referenceIdentity.empty() || value.linkSymbol.empty() ||
        global.name.empty() || global.qualifiedName.empty() ||
        (global.dllImport && !value.effectiveDllImport) ||
        (!global.externalLinkage && value.effectiveDllImport))
    { error = "invalid C dependency global identity"; return false; }
    ByteWriter writer;
    writer.u8(2);
    if (!putText(writer, value.moduleName) || !putText(writer, value.referenceIdentity) ||
        !putText(writer, value.linkSymbol) || !putText(writer, global.name) ||
        !putText(writer, global.qualifiedName) || !putOptionalText(writer, global.externalSymbol) ||
        !putOptionalText(writer, global.clangUsr))
    { error = "invalid C dependency global text"; return false; }
    writer.u8(global.externalLinkage); writer.u8(global.threadLocal); writer.u8(global.dllImport);
    writer.u8(value.effectiveDllImport);
    putOptionalNumber(writer, global.declarationAlignmentBits);
    writer.u8(value.microsoftVolatileSemantics.has_value());
    if (value.microsoftVolatileSemantics) writer.u8(*value.microsoftVolatileSemantics);
    std::vector<std::uint8_t> encoded;
    if (!encodeCDependencyType(global.type, encoded, error) ||
        encoded.size() > std::numeric_limits<std::uint32_t>::max()) return false;
    writer.u32(static_cast<std::uint32_t>(encoded.size()));
    writer.raw(encoded);
    output = writer.takeBytes();
    error.clear();
    return true;
}

bool decodeCDependencyGlobal(std::span<const std::uint8_t> bytes,
    CDependencyGlobalSnapshot& output, std::string& error)
{
    ByteReader reader(bytes);
    CDependencyGlobalSnapshot value;
    auto& global = value.declaration;
    std::uint8_t version = 0;
    bool present = false;
    if (!reader.u8(version) || version != 2 || !reader.text(value.moduleName) ||
        !reader.text(value.referenceIdentity) || !reader.text(value.linkSymbol) ||
        !reader.text(global.name) || !reader.text(global.qualifiedName) ||
        !getOptionalText(reader, global.externalSymbol) || !getOptionalText(reader, global.clangUsr) ||
        !getFlag(reader, global.externalLinkage) || !getFlag(reader, global.threadLocal) ||
        !getFlag(reader, global.dllImport) || !getFlag(reader, value.effectiveDllImport) ||
        !getOptionalNumber(reader, global.declarationAlignmentBits) ||
        !getFlag(reader, present))
    { error = "invalid C dependency global"; return false; }
    if (present)
    {
        bool mode = false;
        if (!getFlag(reader, mode)) { error = "invalid C dependency volatile mode"; return false; }
        value.microsoftVolatileSemantics = mode;
    }
    std::uint32_t size = 0;
    if (!reader.u32(size) || size > reader.remaining())
    { error = "invalid C dependency global type size"; return false; }
    std::vector<std::uint8_t> encoded(size);
    if (!reader.raw(encoded) || !decodeCDependencyType(encoded, global.type, error)) return false;
    std::vector<std::uint8_t> canonical;
    if (!reader.atEnd() || !encodeCDependencyGlobal(value, canonical, error) ||
        canonical.size() != bytes.size() || !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
    { error = "noncanonical C dependency global"; return false; }
    output = std::move(value);
    error.clear();
    return true;
}

bool captureCDependencyAlias(std::span<const CppSemanticTypeAliasEntry> inventory,
    const frontend::CppImportedTypeAlias& selected, std::string_view moduleName,
    std::string_view referenceIdentity, CDependencyAliasSnapshot& output, std::string& error)
{
    const auto& alias = selected.declaration;
    if (moduleName.empty() || referenceIdentity.empty() || alias.name.empty() ||
        alias.qualifiedName.empty() || !alias.clangUsr || alias.clangUsr->empty() ||
        selected.targetTriple != "x86_64-pc-windows-msvc")
    { error = "selected C alias has incomplete identity"; return false; }
    std::vector<std::uint8_t> selectedType;
    if (!encodeCDependencyType(alias.underlyingType, selectedType, error)) return false;
    const CppSemanticTypeAliasEntry* match = nullptr;
    for (const auto& entry : inventory)
    {
        const auto& candidate = entry.declaration;
        if (entry.projectOrdinal != selected.projectOrdinal || entry.referenceIndex != selected.referenceIndex ||
            entry.targetTriple != selected.targetTriple || candidate.name != alias.name ||
            candidate.qualifiedName != alias.qualifiedName || candidate.clangUsr != alias.clangUsr ||
            candidate.insideAnonymousNamespace != alias.insideAnonymousNamespace) continue;
        std::vector<std::uint8_t> candidateType;
        if (!encodeCDependencyType(candidate.underlyingType, candidateType, error)) return false;
        if (candidateType != selectedType) continue;
        if (match) { error = "selected C alias has multiple matching observations"; return false; }
        match = &entry;
    }
    if (!match) { error = "selected C alias has no matching observation"; return false; }
    CDependencyAliasSnapshot value;
    value.moduleName = std::string(moduleName);
    value.referenceIdentity = std::string(referenceIdentity);
    value.declaration = match->declaration;
    value.declaration.location.reset();
    output = std::move(value);
    error.clear();
    return true;
}

bool encodeCDependencyAlias(const CDependencyAliasSnapshot& value,
    std::vector<std::uint8_t>& output, std::string& error)
{
    const auto& alias = value.declaration;
    if (value.moduleName.empty() || value.referenceIdentity.empty() ||
        alias.name.empty() || alias.qualifiedName.empty())
    { error = "invalid C dependency alias identity"; return false; }
    ByteWriter writer;
    writer.u8(1);
    if (!putText(writer, value.moduleName) || !putText(writer, value.referenceIdentity) ||
        !putText(writer, alias.name) || !putText(writer, alias.qualifiedName) ||
        !putOptionalText(writer, alias.clangUsr))
    { error = "invalid C dependency alias text"; return false; }
    writer.u8(alias.insideAnonymousNamespace);
    std::vector<std::uint8_t> encoded;
    if (!encodeCDependencyType(alias.underlyingType, encoded, error) ||
        encoded.size() > std::numeric_limits<std::uint32_t>::max()) return false;
    writer.u32(static_cast<std::uint32_t>(encoded.size()));
    writer.raw(encoded);
    std::optional<std::string> pointerPlan;
    if (alias.abiPlan)
    {
        const auto& pointer = alias.underlyingType;
        if (!alias.clangUsr || alias.clangUsr->empty() || pointer.kind != CppTypeKind::Pointer ||
            !pointer.element || !pointer.element->function)
        { error = "invalid C++ dependency alias ABI identity"; return false; }
        const auto& signature = *pointer.element->function;
        const auto& result = signature.returnType;
        const bool recordReturn = result.kind == CppTypeKind::Record && result.recordIdentity &&
            result.recordIdentity->clangUsr && result.sizeBits && *result.sizeBits % 8 == 0 && result.alignmentBits == 32 &&
            abi_plan::isRegCallWin64FloatRecordInterleaved(*alias.abiPlan,
                "__regcall3____nsc_pointer_abi_probe", *result.sizeBits / 8, 4);
        const bool intReturn = result.kind == CppTypeKind::SignedInteger &&
            abi_plan::isRegCallWin64FloatRecordsReturningInt(*alias.abiPlan, "__regcall3____nsc_pointer_abi_probe") &&
            abi_plan::regCallFloatRecordScalarMatches(alias.abiPlan->physicalReturn, result);
        const bool doubleReturn = result.kind == CppTypeKind::Floating &&
            abi_plan::isRegCallWin64FloatRecordsReturningDouble(*alias.abiPlan, "__regcall3____nsc_pointer_abi_probe") &&
            abi_plan::regCallFloatRecordScalarMatches(alias.abiPlan->physicalReturn, result);
        const bool voidReturn = result.kind == CppTypeKind::Void && result.canonicalSpelling == "void" &&
            !result.isConst && !result.isVolatile && !result.isRestrict &&
            abi_plan::isRegCallWin64FloatRecordsReturningVoid(*alias.abiPlan, "__regcall3____nsc_pointer_abi_probe");
        if (!signature.hasPrototype || signature.variadic || signature.callingConvention != "regcall" ||
            signature.parameterTypes.empty() || (!recordReturn && !intReturn && !doubleReturn && !voidReturn) ||
            !abi_plan::regCallFloatRecordArgumentsMatch(*alias.abiPlan, signature.returnType, signature.parameterTypes) ||
            !(pointerPlan = abi_plan::encodeFunctionPointerPlanForCache(*alias.abiPlan)))
        { error = "invalid C++ dependency alias ABI plan"; return false; }
    }
    if (!putOptionalText(writer, pointerPlan)) { error = "C++ dependency alias plan is too large"; return false; }
    output = writer.takeBytes();
    error.clear();
    return true;
}

bool decodeCDependencyAlias(std::span<const std::uint8_t> bytes,
    CDependencyAliasSnapshot& output, std::string& error)
{
    ByteReader reader(bytes);
    CDependencyAliasSnapshot value;
    auto& alias = value.declaration;
    std::uint8_t version = 0;
    std::uint32_t size = 0;
    if (!reader.u8(version) || version != 1 || !reader.text(value.moduleName) ||
        !reader.text(value.referenceIdentity) || !reader.text(alias.name) ||
        !reader.text(alias.qualifiedName) || !getOptionalText(reader, alias.clangUsr) ||
        !getFlag(reader, alias.insideAnonymousNamespace) || !reader.u32(size) || size > reader.remaining())
    { error = "invalid C dependency alias"; return false; }
    std::vector<std::uint8_t> encoded(size);
    if (!reader.raw(encoded) || !decodeCDependencyType(encoded, alias.underlyingType, error)) return false;
    std::optional<std::string> pointerPlan;
    if (!getOptionalText(reader, pointerPlan)) { error = "invalid C++ dependency alias plan"; return false; }
    if (pointerPlan)
    {
        auto plan = abi_plan::decodeFunctionPointerPlanFromCache(*pointerPlan);
        if (!plan) { error = "invalid C++ dependency alias plan"; return false; }
        alias.abiPlan = std::make_shared<const abi_plan::Plan>(std::move(*plan));
    }
    std::vector<std::uint8_t> canonical;
    if (!reader.atEnd() || !encodeCDependencyAlias(value, canonical, error) ||
        canonical.size() != bytes.size() || !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
    { error = "noncanonical C dependency alias"; return false; }
    output = std::move(value);
    error.clear();
    return true;
}

static bool encodeCDependencyEnumerator(const CDependencyCatalog::Enumerator& value,
    std::vector<std::uint8_t>& output, std::string& error)
{
    if (value.moduleName.empty() || value.referenceIdentity.empty() || value.name.empty() ||
        value.clangUsr.empty() || value.typeKey.empty())
    { error = "invalid C dependency enumerator identity"; return false; }
    ByteWriter writer;
    writer.u8(1);
    if (!putText(writer, value.moduleName) || !putText(writer, value.referenceIdentity) ||
        !putText(writer, value.name) || !putText(writer, value.clangUsr) || !putText(writer, value.typeKey))
    { error = "invalid C dependency enumerator text"; return false; }
    writer.u64(value.bits);
    output = writer.takeBytes(); error.clear(); return true;
}

static bool decodeCDependencyEnumerator(std::span<const std::uint8_t> bytes,
    CDependencyCatalog::Enumerator& output, std::string& error)
{
    ByteReader reader(bytes);
    CDependencyCatalog::Enumerator value;
    std::uint8_t version = 0;
    if (!reader.u8(version) || version != 1 || !reader.text(value.moduleName) ||
        !reader.text(value.referenceIdentity) || !reader.text(value.name) ||
        !reader.text(value.clangUsr) || !reader.text(value.typeKey) || !reader.u64(value.bits))
    { error = "invalid C dependency enumerator"; return false; }
    std::vector<std::uint8_t> canonical;
    if (!reader.atEnd() || !encodeCDependencyEnumerator(value, canonical, error) ||
        canonical.size() != bytes.size() || !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
    { error = "noncanonical C dependency enumerator"; return false; }
    output = std::move(value); error.clear(); return true;
}

bool encodeCDependencyCatalog(const CDependencyCatalog& catalog,
    std::vector<std::uint8_t>& output, std::string& error)
{
    ByteWriter writer;
    writer.u8(catalog.enumerators.empty() ? 1 : 2);
    const auto writeGroup = [&](const auto& values, auto encode) {
        if (values.size() > maxItems) return false;
        std::vector<std::vector<std::uint8_t>> items;
        items.reserve(values.size());
        for (const auto& value : values)
        {
            std::vector<std::uint8_t> bytes;
            if (!encode(value, bytes, error) || bytes.size() > std::numeric_limits<std::uint32_t>::max()) return false;
            items.push_back(std::move(bytes));
        }
        std::sort(items.begin(), items.end());
        if (std::adjacent_find(items.begin(), items.end()) != items.end())
        { error = "duplicate C dependency snapshot"; return false; }
        writer.u32(static_cast<std::uint32_t>(items.size()));
        for (const auto& bytes : items)
        {
            writer.u32(static_cast<std::uint32_t>(bytes.size()));
            writer.raw(bytes);
        }
        return true;
    };
    if (!writeGroup(catalog.functions, encodeCDependencyFunction) ||
        !writeGroup(catalog.globals, encodeCDependencyGlobal) ||
        !writeGroup(catalog.aliases, encodeCDependencyAlias) ||
        (!catalog.enumerators.empty() && !writeGroup(catalog.enumerators, encodeCDependencyEnumerator)))
    { if (error.empty()) error = "invalid C dependency catalogue"; return false; }
    output = writer.takeBytes();
    error.clear();
    return true;
}

bool decodeCDependencyCatalog(std::span<const std::uint8_t> bytes,
    CDependencyCatalog& output, std::string& error)
{
    ByteReader reader(bytes);
    CDependencyCatalog catalog;
    std::uint8_t version = 0;
    if (!reader.u8(version) || (version != 1 && version != 2))
    { error = "invalid C dependency catalogue version"; return false; }
    const auto readGroup = [&](auto& values, auto decode) {
        std::uint32_t count = 0;
        if (!reader.u32(count) || count > maxItems || count > reader.remaining() / 4) return false;
        values.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            std::uint32_t size = 0;
            if (!reader.u32(size) || size > reader.remaining()) return false;
            std::vector<std::uint8_t> encoded(size);
            typename std::decay_t<decltype(values)>::value_type value;
            if (!reader.raw(encoded) || !decode(encoded, value, error)) return false;
            values.push_back(std::move(value));
        }
        return true;
    };
    if (!readGroup(catalog.functions, decodeCDependencyFunction) ||
        !readGroup(catalog.globals, decodeCDependencyGlobal) ||
        !readGroup(catalog.aliases, decodeCDependencyAlias) ||
        (version == 2 && !readGroup(catalog.enumerators, decodeCDependencyEnumerator)))
    { if (error.empty()) error = "invalid C dependency catalogue"; return false; }
    std::vector<std::uint8_t> canonical;
    if (!reader.atEnd() || !encodeCDependencyCatalog(catalog, canonical, error) ||
        canonical.size() != bytes.size() || !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
    { error = "noncanonical C dependency catalogue"; return false; }
    output = std::move(catalog);
    error.clear();
    return true;
}

bool encodeCTypeCatalogWithDependencies(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, const CDependencyCatalog& dependencies,
    std::vector<std::uint8_t>& output, std::string& error)
{
    if (dependencies.functions.empty() && dependencies.globals.empty() && dependencies.aliases.empty() && dependencies.enumerators.empty())
    { error = "empty C dependency catalogue"; return false; }
    std::vector<std::uint8_t> types;
    std::vector<std::uint8_t> declarations;
    if (!encodeCTypeCatalogWithProvenance(records, enums, types, error) ||
        !encodeCDependencyCatalog(dependencies, declarations, error)) return false;
    ByteWriter writer;
    writer.u32(5);
    writer.u64(static_cast<std::uint64_t>(types.size()));
    writer.raw(types);
    writer.u64(static_cast<std::uint64_t>(declarations.size()));
    writer.raw(declarations);
    output = writer.takeBytes();
    error.clear();
    return true;
}

bool decodeCTypeCatalogWithDependencies(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums,
    CDependencyCatalog& dependencies, std::string& error)
{
    ByteReader reader(bytes);
    std::uint32_t version = 0;
    std::uint64_t size = 0;
    if (!reader.u32(version) || version != 5 || !reader.u64(size) || size > reader.remaining())
    { error = "invalid C dependency envelope"; return false; }
    std::vector<std::uint8_t> types(static_cast<std::size_t>(size));
    std::vector<CppCRecordSnapshot> decodedRecords;
    std::vector<CppCEnumSnapshot> decodedEnums;
    if (!reader.raw(types) || !decodeCTypeCatalogWithProvenance(types, decodedRecords, decodedEnums, error)) return false;
    if (!reader.u64(size) || size > reader.remaining())
    { error = "invalid C dependency envelope size"; return false; }
    std::vector<std::uint8_t> declarations(static_cast<std::size_t>(size));
    CDependencyCatalog decodedDependencies;
    if (!reader.raw(declarations) || !decodeCDependencyCatalog(declarations, decodedDependencies, error)) return false;
    std::vector<std::uint8_t> canonical;
    if (!reader.atEnd() || !encodeCTypeCatalogWithDependencies(decodedRecords, decodedEnums,
            decodedDependencies, canonical, error) || canonical.size() != bytes.size() ||
        !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
    { error = "noncanonical C dependency envelope"; return false; }
    records = std::move(decodedRecords);
    enums = std::move(decodedEnums);
    dependencies = std::move(decodedDependencies);
    error.clear();
    return true;
}

bool decodeCTypeCatalogAndDependencies(std::span<const std::uint8_t> bytes, std::uint16_t formatMinor,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums,
    CDependencyCatalog& dependencies, std::string& error)
{
    std::vector<CppCRecordSnapshot> decodedRecords;
    std::vector<CppCEnumSnapshot> decodedEnums;
    CDependencyCatalog decodedDependencies;
    const bool anonymousEnvelope = bytes.size() >= 4 && bytes[0] == 6 &&
        bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0;
    const bool dependencyEnvelope = bytes.size() >= 4 && bytes[0] == 5 &&
        bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0;
    const bool enumProvenanceEnvelope = bytes.size() >= 4 && bytes[0] == 7 &&
        bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0;
    if (formatMinor >= 49 && enumProvenanceEnvelope)
    {
        if (!decodeCTypeCatalogWithEnumProvenance(bytes, decodedRecords, decodedEnums, decodedDependencies, error))
            return false;
    }
    else if (formatMinor >= 48 && anonymousEnvelope)
    {
        if (!decodeCTypeCatalogWithAnonymousCarriers(bytes, decodedRecords, decodedEnums, decodedDependencies, error))
            return false;
    }
    else if (formatMinor >= 46 && dependencyEnvelope)
    {
        if (!decodeCTypeCatalogWithDependencies(bytes, decodedRecords, decodedEnums, decodedDependencies, error))
            return false;
        if (formatMinor < 47 && !decodedDependencies.enumerators.empty())
        { error = "C enumerator dependencies require metadata 1.47"; return false; }
    }
    else if (!decodeVersionedCTypeCatalog(bytes, decodedRecords, decodedEnums, error, formatMinor))
        return false;
    records = std::move(decodedRecords);
    enums = std::move(decodedEnums);
    dependencies = std::move(decodedDependencies);
    error.clear();
    return true;
}

bool encodeCTypeCatalogWithAnonymousCarriers(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, const CDependencyCatalog& dependencies,
    std::vector<std::uint8_t>& output, std::string& error)
{
    std::vector<std::pair<std::uint32_t, std::uint32_t>> carriers;
    if (records.size() > maxItems) { error = "too many C records"; return false; }
    for (std::size_t r = 0; r < records.size(); ++r)
    {
        if (records[r].fields.size() > maxItems) { error = "too many C fields"; return false; }
        for (std::size_t f = 0; f < records[r].fields.size(); ++f)
        {
            const auto& field = records[r].fields[f];
            if (!field.isAnonymousCarrier) continue;
            if (field.name != "__nsc_c_anonymous_" + std::to_string(f) ||
                field.type.kind != CppTypeKind::Record || !field.type.recordIdentity ||
                field.type.recordIdentity->name != field.name || field.type.recordIdentity->anonymousContext ||
                field.bitWidth || !records[r].complete)
            { error = "invalid anonymous C carrier"; return false; }
            if (carriers.size() == maxItems) { error = "too many anonymous C carriers"; return false; }
            carriers.emplace_back(static_cast<std::uint32_t>(r), static_cast<std::uint32_t>(f));
        }
    }
    if (carriers.empty()) { error = "empty anonymous C carrier envelope"; return false; }
    std::vector<std::uint8_t> base;
    const bool hasDependencies = !dependencies.functions.empty() || !dependencies.globals.empty() ||
        !dependencies.aliases.empty() || !dependencies.enumerators.empty();
    if (hasDependencies ? !encodeCTypeCatalogWithDependencies(records, enums, dependencies, base, error)
                        : !encodeCTypeCatalogWithProvenance(records, enums, base, error)) return false;
    ByteWriter writer;
    writer.u32(6);
    writer.u64(static_cast<std::uint64_t>(base.size()));
    writer.raw(base);
    writer.u32(static_cast<std::uint32_t>(carriers.size()));
    for (const auto& [record, field] : carriers) { writer.u32(record); writer.u32(field); }
    output = writer.takeBytes();
    error.clear();
    return true;
}

bool decodeCTypeCatalogWithAnonymousCarriers(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums,
    CDependencyCatalog& dependencies, std::string& error)
{
    ByteReader reader(bytes);
    std::uint32_t version = 0, count = 0;
    std::uint64_t size = 0;
    if (!reader.u32(version) || version != 6 || !reader.u64(size) || size > reader.remaining())
    { error = "invalid anonymous C carrier envelope"; return false; }
    std::vector<std::uint8_t> base(static_cast<std::size_t>(size));
    std::vector<CppCRecordSnapshot> decodedRecords;
    std::vector<CppCEnumSnapshot> decodedEnums;
    CDependencyCatalog decodedDependencies;
    if (!reader.raw(base) || !decodeCTypeCatalogAndDependencies(base, 47, decodedRecords, decodedEnums,
                                                                  decodedDependencies, error)) return false;
    if (!reader.u32(count) || !count || count > maxItems || count > reader.remaining() / 8)
    { error = "invalid anonymous C carrier count"; return false; }
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::uint32_t record = 0, field = 0;
        if (!reader.u32(record) || !reader.u32(field) || record >= decodedRecords.size() ||
            field >= decodedRecords[record].fields.size() || decodedRecords[record].fields[field].isAnonymousCarrier)
        { error = "invalid anonymous C carrier index"; return false; }
        decodedRecords[record].fields[field].isAnonymousCarrier = true;
    }
    std::vector<std::uint8_t> canonical;
    if (!reader.atEnd() || !encodeCTypeCatalogWithAnonymousCarriers(decodedRecords, decodedEnums,
            decodedDependencies, canonical, error) || canonical.size() != bytes.size() ||
        !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
    { error = "noncanonical anonymous C carrier envelope"; return false; }
    records = std::move(decodedRecords);
    enums = std::move(decodedEnums);
    dependencies = std::move(decodedDependencies);
    error.clear();
    return true;
}

bool encodeCTypeCatalogWithEnumProvenance(std::span<const CppCRecordSnapshot> records,
    std::span<const CppCEnumSnapshot> enums, const CDependencyCatalog& dependencies,
    std::vector<std::uint8_t>& output, std::string& error)
{
    if (enums.size() > maxItems || std::none_of(enums.begin(), enums.end(),
            [](const auto& value) { return value.declaration.sourceProvenance.has_value(); }))
    { error = "empty or oversized C enum provenance"; return false; }
    const bool carriers = std::any_of(records.begin(), records.end(), [](const auto& record) {
        return std::any_of(record.fields.begin(), record.fields.end(), [](const auto& field) { return field.isAnonymousCarrier; });
    });
    const bool deps = !dependencies.functions.empty() || !dependencies.globals.empty() ||
        !dependencies.aliases.empty() || !dependencies.enumerators.empty();
    std::vector<std::uint8_t> base;
    if (carriers ? !encodeCTypeCatalogWithAnonymousCarriers(records, enums, dependencies, base, error) :
        deps ? !encodeCTypeCatalogWithDependencies(records, enums, dependencies, base, error) :
               !encodeCTypeCatalogWithProvenance(records, enums, base, error)) return false;
    ByteWriter writer;
    writer.u32(7);
    writer.u64(static_cast<std::uint64_t>(base.size()));
    writer.raw(base);
    writer.u32(static_cast<std::uint32_t>(enums.size()));
    for (const auto& item : enums)
    {
        const auto& provenance = item.declaration.sourceProvenance;
        writer.u8(provenance.has_value());
        if (!provenance) continue;
        std::string path;
        try
        {
            const auto utf8 = provenance->sourcePath.generic_u8string();
            path.assign(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        }
        catch (const std::exception&) { error = "invalid C enum provenance path encoding"; return false; }
        const auto validHex = [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); };
        if (path.empty() || provenance->bufferSha256.size() != 64 ||
            !std::all_of(provenance->bufferSha256.begin(), provenance->bufferSha256.end(), validHex) ||
            !putText(writer, path) || !putText(writer, provenance->bufferSha256))
        { error = "invalid C enum source provenance"; return false; }
        writer.u32(provenance->byteOffset);
    }
    output = writer.takeBytes(); error.clear(); return true;
}

bool decodeCTypeCatalogWithEnumProvenance(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums,
    CDependencyCatalog& dependencies, std::string& error)
{
    ByteReader reader(bytes);
    std::uint32_t version = 0, count = 0;
    std::uint64_t size = 0;
    if (!reader.u32(version) || version != 7 || !reader.u64(size) || size > reader.remaining())
    { error = "invalid C enum provenance envelope"; return false; }
    std::vector<std::uint8_t> base(static_cast<std::size_t>(size));
    std::vector<CppCRecordSnapshot> decodedRecords;
    std::vector<CppCEnumSnapshot> decodedEnums;
    CDependencyCatalog decodedDependencies;
    if (!reader.raw(base) || !decodeCTypeCatalogAndDependencies(base, 48, decodedRecords, decodedEnums,
                                                                decodedDependencies, error)) return false;
    if (!reader.u32(count) || count != decodedEnums.size())
    { error = "C enum provenance count mismatch"; return false; }
    for (auto& item : decodedEnums)
    {
        bool present = false;
        if (!getFlag(reader, present)) { error = "invalid C enum provenance flag"; return false; }
        if (!present) continue;
        std::string path, digest;
        std::uint32_t offset = 0;
        if (!reader.text(path) || !reader.text(digest) || !reader.u32(offset))
        { error = "truncated C enum provenance"; return false; }
        try
        {
            const std::u8string utf8(reinterpret_cast<const char8_t*>(path.data()), path.size());
            item.declaration.sourceProvenance = CppEnumDeclaration::SourceProvenance{
                std::filesystem::path(utf8), std::move(digest), offset};
        }
        catch (const std::exception&) { error = "invalid C enum provenance path encoding"; return false; }
    }
    std::vector<std::uint8_t> canonical;
    if (!reader.atEnd() || !encodeCTypeCatalogWithEnumProvenance(decodedRecords, decodedEnums,
            decodedDependencies, canonical, error) || canonical.size() != bytes.size() ||
        !std::equal(canonical.begin(), canonical.end(), bytes.begin()))
    { error = "noncanonical C enum provenance envelope"; return false; }
    records = std::move(decodedRecords); enums = std::move(decodedEnums);
    dependencies = std::move(decodedDependencies); error.clear(); return true;
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
        writer.u8(record.plainReplaySyntax);
        writer.u8(record.packedReplaySyntax);
        if (!putCppValueObservation(writer, record.cppValueObservation))
        { error = "invalid C++ record observation identity"; return false; }
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
    auto normalizedLeft = left;
    auto normalized = right;
    // This auxiliary observation controls replay eligibility, not C type identity.
    normalizedLeft.plainReplaySyntax = false;
    normalizedLeft.packedReplaySyntax = false;
    normalized.plainReplaySyntax = false;
    normalized.packedReplaySyntax = false;
    normalized.moduleName = left.moduleName;
    normalized.identityKey = left.identityKey;
    if (!encodeCRecordCatalog(std::span(&normalizedLeft, 1), leftBytes, error) ||
        !encodeCRecordCatalog(std::span(&normalized, 1), rightBytes, error)) return false;
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

bool compareCEnumSourceEvidence(const CppCEnumSnapshot& left, const CppCEnumSnapshot& right,
                                CRecordContractComparison& result, std::string& error,
                                bool allowEquivalentDistinctSources,
                                bool allowBothSourcesMissing)
{
    const auto& a = left.declaration;
    const auto& b = right.declaration;
    if (!a.clangUsr || a.clangUsr->empty() || !b.clangUsr || b.clangUsr->empty())
    { result = CRecordContractComparison::Insufficient; error.clear(); return true; }
    if (a.clangUsr != b.clangUsr)
    { result = CRecordContractComparison::Different; error.clear(); return true; }
    auto normalized = right;
    normalized.identityKey = left.identityKey;
    std::vector<std::uint8_t> firstBytes, secondBytes;
    if (!encodeCTypeCatalog({}, std::span(&left, 1), firstBytes, error) ||
        !encodeCTypeCatalog({}, std::span(&normalized, 1), secondBytes, error)) return false;
    if (firstBytes != secondBytes)
    { result = CRecordContractComparison::Different; error.clear(); return true; }
    if (!a.sourceProvenance || !b.sourceProvenance)
    { result = CRecordContractComparison::Insufficient; error.clear(); return true; }
    const auto& first = *a.sourceProvenance;
    const auto& second = *b.sourceProvenance;
    // Two copies of one live virtual-source observation can be verified against
    // their shared immutable buffer; serialized catalogs never carry this token.
    if (a.transientSourceBuffer && a.transientSourceBuffer == b.transientSourceBuffer &&
        first.sourcePath == second.sourcePath && first.bufferSha256 == second.bufferSha256 &&
        first.byteOffset == second.byteOffset && first.byteOffset < a.transientSourceBuffer->size())
    {
        const auto& buffer = *a.transientSourceBuffer;
        const auto* data = reinterpret_cast<const std::uint8_t*>(buffer.data());
        if (basic::sha256Hex(basic::sha256(std::span(data, buffer.size()))) == first.bufferSha256)
        { result = CRecordContractComparison::Matching; error.clear(); return true; }
    }
    if (allowEquivalentDistinctSources && first.sourcePath.is_absolute() && second.sourcePath.is_absolute() &&
        first.sourcePath != second.sourcePath)
    {
        const auto state = [](const auto& source) {
            std::error_code ec;
            const bool exists = std::filesystem::exists(source.sourcePath, ec);
            if (ec) return -1;
            if (!exists) return 0;
            basic::Sha256Digest digest;
            std::uintmax_t size = 0;
            std::string hashError;
            return basic::sha256File(source.sourcePath, digest, size, hashError) &&
                source.byteOffset < size && basic::sha256Hex(digest) == source.bufferSha256 ? 1 : -1;
        };
        const auto leftState = state(first), rightState = state(second);
        if ((leftState == 0 && rightState == 1) || (leftState == 1 && rightState == 0) ||
            (allowBothSourcesMissing && leftState == 0 && rightState == 0))
        { result = CRecordContractComparison::Matching; error.clear(); return true; }
    }
    if (!allowEquivalentDistinctSources &&
        (first.bufferSha256 != second.bufferSha256 || first.byteOffset != second.byteOffset))
    { result = CRecordContractComparison::Different; error.clear(); return true; }
    std::error_code ec;
    if (!first.sourcePath.is_absolute() || !second.sourcePath.is_absolute() ||
        !std::filesystem::is_regular_file(first.sourcePath, ec) || ec)
    { result = CRecordContractComparison::Insufficient; error.clear(); return true; }
    const bool sameFile = std::filesystem::equivalent(first.sourcePath, second.sourcePath, ec);
    if (ec) { result = CRecordContractComparison::Insufficient; error.clear(); return true; }
    if (sameFile && (first.bufferSha256 != second.bufferSha256 || first.byteOffset != second.byteOffset))
    { result = CRecordContractComparison::Different; error.clear(); return true; }
    if (!sameFile && !allowEquivalentDistinctSources)
    { result = CRecordContractComparison::Different; error.clear(); return true; }
    const auto unchanged = [](const auto& source) {
        basic::Sha256Digest digest;
        std::uintmax_t size = 0;
        std::string hashError;
        return basic::sha256File(source.sourcePath, digest, size, hashError) &&
            source.byteOffset < size && basic::sha256Hex(digest) == source.bufferSha256;
    };
    if (!unchanged(first) || (!sameFile && !unchanged(second)))
    { result = CRecordContractComparison::Insufficient; error.clear(); return true; }
    result = CRecordContractComparison::Matching; error.clear(); return true;
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
        // EN: T08-D16: two incomplete C records with the same tag, kind, scope, target and USR are the same type, as
        //     C makes them compatible across translation units; there is no layout whose source could differ.
        // ES: T08-D16: dos records C incompletos con la misma etiqueta, clase, ambito, destino y USR son el mismo
        //     tipo, como C los hace compatibles entre unidades; no hay un layout cuyo origen pueda diferir.
        if (contract == CRecordContractComparison::Matching)
        { result = contract; error.clear(); return true; }
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
            !getFlag(reader, record.plainReplaySyntax) ||
            !getFlag(reader, record.packedReplaySyntax) ||
            !getCppValueObservation(reader, record.cppValueObservation) ||
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
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums, std::string& error,
    std::uint16_t formatMinor)
{
    if (formatMinor == 42) return decodeCTypeCatalog(bytes, records, enums, error);
    if (formatMinor >= 49 && bytes.size() >= 4 && bytes[0] == 7 &&
        bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0)
    {
        CDependencyCatalog dependencies;
        return decodeCTypeCatalogWithEnumProvenance(bytes, records, enums, dependencies, error);
    }
    if (formatMinor >= 48 && bytes.size() >= 4 && bytes[0] == 6 &&
        bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0)
    {
        CDependencyCatalog dependencies;
        return decodeCTypeCatalogWithAnonymousCarriers(bytes, records, enums, dependencies, error);
    }
    if (formatMinor >= 46 && bytes.size() >= 4 && bytes[0] == 5 &&
        bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0)
    {
        CDependencyCatalog dependencies;
        std::vector<CppCRecordSnapshot> decodedRecords;
        std::vector<CppCEnumSnapshot> decodedEnums;
        if (!decodeCTypeCatalogWithDependencies(bytes, decodedRecords, decodedEnums, dependencies, error)) return false;
        if (formatMinor < 47 && !dependencies.enumerators.empty())
        { error = "C enumerator dependencies require metadata 1.47"; return false; }
        records = std::move(decodedRecords);
        enums = std::move(decodedEnums);
        return true;
    }
    return isCTypeCatalogWithProvenance(bytes) ?
        decodeCTypeCatalogWithProvenance(bytes, records, enums, error) :
        decodeCTypeCatalog(bytes, records, enums, error);
}

bool decodeVersionedCTypeCatalog(std::span<const std::uint8_t> bytes,
    std::vector<CppCRecordSnapshot>& records, std::vector<CppCEnumSnapshot>& enums, std::string& error)
{
    if (bytes.size() < 4 || bytes[0] != 7 || bytes[1] || bytes[2] || bytes[3])
        return isCTypeCatalogWithProvenance(bytes) ?
            decodeCTypeCatalogWithProvenance(bytes, records, enums, error) :
            decodeCTypeCatalog(bytes, records, enums, error);

    std::vector<CppCRecordSnapshot> observedRecords, fallbackRecords;
    std::vector<CppCEnumSnapshot> observedEnums, fallbackEnums;
    CDependencyCatalog dependencies;
    std::string observedError, fallbackError;
    const bool observed = decodeCTypeCatalogWithEnumProvenance(
        bytes, observedRecords, observedEnums, dependencies, observedError);
    const bool fallback = decodeCTypeCatalog(bytes, fallbackRecords, fallbackEnums, fallbackError);
    if (observed && fallback)
    { error = "ambiguous C type catalogue without enclosing metadata"; return false; }
    if (!observed && !fallback)
    { error = observedError; return false; }
    records = observed ? std::move(observedRecords) : std::move(fallbackRecords);
    enums = observed ? std::move(observedEnums) : std::move(fallbackEnums);
    error.clear();
    return true;
}

// EN: T08-D26. Declaration contract of two C records published by different producers, without source evidence:
//     tag, kind, scope, target and USR and, for complete records, size, alignment, `volatile` mode, every field
//     (name, type, offset, bit width and USR) and the floating formats. Two incomplete records with the same tag
//     are the same C type (T08-D16). A complete record and an incomplete one stay distinct.
// ES: T08-D26. Contrato de declaracion de dos records C publicados por productores distintos, sin evidencia de
//     origen: etiqueta, clase, ambito, destino y USR y, en los completos, tamanio, alineacion, modo `volatile`,
//     cada campo (nombre, tipo, desplazamiento, ancho de bits y USR) y los formatos flotantes. Dos records
//     incompletos con la misma etiqueta son el mismo tipo C (T08-D16). Uno completo y uno incompleto no se unen.
static bool compareCRecordDeclarationContracts(const CppCRecordSnapshot& left, const CppCRecordSnapshot& right,
    CRecordContractComparison& result, std::string& error)
{
    if (left.complete != right.complete)
    { result = CRecordContractComparison::Different; error.clear(); return true; }
    if (left.complete) return compareCRecordContracts(left, right, result, error);
    if (left.name != right.name || left.namespaceSegments != right.namespaceSegments ||
        left.containingRecords != right.containingRecords || left.targetTriple != right.targetTriple ||
        left.kind != right.kind || !left.clangUsr || left.clangUsr->empty() || left.clangUsr != right.clangUsr)
    { result = CRecordContractComparison::Different; error.clear(); return true; }
    auto normalized = right;
    normalized.moduleName = left.moduleName;
    normalized.identityKey = left.identityKey;
    std::vector<std::uint8_t> leftBytes, rightBytes;
    if (!encodeCRecordCatalog(std::span(&left, 1), leftBytes, error) ||
        !encodeCRecordCatalog(std::span(&normalized, 1), rightBytes, error)) return false;
    result = leftBytes == rightBytes ? CRecordContractComparison::Matching : CRecordContractComparison::Different;
    error.clear();
    return true;
}

// EN: T08-D26. Declaration contract of two C enums: name, USR, target, underlying type, size, alignment and every
//     enumerator (name, value and type), in order.
// ES: T08-D26. Contrato de declaracion de dos enum C: nombre, USR, destino, tipo subyacente, tamanio, alineacion y
//     cada enumerador (nombre, valor y tipo), en orden.
static bool compareCEnumDeclarationContracts(const CppCEnumSnapshot& left, const CppCEnumSnapshot& right,
    CRecordContractComparison& result, std::string& error)
{
    const auto& a = left.declaration;
    const auto& b = right.declaration;
    if (a.name != b.name || left.targetTriple != right.targetTriple || !a.clangUsr || a.clangUsr->empty() ||
        a.clangUsr != b.clangUsr)
    { result = CRecordContractComparison::Different; error.clear(); return true; }
    auto normalized = right;
    normalized.identityKey = left.identityKey;
    std::vector<std::uint8_t> leftBytes, rightBytes;
    if (!encodeCTypeCatalog({}, std::span(&left, 1), leftBytes, error) ||
        !encodeCTypeCatalog({}, std::span(&normalized, 1), rightBytes, error)) return false;
    result = leftBytes == rightBytes ? CRecordContractComparison::Matching : CRecordContractComparison::Different;
    error.clear();
    return true;
}

bool compareCRecordClosureEvidence(std::span<const CppCRecordSnapshot> leftRecords,
    std::span<const CppCEnumSnapshot> leftEnums, std::string_view leftRoot,
    std::span<const CppCRecordSnapshot> rightRecords, std::span<const CppCEnumSnapshot> rightEnums,
    std::string_view rightRoot, CRecordContractComparison& result, std::string& error,
    bool allowBothSourcesMissing, bool contractsOnly)
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
        if (contractsOnly)
        {
            if (!compareCRecordDeclarationContracts(*a, *b, verdict, error)) return false;
        }
        else if (!compareCRecordSourceEvidence(*a, *b, verdict, error)) return false;
        if (!contractsOnly && verdict != CRecordContractComparison::Matching && a->complete && b->complete &&
            a->sourceProvenance && b->sourceProvenance)
        {
            const auto state = [](const auto& source) {
                if (!source.sourcePath.is_absolute()) return -1;
                std::error_code ec;
                const bool exists = std::filesystem::exists(source.sourcePath, ec);
                if (ec) return -1;
                if (!exists) return 0;
                basic::Sha256Digest digest;
                std::uintmax_t size = 0;
                std::string hashError;
                return basic::sha256File(source.sourcePath, digest, size, hashError) &&
                    source.byteOffset < size && basic::sha256Hex(digest) == source.bufferSha256 ? 1 : -1;
            };
            const auto leftState = state(*a->sourceProvenance), rightState = state(*b->sourceProvenance);
            std::error_code ec;
            const bool bothPresent = leftState == 1 && rightState == 1;
            const bool oneMissing = (leftState == 0 && rightState == 1) || (leftState == 1 && rightState == 0);
            const bool bothMissing = allowBothSourcesMissing && leftState == 0 && rightState == 0;
            const bool differentFiles = bothPresent
                ? !std::filesystem::equivalent(a->sourceProvenance->sourcePath, b->sourceProvenance->sourcePath, ec) && !ec
                : (oneMissing || bothMissing) && a->sourceProvenance->sourcePath != b->sourceProvenance->sourcePath;
            if (differentFiles && (bothPresent || oneMissing || bothMissing) &&
                !compareCRecordContracts(*a, *b, verdict, error)) return false;
        }
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
                    if (contractsOnly ? !compareCEnumDeclarationContracts(*leftEnum, *rightEnum, verdict, error) :
                        !compareCEnumSourceEvidence(*leftEnum, *rightEnum, verdict, error, true)) return false;
                    if (verdict != CRecordContractComparison::Matching) return true;
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
        type.origin = record.cppValueObservation ? frontend::CppImportedTypeOrigin::CppRecord :
            record.kind == CppRecordKind::Union ? frontend::CppImportedTypeOrigin::CUnion :
                                                 frontend::CppImportedTypeOrigin::CStruct;
        if (record.cppValueObservation)
        {
            type.cppValueTraits = std::make_shared<const CppRecordValueTraits>(record.cppValueObservation->traits);
            type.cppValueOperations = std::make_shared<const CppRecordValueOperations>(record.cppValueObservation->operations);
        }
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

bool reconcilePrecompiledCTypes(std::span<const frontend::PrecompiledSyntaxModule> modules,
    const compilation::ProjectGraph& graph, std::map<std::string, std::string>& output, std::string& error)
{
    std::map<std::string, std::size_t> projectByModule;
    for (std::size_t i = 0; i < graph.projects().size(); ++i)
        projectByModule.emplace(graph.projects()[i].moduleName(), i);
    // EN: One catalogue per owning module, sorted by identity as the catalogue codec requires. The same identity in
    //     two catalogues has the same bytes (reconstructPrecompiledCImportedTypes rejects any other case).
    // ES: Un catalogo por modulo duenio, ordenado por identidad como exige el codec. La misma identidad en dos
    //     catalogos tiene los mismos bytes (reconstructPrecompiledCImportedTypes rechaza cualquier otro caso).
    struct Producer
    {
        std::size_t project = 0;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
    };
    std::map<std::string, Producer> producers;
    std::set<std::string> seen;
    for (const auto& module : modules)
    {
        if (module.cppCTypeCatalog.empty()) continue;
        std::vector<CppCRecordSnapshot> records;
        std::vector<CppCEnumSnapshot> enums;
        if (!decodeVersionedCTypeCatalog(module.cppCTypeCatalog, records, enums, error,
                                         module.cppCTypeCatalogFormatMinor)) return false;
        const auto producerFor = [&](const std::string& owner) -> Producer* {
            const auto project = projectByModule.find(owner);
            if (project == projectByModule.end())
            { error = "C type owner is missing or ambiguous: " + owner; return nullptr; }
            auto& producer = producers[owner];
            producer.project = project->second;
            return &producer;
        };
        for (auto& record : records)
        {
            if (!seen.insert(record.identityKey).second) continue;
            auto* producer = producerFor(record.moduleName);
            if (!producer) return false;
            producer->records.push_back(std::move(record));
        }
        for (auto& item : enums)
        {
            const auto separator = item.identityKey.find("::");
            if (separator == std::string::npos || separator == 0)
            { error = "precompiled C enum has invalid identity"; return false; }
            if (!seen.insert(item.identityKey).second) continue;
            auto* producer = producerFor(item.identityKey.substr(0, separator));
            if (!producer) return false;
            producer->enums.push_back(std::move(item));
        }
    }
    std::vector<Producer*> ordered;
    for (auto& [name, producer] : producers)
    {
        const auto byIdentity = [](const auto& left, const auto& right) {
            return left.identityKey < right.identityKey;
        };
        std::sort(producer.records.begin(), producer.records.end(), byIdentity);
        std::sort(producer.enums.begin(), producer.enums.end(), byIdentity);
        ordered.push_back(&producer);
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto* left, const auto* right) {
        return left->project < right->project;
    });
    std::map<std::string, std::string> result;
    for (std::size_t current = 1; current < ordered.size(); ++current)
    {
        const auto& producer = *ordered[current];
        for (const auto& record : producer.records)
        {
            if (!record.clangUsr || record.clangUsr->empty()) continue;
            for (std::size_t earlier = 0; earlier < current && !result.contains(record.identityKey); ++earlier)
            {
                const auto& other = *ordered[earlier];
                for (const auto& candidate : other.records)
                {
                    if (result.contains(candidate.identityKey) || candidate.name != record.name ||
                        candidate.namespaceSegments != record.namespaceSegments ||
                        candidate.containingRecords != record.containingRecords || candidate.kind != record.kind ||
                        candidate.targetTriple != record.targetTriple || candidate.clangUsr != record.clangUsr)
                        continue;
                    CRecordContractComparison verdict{};
                    if (!compareCRecordClosureEvidence(producer.records, producer.enums, record.identityKey,
                            other.records, other.enums, candidate.identityKey, verdict, error, false, true))
                        return false;
                    if (verdict != CRecordContractComparison::Matching) continue;
                    result.emplace(record.identityKey, candidate.identityKey);
                    break;
                }
            }
        }
        for (const auto& item : producer.enums)
        {
            if (!item.declaration.clangUsr || item.declaration.clangUsr->empty()) continue;
            for (std::size_t earlier = 0; earlier < current && !result.contains(item.identityKey); ++earlier)
                for (const auto& candidate : ordered[earlier]->enums)
                {
                    if (result.contains(candidate.identityKey) ||
                        candidate.declaration.clangUsr != item.declaration.clangUsr) continue;
                    CRecordContractComparison verdict{};
                    if (!compareCEnumDeclarationContracts(item, candidate, verdict, error)) return false;
                    if (verdict != CRecordContractComparison::Matching) continue;
                    result.emplace(item.identityKey, candidate.identityKey);
                    break;
                }
        }
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
        if (!decodeVersionedCTypeCatalog(module.cppCTypeCatalog, records, enums, error,
                                         module.cppCTypeCatalogFormatMinor)) return false;
        const auto add = [&](std::string_view identity, std::string_view owner,
                             std::span<const CppCRecordSnapshot> oneRecord,
                             std::span<const CppCEnumSnapshot> oneEnum) {
            const auto project = projectByModule.find(std::string(owner));
            if (project == projectByModule.end() || ambiguousModules.contains(std::string(owner)))
            { error = "C type owner is missing or ambiguous: " + std::string(owner); return false; }
            std::vector<std::uint8_t> canonical;
            std::vector<CppCRecordSnapshot> identityRecords(oneRecord.begin(), oneRecord.end());
            // Replay observations do not change whether two C declarations name
            // the same type. Consumers of the observation must reconcile it separately.
            for (auto& item : identityRecords) { item.plainReplaySyntax = false; item.packedReplaySyntax = false; }
            const bool hasCarriers = std::any_of(oneRecord.begin(), oneRecord.end(), [](const auto& record) {
                return std::any_of(record.fields.begin(), record.fields.end(), [](const auto& field) {
                    return field.isAnonymousCarrier;
                });
            });
            const bool hasEnumProvenance = std::any_of(oneEnum.begin(), oneEnum.end(), [](const auto& item) {
                return item.declaration.sourceProvenance.has_value();
            });
            if (hasEnumProvenance ? !encodeCTypeCatalogWithEnumProvenance(identityRecords, oneEnum, {}, canonical, error) :
                hasCarriers ? !encodeCTypeCatalogWithAnonymousCarriers(identityRecords, oneEnum, {}, canonical, error) :
                              !encodeCTypeCatalogWithProvenance(identityRecords, oneEnum, canonical, error)) return false;
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
            for (auto& type : reconstructed) type.fromPrecompiledCatalog = true;
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
    // EN: T08-D26: an equivalent declaration of another producer names the canonical type of the first one.
    // ES: T08-D26: una declaracion equivalente de otro productor nombra el tipo canonico del primero.
    std::map<std::string, std::string> reconciled;
    if (!reconcilePrecompiledCTypes(modules, graph, reconciled, error)) return false;
    for (auto& type : result)
        if (const auto found = reconciled.find(type.identity.key()); found != reconciled.end())
            type.reconciledIdentityKey = found->second;
    output = std::move(result); error.clear(); return true;
}
}
