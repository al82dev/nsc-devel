#include "nativesharp/Interop/LibraryAbiReport.h"

#include "nativesharp/Interop/LibraryApiDiff.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace nativesharp::interop
{
namespace
{
const char* typeKindName(LibraryMetadataTypeKind kind) noexcept
{
    switch (kind)
    {
        case LibraryMetadataTypeKind::Class: return "class";
        case LibraryMetadataTypeKind::Interface: return "interface";
        case LibraryMetadataTypeKind::Struct: return "struct";
        case LibraryMetadataTypeKind::Enum: return "enum";
        case LibraryMetadataTypeKind::Delegate: return "delegate";
    }
    return "unknown";
}

const char* ownershipName(LibraryMetadataOwnershipKind ownership) noexcept
{
    switch (ownership)
    {
        case LibraryMetadataOwnershipKind::None: return "none";
        case LibraryMetadataOwnershipKind::Owned: return "owned";
        case LibraryMetadataOwnershipKind::Borrowed: return "borrowed";
    }
    return "unknown";
}

const char* passName(LibraryMetadataParameterPassKind pass) noexcept
{
    switch (pass)
    {
        case LibraryMetadataParameterPassKind::Value: return "value";
        case LibraryMetadataParameterPassKind::Ref: return "ref";
        case LibraryMetadataParameterPassKind::Out: return "out";
        case LibraryMetadataParameterPassKind::In: return "in";
    }
    return "unknown";
}

const char* memberKindName(std::uint8_t kind) noexcept
{
    switch (kind)
    {
        case 0U: return "field";
        case 1U: return "event";
        case 2U: return "method";
        case 3U: return "constructor";
        case 4U: return "destructor";
        default: return "unknown";
    }
}

std::string flagsHex(std::uint64_t flags)
{
    std::ostringstream output;
    output << "0x" << std::hex << std::setfill('0') << std::setw(8) << flags;
    return output.str();
}

const char* delegateConvention(const LibraryMetadataType& type) noexcept
{
    if ((type.delegateFlags & 2U) != 0U) return "vectorcall";
    if ((type.delegateFlags & 1U) != 0U) return "win64-c";
    return "managed";
}

const char* memberConvention(const LibraryMetadataMember& member) noexcept
{
    constexpr std::uint32_t kExternC = 1U << 8U;
    return (member.flags & kExternC) != 0U ? "win64-c" : "native-win64";
}

void writeParameters(
    std::span<const LibraryMetadataParameter> parameters,
    std::ostream& output)
{
    for (std::size_t ordinal = 0; ordinal < parameters.size(); ++ordinal)
    {
        const LibraryMetadataParameter& parameter = parameters[ordinal];
        output << "    parameter #" << ordinal
               << " name=" << parameter.name
               << " type=" << parameter.typeKey
               << " pass=" << passName(parameter.passKind)
               << " ownership=" << ownershipName(parameter.ownership)
               << " pack=" << (parameter.isParameterPack ? "yes" : "no")
               << " dyn=" << (parameter.isDynamicTrait ? "yes" : "no");
        if (parameter.ffiLengthParameter != UINT32_MAX)
            output << " length=#" << parameter.ffiLengthParameter
                   << '(' << parameter.ffiLengthParameterName << ')';
        if (parameter.ffiCallbackLifetime != 0U)
            output << " callback="
                   << (parameter.ffiCallbackLifetime == 2U ? "escaping" : "scoped");
        output << '\n';
    }
}

void writeAbiReportHeader(const LibraryMetadata& metadata, std::ostream& output)
{
    output << "NativeSharp ABI report v1\n"
           << "module: " << metadata.moduleName << '\n'
           << "target: " << metadata.targetTriple << '\n'
           << "pointer: size=" << metadata.pointerSize
           << " align=" << metadata.pointerAlignment << '\n'
           << "native-abi: " << metadata.nativeAbiMajor << '.' << metadata.nativeAbiMinor << '\n'
           << "runtime-abi: " << metadata.runtimeAbiMajor << '.' << metadata.runtimeAbiMinor << '\n'
           << "mangling-abi: " << metadata.manglingAbiVersion << '\n'
           << "object-abi: " << metadata.objectAbiVersion << '\n'
           << "delegate-abi: " << metadata.delegateAbiVersion << '\n'
           << "exception-abi: " << metadata.exceptionAbiVersion << '\n'
           << "abi-surface-fingerprint: " << LibraryApiDiffer::abiSurfaceFingerprint(metadata) << '\n'
           << "metadata-abi-fingerprint: "
           << LibraryMetadataCodec::fingerprintHex(metadata.abiFingerprint) << '\n';
}

void writeDelegateType(const LibraryMetadataType& type, std::ostream& output)
{
    if (type.kind != LibraryMetadataTypeKind::Delegate) return;
    output << "    delegate calling-convention=" << delegateConvention(type)
           << " return="
           << (type.delegateReturnDynamicTrait ? "dyn:" : "")
           << type.delegateReturnTypeKey;
    if (!type.delegateErrorTypeKey.empty())
        output << " throws=" << type.delegateErrorTypeKey;
    output << '\n';
    for (std::size_t ordinal = 0; ordinal < type.delegateParameterTypeKeys.size(); ++ordinal)
        output << "      parameter #" << ordinal
               << " type="
               << (ordinal < type.delegateParameterDynamicTraits.size() &&
                           type.delegateParameterDynamicTraits[ordinal] != 0U
                       ? "dyn:"
                       : "")
               << type.delegateParameterTypeKeys[ordinal] << '\n';
}

void writeVariantType(const LibraryMetadataType& type, std::ostream& output)
{
    if ((type.flags & kMetadataTypeVariant) == 0U) return;
    for (std::size_t ordinal = 0; ordinal < type.variantCases.size(); ++ordinal)
    {
        const LibraryMetadataVariantCase& variantCase = type.variantCases[ordinal];
        output << "    variant-case #" << ordinal << " name=" << variantCase.name;
        if (!variantCase.payloadTypeKey.empty())
            output << " payload=" << variantCase.payloadTypeKey;
        output << '\n';
    }
}

void writeTraitType(const LibraryMetadataType& type, std::ostream& output)
{
    for (std::size_t ordinal = 0; ordinal < type.traitMethodSignatures.size(); ++ordinal)
        output << "    trait-slot #" << ordinal
               << " signature=" << type.traitMethodSignatures[ordinal] << '\n';
    if ((type.flags & kMetadataTypeTrait) != 0U)
        output << "    trait-object representation=descriptor-reference"
               << " words=" << static_cast<unsigned>(type.traitObjectWordCount)
               << " object-safe=" <<
                    ((type.traitObjectFlags & kMetadataTraitObjectObjectSafe) != 0U ? "yes" : "no")
               << " null=zero conversion-allocation=none\n";
    for (std::size_t ordinal = 0; ordinal < type.associatedItems.size(); ++ordinal)
    {
        const LibraryMetadataAssociatedItem& item = type.associatedItems[ordinal];
        output << "    associated-item #" << ordinal
               << " kind=" << (item.kind == LibraryMetadataAssociatedItemKind::Type
                       ? "type" : "constant")
               << " name=" << item.name
               << " type=" << (item.typeKey.empty() ? "<required>" : item.typeKey)
               << " default=" << (item.hasDefault ? "yes" : "no");
        if (item.kind == LibraryMetadataAssociatedItemKind::Constant && item.hasDefault)
            output << " bits=" << item.valueBits;
        output << '\n';
    }
}

void writeTypeFields(const LibraryMetadataType& type, std::ostream& output)
{
    std::vector<const LibraryMetadataField*> fields;
    for (const LibraryMetadataField& field : type.fields) fields.push_back(&field);
    std::ranges::sort(fields, [](const LibraryMetadataField* left, const LibraryMetadataField* right)
    {
        return std::tie(left->offset, left->bitOffset, left->name) <
            std::tie(right->offset, right->bitOffset, right->name);
    });
    for (const LibraryMetadataField* field : fields)
    {
        output << "    field " << field->name
               << " type=" << field->typeKey
               << " ownership=" << ownershipName(field->ownership)
               << " offset=" << field->offset
               << " size=" << field->size
               << " align=" << field->alignment
               << " flags=" << flagsHex(field->flags);
        if (field->bitWidth != 0U)
            output << " bit-offset=" << field->bitOffset << " bit-width=" << field->bitWidth;
        output << '\n';
    }
}

void writeType(const LibraryMetadataType& type, std::ostream& output)
{
    output << "  type " << type.identityKey
           << " kind=" << (((type.flags & kMetadataTypeOpaqueResult) != 0U)
                   ? "opaque-result"
                   : ((type.flags & kMetadataTypeTrait) != 0U)
                       ? "trait"
                       : typeKindName(type.kind))
           << " access=" << static_cast<unsigned>(type.accessibility)
           << " flags=" << flagsHex(type.flags)
           << " size=" << type.size
           << " align=" << type.alignment
           << " header=" << type.headerSize
           << " ref-size=" << type.referenceSize
           << " ref-align=" << type.referenceAlignment
           << " stable-id=" << flagsHex(type.stableTypeId) << '\n';
    if (!type.dropGlueEntryName.empty())
        output << "    drop-glue " << type.dropGlueEntryName << '\n';
    if (!type.baseTypeKey.empty()) output << "    base " << type.baseTypeKey << '\n';
    for (const std::string& interfaceKey : type.interfaceTypeKeys)
        output << "    interface " << interfaceKey << '\n';
    if (type.kind == LibraryMetadataTypeKind::Enum)
        output << "    enum-underlying " << type.enumUnderlyingTypeKey << '\n';
    writeDelegateType(type, output);
    writeVariantType(type, output);
    writeTraitType(type, output);
    writeTypeFields(type, output);
}

void writeTypes(const LibraryMetadata& metadata, std::ostream& output)
{
    std::vector<const LibraryMetadataType*> types;
    for (const LibraryMetadataType& type : metadata.types) types.push_back(&type);
    std::ranges::sort(types, {}, [](const LibraryMetadataType* type)
    {
        return type->identityKey;
    });
    output << "types: " << types.size() << '\n';
    for (const LibraryMetadataType* type : types) writeType(*type, output);
}

void writeMembers(const LibraryMetadata& metadata, std::ostream& output)
{
    std::vector<const LibraryMetadataMember*> members;
    for (const LibraryMetadataMember& member : metadata.members) members.push_back(&member);
    std::ranges::sort(members, [](const LibraryMetadataMember* left, const LibraryMetadataMember* right)
    {
        return std::tie(left->declaringTypeKey, left->kind, left->signatureKey) <
            std::tie(right->declaringTypeKey, right->kind, right->signatureKey);
    });
    output << "members: " << members.size() << '\n';
    for (const LibraryMetadataMember* member : members)
    {
        output << "  member " << member->declaringTypeKey << '.' << member->signatureKey
               << " kind=" << memberKindName(member->kind)
               << " access=" << static_cast<unsigned>(member->accessibility)
               << " flags=" << flagsHex(member->flags)
               << " calling-convention=" << memberConvention(*member)
               << " return="
               << ((member->flags & kMetadataMemberDynamicTrait) != 0U ? "dyn:" : "")
               << member->declaredTypeKey
               << " return-ownership=" << ownershipName(member->declaredOwnership)
               << " mangled=" << member->mangledName;
        if (member->ffiUnwindPolicy != 0U)
            output << " unwind=" << (member->ffiUnwindPolicy == 2U ? "abort" : "forbidden");
        if (!member->declaredErrorTypeKey.empty())
            output << " throws=" << member->declaredErrorTypeKey;
        if (!member->cAbiName.empty()) output << " c-name=" << member->cAbiName;
        output << '\n';
        writeParameters(member->parameters, output);
    }
}

void writeDispatchTables(const LibraryMetadata& metadata, std::ostream& output)
{
    std::vector<const LibraryMetadataDispatch*> dispatches;
    for (const LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
        dispatches.push_back(&dispatch);
    std::ranges::sort(dispatches, {}, [](const LibraryMetadataDispatch* dispatch)
    {
        return dispatch->typeKey;
    });
    output << "dispatch-tables: " << dispatches.size() << '\n';
    for (const LibraryMetadataDispatch* dispatch : dispatches)
    {
        output << "  dispatch " << dispatch->typeKey
               << " descriptor=" << dispatch->descriptorName
               << " descriptor-accessor=" << dispatch->descriptorAccessorName
               << " vtable=" << dispatch->vtableName
               << " destruction-entry=" << dispatch->destructionEntryName
               << " slots=" << dispatch->slotSignatures.size() << '\n';
        for (std::size_t ordinal = 0; ordinal < dispatch->slotSignatures.size(); ++ordinal)
        {
            output << "    slot #" << ordinal << " signature=" << dispatch->slotSignatures[ordinal];
            if (ordinal < dispatch->slotCalleeNames.size())
                output << " callee=" << dispatch->slotCalleeNames[ordinal];
            output << '\n';
        }
        for (const LibraryMetadataDispatchInterface& interface : dispatch->interfaces)
            output << "    interface-table " << interface.interfaceTypeKey
                   << " symbol=" << interface.methodTableName
                   << " slots=" << interface.interfaceSignatures.size() << '\n';
    }
}

void writeExports(const LibraryMetadata& metadata, std::ostream& output)
{
    std::vector<const LibraryMetadataExport*> exports;
    for (const LibraryMetadataExport& exported : metadata.exports) exports.push_back(&exported);
    std::ranges::sort(exports, {}, [](const LibraryMetadataExport* exported)
    {
        return exported->cAbiName;
    });
    output << "exports: " << exports.size() << '\n';
    for (const LibraryMetadataExport* exported : exports)
    {
        output << "  export " << exported->cAbiName
               << " calling-convention=win64-c"
               << " owner=" << exported->declaringTypeKey
               << " return=" << exported->returnTypeKey
               << " native=" << exported->nativeMangledName << '\n';
        for (std::size_t ordinal = 0; ordinal < exported->parameterTypeKeys.size(); ++ordinal)
            output << "    parameter #" << ordinal
                   << " type=" << exported->parameterTypeKeys[ordinal] << '\n';
    }
}
}

void LibraryAbiReporter::write(const LibraryMetadata& metadata, std::ostream& output)
{
    writeAbiReportHeader(metadata, output);
    writeTypes(metadata, output);
    writeMembers(metadata, output);
    writeDispatchTables(metadata, output);
    writeExports(metadata, output);
}
}
