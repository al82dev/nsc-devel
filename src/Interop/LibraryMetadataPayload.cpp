#include "nativesharp/Interop/LibraryMetadata.h"
#include "nativesharp/Interop/LibraryMetadataMerger.h"

#include "nativesharp/Basic/ArtifactTransaction.h"
#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Basic/Version.h"
#include "nativesharp/Frontend/EnumUnderlyingType.h"
#include "nativesharp/Compilation/Project.h"
#include "nativesharp/Compilation/ProjectGraph.h"
#include "nativesharp/Compilation/TypeIdentity.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Frontend/ProjectGenericModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectIntegerEvaluator.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Frontend/ProjectObjectLayoutModel.h"
#include "nativesharp/Frontend/ProjectOopMirModel.h"
#include "nativesharp/Language/CallableSemantics.h"
#include "nativesharp/Lexer/TokenKind.h"

#include "Internal/TemplateSourceReplacement.h"
#include "Internal/TemplateSourcePiece.h"
#include "Internal/ByteReader.h"
#include "Internal/ByteWriter.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Internal/LibraryMetadataCodecSupport.h"

namespace nativesharp::interop::metadata_detail
{

void writeStringVector(ByteWriter& writer, const std::vector<std::string>& values)
{
    writer.u32(static_cast<std::uint32_t>(values.size()));
    for (const std::string& value : values) writer.text(value);
}
bool readStringVector(ByteReader& reader, std::vector<std::string>& values)
{
    std::uint32_t count = 0;
    if (!reader.u32(count) || count > kMaximumRecordCount) return false;
    values.clear();
    values.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::string value;
        if (!reader.text(value)) return false;
        values.push_back(std::move(value));
    }
    return true;
}

void encodePayloadPreamble(
    ByteWriter& writer,
    const LibraryMetadata& metadata,
    std::uint8_t legacySourceRevision)
{
    writer.text(metadata.producerVersion);
    writer.text(metadata.moduleName);
    writer.text(metadata.targetTriple);
    if (metadata.formatMinor >= 23) writer.u8(legacySourceRevision);
    if (metadata.formatMinor < 8) return;
    writer.u16(metadata.nativeAbiMajor);
    writer.u16(metadata.nativeAbiMinor);
    writer.u16(metadata.runtimeAbiMajor);
    writer.u16(metadata.runtimeAbiMinor);
    writer.u16(metadata.manglingAbiVersion);
    writer.u16(metadata.objectAbiVersion);
    writer.u16(metadata.delegateAbiVersion);
    writer.u16(metadata.exceptionAbiVersion);
    writer.u32(metadata.managedDelegateCellSize);
}

void encodeTypeFeatures(
    ByteWriter& writer,
    const LibraryMetadata& metadata,
    const LibraryMetadataType& type)
{
    // Minor 2. Un lector de minor 1 no lo espera, asi que se escribe solo
    // cuando el formato que se esta produciendo lo incluye -- la misma
    // condicion que ya usa `ownership`.
    if (metadata.formatMinor >= 2) writer.text(type.enumUnderlyingTypeKey);
    if (metadata.formatMinor >= 5)
    {
        writer.u8(type.delegateFlags);
        writer.text(type.delegateReturnTypeKey);
        writeStringVector(writer, type.delegateParameterTypeKeys);
        if (metadata.formatMinor >= 31)
        {
            writer.u8(type.delegateReturnDynamicTrait ? 1U : 0U);
            writer.u32(static_cast<std::uint32_t>(type.delegateParameterTypeKeys.size()));
            for (std::size_t parameterIndex = 0;
                 parameterIndex < type.delegateParameterTypeKeys.size();
                 ++parameterIndex)
            {
                writer.u8(parameterIndex < type.delegateParameterDynamicTraits.size()
                    ? type.delegateParameterDynamicTraits[parameterIndex]
                    : 0U);
            }
        }
    }
    if (metadata.formatMinor >= 27)
    {
        writer.u32(static_cast<std::uint32_t>(type.variantCases.size()));
        for (const LibraryMetadataVariantCase& variantCase : type.variantCases)
        {
            writer.text(variantCase.name);
            writer.text(variantCase.payloadTypeKey);
        }
    }
    if (metadata.formatMinor >= 28)
        writeStringVector(writer, type.traitMethodSignatures);
    if (metadata.formatMinor >= 29)
    {
        writer.u32(static_cast<std::uint32_t>(type.associatedItems.size()));
        for (const LibraryMetadataAssociatedItem& item : type.associatedItems)
        {
            writer.u8(static_cast<std::uint8_t>(item.kind));
            writer.text(item.name);
            writer.text(item.typeKey);
            writer.u8(item.hasDefault ? 1U : 0U);
            writer.u64(item.valueBits);
        }
    }
    if (metadata.formatMinor >= 31)
    {
        writer.u8(type.traitObjectFlags);
        writer.u8(type.traitObjectWordCount);
    }
}

void encodeTypeFields(
    ByteWriter& writer,
    const LibraryMetadata& metadata,
    const LibraryMetadataType& type)
{
    writer.u32(static_cast<std::uint32_t>(type.fields.size()));
    for (const LibraryMetadataField& field : type.fields)
    {
        writer.text(field.name);
        writer.text(field.typeKey);
        if (metadata.formatMinor >= 1)
            writer.u8(static_cast<std::uint8_t>(field.ownership));
        writer.u64(field.offset);
        writer.u64(field.size);
        writer.u64(field.alignment);
        writer.u32(field.flags);
        if (metadata.formatMinor >= 18)
        {
            writer.u64(field.bitOffset);
            writer.u64(field.bitWidth);
        }
    }
    if (metadata.formatMinor >= 36) writer.text(type.dropGlueEntryName);
}

void encodeTypes(ByteWriter& writer, const LibraryMetadata& metadata)
{
    writer.u32(static_cast<std::uint32_t>(metadata.types.size()));
    for (const LibraryMetadataType& type : metadata.types)
    {
        writer.u8(static_cast<std::uint8_t>(type.kind));
        writer.u8(type.accessibility);
        writer.u16(type.flags);
        writer.text(type.identityKey);
        writer.text(type.fullyQualifiedName);
        writer.text(type.baseTypeKey);
        writeStringVector(writer, type.interfaceTypeKeys);
        writer.u64(type.size);
        writer.u64(type.alignment);
        writer.u64(type.headerSize);
        writer.u64(type.referenceSize);
        writer.u64(type.referenceAlignment);
        writer.u64(type.stableTypeId);
        encodeTypeFeatures(writer, metadata, type);
        encodeTypeFields(writer, metadata, type);
    }
}

void encodeParameter(
    ByteWriter& writer,
    const LibraryMetadata& metadata,
    const LibraryMetadataParameter& parameter)
{
    writer.text(parameter.name);
    writer.text(parameter.typeKey);
    if (metadata.formatMinor >= 1)
        writer.u8(static_cast<std::uint8_t>(parameter.ownership));
    if (metadata.formatMinor >= 4)
        writer.u8(static_cast<std::uint8_t>(parameter.passKind));
    if (metadata.formatMinor >= 24)
        writer.u8(parameter.isParameterPack ? 1U : 0U);
    if (metadata.formatMinor >= 31)
        writer.u8(parameter.isDynamicTrait ? 1U : 0U);
    if (metadata.formatMinor >= 3)
    {
        writer.u8(static_cast<std::uint8_t>(parameter.defaultValueKind));
        writer.u8(parameter.defaultValueFlags);
        writer.u64(parameter.defaultValueBits);
        writer.text(parameter.defaultValueText);
    }
}

void encodeMember(
    ByteWriter& writer,
    const LibraryMetadata& metadata,
    const LibraryMetadataMember& member)
{
    writer.u8(member.kind);
    writer.u8(member.accessibility);
    writer.u32(member.flags);
    writer.text(member.declaringTypeKey);
    writer.text(member.name);
    writer.text(member.signatureKey);
    writer.text(member.declaredTypeKey);
    if (metadata.formatMinor >= 1)
        writer.u8(static_cast<std::uint8_t>(member.declaredOwnership));
    writer.text(member.cAbiName);
    writer.text(member.mangledName);
    writer.u32(static_cast<std::uint32_t>(member.parameters.size()));
    for (const LibraryMetadataParameter& parameter : member.parameters)
        encodeParameter(writer, metadata, parameter);
    writer.u8(member.constantKind);
    writer.u64(member.constantBits);
    if (metadata.formatMinor >= 9) writer.text(member.constantText);
    if (metadata.formatMinor >= 10) writer.text(member.importDllName);
    if (metadata.formatMinor >= 13) writer.text(member.inlineSourceText);
}

void encodeMembersAndTraitImpls(
    ByteWriter& writer,
    const LibraryMetadata& metadata,
    bool includeTraitImpls)
{
    writer.u32(static_cast<std::uint32_t>(metadata.members.size()));
    for (const LibraryMetadataMember& member : metadata.members)
        encodeMember(writer, metadata, member);
    // NSMETA 1.40 (NS304-T08-F05): external trait conformances. The explicit
    // switch lets the ABI projection retain this section for 1.40 after its
    // common payload has been projected through the neutral 1.24 layout.
    if (!includeTraitImpls) return;
    writer.u32(static_cast<std::uint32_t>(metadata.traitImpls.size()));
    for (const LibraryMetadataTraitImpl& traitImpl : metadata.traitImpls)
    {
        writer.text(traitImpl.targetTypeKey);
        writer.text(traitImpl.traitTypeKey);
        writer.u32(static_cast<std::uint32_t>(traitImpl.methods.size()));
        for (const LibraryMetadataMember& member : traitImpl.methods)
            encodeMember(writer, metadata, member);
    }
}

void encodeGenericRecords(ByteWriter& writer, const LibraryMetadata& metadata)
{
    writer.u32(static_cast<std::uint32_t>(metadata.genericTemplates.size()));
    for (const LibraryMetadataGenericTemplate& generic : metadata.genericTemplates)
    {
        writer.u8(generic.kind);
        if (metadata.formatMinor >= 7) writer.u8(generic.accessibility);
        writer.text(generic.canonicalKey);
        writer.text(generic.moduleName);
        writer.text(generic.namespaceName);
        writer.text(generic.ownerName);
        writer.text(generic.name);
        writeStringVector(writer, generic.parameters);
        if (metadata.formatMinor >= 7) writer.text(generic.sourceText);
        if (metadata.formatMinor >= 11) writeStringVector(writer, generic.dependencies);
    }
    writer.u32(static_cast<std::uint32_t>(metadata.genericSpecializations.size()));
    for (const LibraryMetadataGenericSpecialization& specialization :
         metadata.genericSpecializations)
    {
        writer.u8(specialization.kind);
        writer.text(specialization.templateKey);
        writer.text(specialization.specializationKey);
        writer.text(specialization.specializedName);
        writeStringVector(writer, specialization.typeArguments);
    }
    if (metadata.formatMinor >= 12) writeStringVector(writer, metadata.externalTypeKeys);
}

void encodeDispatchRecords(ByteWriter& writer, const LibraryMetadata& metadata)
{
    writer.u32(static_cast<std::uint32_t>(metadata.dispatchTables.size()));
    for (const LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
    {
        writer.text(dispatch.typeKey);
        writer.u64(dispatch.stableTypeId);
        writer.text(dispatch.descriptorName);
        if (metadata.formatMinor >= 6) writer.text(dispatch.descriptorAccessorName);
        writer.text(dispatch.vtableName);
        writer.text(dispatch.destructionEntryName);
        writeStringVector(writer, dispatch.slotSignatures);
        writeStringVector(writer, dispatch.slotCalleeNames);
        writer.u32(static_cast<std::uint32_t>(dispatch.interfaces.size()));
        for (const LibraryMetadataDispatchInterface& interfaceRecord : dispatch.interfaces)
        {
            writer.text(interfaceRecord.interfaceTypeKey);
            writer.u64(interfaceRecord.stableInterfaceTypeId);
            writer.text(interfaceRecord.methodTableName);
            writeStringVector(writer, interfaceRecord.interfaceSignatures);
            writeStringVector(writer, interfaceRecord.implementationSignatures);
            writeStringVector(writer, interfaceRecord.calleeNames);
        }
    }
}

void encodeExportRecords(ByteWriter& writer, const LibraryMetadata& metadata)
{
    writer.u32(static_cast<std::uint32_t>(metadata.exports.size()));
    for (const LibraryMetadataExport& exportRecord : metadata.exports)
    {
        writer.text(exportRecord.cAbiName);
        writer.text(exportRecord.declaringTypeKey);
        writer.text(exportRecord.returnTypeKey);
        writeStringVector(writer, exportRecord.parameterTypeKeys);
        writer.text(exportRecord.nativeMangledName);
    }
}

void encodeNamespaceFunctions(ByteWriter& writer, const LibraryMetadata& metadata)
{
    if (metadata.formatMinor < 21) return;
    writer.u32(static_cast<std::uint32_t>(metadata.namespaceFunctions.size()));
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        writer.u8(function.accessibility);
        writer.u8(function.flags);
        writer.text(function.namespaceName);
        writer.text(function.name);
        writer.text(function.signatureKey);
        writer.text(function.declaredTypeKey);
        writer.u8(static_cast<std::uint8_t>(function.declaredOwnership));
        writer.text(function.mangledName);
        writer.u32(static_cast<std::uint32_t>(function.parameters.size()));
        for (const LibraryMetadataParameter& parameter : function.parameters)
        {
            writer.text(parameter.name);
            writer.text(parameter.typeKey);
            writer.u8(static_cast<std::uint8_t>(parameter.ownership));
            writer.u8(static_cast<std::uint8_t>(parameter.passKind));
            if (metadata.formatMinor >= 24)
                writer.u8(parameter.isParameterPack ? 1U : 0U);
            if (metadata.formatMinor >= 31)
                writer.u8(parameter.isDynamicTrait ? 1U : 0U);
            writer.u8(static_cast<std::uint8_t>(parameter.defaultValueKind));
            writer.u8(parameter.defaultValueFlags);
            writer.u64(parameter.defaultValueBits);
            writer.text(parameter.defaultValueText);
        }
    }
}

void encodeNamespaceStates(ByteWriter& writer, const LibraryMetadata& metadata)
{
    if (metadata.formatMinor < 22) return;
    writer.u32(static_cast<std::uint32_t>(metadata.namespaceStates.size()));
    for (const LibraryMetadataNamespaceState& state : metadata.namespaceStates)
    {
        writer.u8(state.accessibility);
        writer.u8(state.flags);
        writer.u8(static_cast<std::uint8_t>(state.initializerKind));
        writer.text(state.namespaceName);
        writer.text(state.name);
        writer.text(state.declaredTypeKey);
        writer.text(state.storageName);
        writer.text(state.accessorName);
        writer.u64(state.initializerBits);
        writer.text(state.initializerText);
    }
}

void encodeBinaryClosure(ByteWriter& writer, const LibraryMetadata& metadata)
{
    if (metadata.formatMinor < 25) return;
    writer.u32(static_cast<std::uint32_t>(metadata.binaryDependencies.size()));
    for (const LibraryMetadataBinaryDependency& dependency : metadata.binaryDependencies)
    {
        writer.text(dependency.moduleName);
        writer.text(dependency.libraryFileName);
        writer.text(dependency.metadataFileName);
        writer.text(dependency.runtimeFileName);
        writer.raw(dependency.abiFingerprint);
    }
    writer.u32(static_cast<std::uint32_t>(metadata.platformImports.size()));
    for (const LibraryMetadataPlatformImport& importRecord : metadata.platformImports)
    {
        writer.text(importRecord.dllName);
        writer.text(importRecord.symbolName);
    }
}

template <typename Constraints>
void encodeLifetimeConstraints(ByteWriter& writer, const Constraints& constraints)
{
    writer.u32(static_cast<std::uint32_t>(constraints.size()));
    for (const auto& [longer, shorter] : constraints)
    {
        writer.text(longer);
        writer.text(shorter);
    }
}

void encodeLifetimeContracts(ByteWriter& writer, const LibraryMetadata& metadata)
{
    if (metadata.formatMinor < 34) return;
    writer.u32(static_cast<std::uint32_t>(metadata.types.size()));
    for (const LibraryMetadataType& type : metadata.types)
    {
        writeStringVector(writer, type.lifetimeParameters);
        encodeLifetimeConstraints(writer, type.lifetimeOutlivesConstraints);
        writer.u8(type.delegateReferenceReturn ? 1U : 0U);
        writer.text(type.delegateReturnLifetimeName);
        writer.u32(static_cast<std::uint32_t>(type.delegateParameterPassKinds.size()));
        for (const LibraryMetadataParameterPassKind passKind : type.delegateParameterPassKinds)
            writer.u8(static_cast<std::uint8_t>(passKind));
        writeStringVector(writer, type.delegateParameterLifetimeNames);
        writer.u32(static_cast<std::uint32_t>(type.fields.size()));
        for (const LibraryMetadataField& field : type.fields) writer.text(field.lifetimeName);
    }
    writer.u32(static_cast<std::uint32_t>(metadata.members.size()));
    for (const LibraryMetadataMember& member : metadata.members)
    {
        writer.text(member.returnLifetimeName);
        writeStringVector(writer, member.lifetimeParameters);
        encodeLifetimeConstraints(writer, member.lifetimeOutlivesConstraints);
        writer.u32(static_cast<std::uint32_t>(member.parameters.size()));
        for (const LibraryMetadataParameter& parameter : member.parameters)
            writer.text(parameter.lifetimeName);
    }
    writer.u32(static_cast<std::uint32_t>(metadata.namespaceFunctions.size()));
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        writer.text(function.returnLifetimeName);
        writeStringVector(writer, function.lifetimeParameters);
        encodeLifetimeConstraints(writer, function.lifetimeOutlivesConstraints);
        writer.u32(static_cast<std::uint32_t>(function.parameters.size()));
        for (const LibraryMetadataParameter& parameter : function.parameters)
            writer.text(parameter.lifetimeName);
    }
}

template <typename Callable>
void encodeEscapeSummary(ByteWriter& writer, const Callable& callable)
{
    writer.u32(static_cast<std::uint32_t>(callable.parameterEscapeSummary.size()));
    for (const LibraryMetadataEscapeClass item : callable.parameterEscapeSummary)
        writer.u8(static_cast<std::uint8_t>(item));
}

void encodeEscapeContracts(ByteWriter& writer, const LibraryMetadata& metadata)
{
    if (metadata.formatMinor < 35) return;
    writer.u32(static_cast<std::uint32_t>(metadata.members.size()));
    for (const LibraryMetadataMember& member : metadata.members)
        encodeEscapeSummary(writer, member);
    writer.u32(static_cast<std::uint32_t>(metadata.namespaceFunctions.size()));
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
        encodeEscapeSummary(writer, function);
}

void encodeErrorContracts(ByteWriter& writer, const LibraryMetadata& metadata)
{
    if (metadata.formatMinor < 37) return;
    writer.u32(static_cast<std::uint32_t>(metadata.types.size()));
    for (const LibraryMetadataType& type : metadata.types)
        writer.text(type.delegateErrorTypeKey);
    writer.u32(static_cast<std::uint32_t>(metadata.members.size()));
    for (const LibraryMetadataMember& member : metadata.members)
        writer.text(member.declaredErrorTypeKey);
    writer.u32(static_cast<std::uint32_t>(metadata.namespaceFunctions.size()));
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
        writer.text(function.declaredErrorTypeKey);
}

void encodeGenericContracts(ByteWriter& writer, const LibraryMetadata& metadata)
{
    if (metadata.formatMinor < 38) return;
    writer.u32(static_cast<std::uint32_t>(metadata.genericTemplates.size()));
    for (const LibraryMetadataGenericTemplate& generic : metadata.genericTemplates)
    {
        writer.u32(static_cast<std::uint32_t>(generic.parameterContracts.size()));
        for (const LibraryMetadataGenericTemplate::ParameterContract& parameter :
             generic.parameterContracts)
        {
            writer.text(parameter.name);
            writer.u8(parameter.kind);
            writer.u16(parameter.flags);
            writer.text(parameter.constantTypeKey);
            writer.text(parameter.equalityTypeKey);
            writer.text(parameter.associatedProjectionKey);
            writeStringVector(writer, parameter.typeConstraintKeys);
        }
        writeStringVector(writer, generic.lifetimeParameters);
        encodeLifetimeConstraints(writer, generic.lifetimeOutlivesConstraints);
    }
    writer.u32(static_cast<std::uint32_t>(metadata.members.size()));
    for (const LibraryMetadataMember& member : metadata.members)
    {
        writer.u8(member.ffiUnwindPolicy);
        writer.u32(static_cast<std::uint32_t>(member.parameters.size()));
        for (const LibraryMetadataParameter& parameter : member.parameters)
        {
            writer.u32(parameter.ffiLengthParameter);
            writer.text(parameter.ffiLengthParameterName);
            writer.u8(parameter.ffiCallbackLifetime);
        }
    }
}

std::vector<std::uint8_t> encodePayload(
    const LibraryMetadata& metadata,
    std::uint8_t legacySourceRevision,
    bool includeTraitImpls)
{
    ByteWriter writer;
    encodePayloadPreamble(writer, metadata, legacySourceRevision);
    encodeTypes(writer, metadata);
    encodeMembersAndTraitImpls(writer, metadata, includeTraitImpls);
    encodeGenericRecords(writer, metadata);
    encodeDispatchRecords(writer, metadata);
    encodeExportRecords(writer, metadata);

    encodeNamespaceFunctions(writer, metadata);
    encodeNamespaceStates(writer, metadata);
    encodeBinaryClosure(writer, metadata);
    encodeLifetimeContracts(writer, metadata);
    encodeEscapeContracts(writer, metadata);
    encodeErrorContracts(writer, metadata);
    encodeGenericContracts(writer, metadata);
    if (metadata.formatMinor >= 39) writer.u32(metadata.languageFeatureFlags);
    return writer.bytes();
}

void appendCatalogueBytes(
    std::vector<std::uint8_t>& projection,
    const ByteWriter& catalogue)
{
    const std::vector<std::uint8_t>& bytes = catalogue.bytes();
    projection.insert(projection.end(), bytes.begin(), bytes.end());
}

void appendVariantProjection(
    const LibraryMetadata& metadata,
    std::vector<std::uint8_t>& projection)
{
    // The 1.25 private closure stays outside the public ABI projection, while the 1.27 ordered
    // variant catalogue cannot: changing declaration order changes the observable CaseTag value.
    // Append this section only when a variant exists so fingerprints of pre-1.27 models remain
    // byte-for-byte stable.
    const bool hasVariant = std::ranges::any_of(
        metadata.types,
        [](const LibraryMetadataType& type)
        {
            return (type.flags & kMetadataTypeVariant) != 0U;
        });
    if (!hasVariant) return;
    ByteWriter catalogue;
    catalogue.u32(static_cast<std::uint32_t>(metadata.types.size()));
    for (const LibraryMetadataType& type : metadata.types)
    {
        catalogue.text(type.identityKey);
        catalogue.u32(static_cast<std::uint32_t>(type.variantCases.size()));
        for (const LibraryMetadataVariantCase& variantCase : type.variantCases)
        {
            catalogue.text(variantCase.name);
            catalogue.text(variantCase.payloadTypeKey);
        }
    }
    appendCatalogueBytes(projection, catalogue);
}

void appendDropGlueProjection(
    const LibraryMetadata& metadata,
    std::vector<std::uint8_t>& projection)
{
    const bool hasValueDropGlue = metadata.formatMinor >= 36 && std::ranges::any_of(
        metadata.types,
        [](const LibraryMetadataType& type)
        {
            return !type.dropGlueEntryName.empty();
        });
    if (!hasValueDropGlue) return;
    ByteWriter catalogue;
    catalogue.u32(static_cast<std::uint32_t>(metadata.types.size()));
    for (const LibraryMetadataType& type : metadata.types)
    {
        catalogue.text(type.identityKey);
        catalogue.text(type.dropGlueEntryName);
    }
    appendCatalogueBytes(projection, catalogue);
}

void appendTraitProjection(
    const LibraryMetadata& metadata,
    std::vector<std::uint8_t>& projection)
{
    const bool hasTrait = std::ranges::any_of(
        metadata.types,
        [](const LibraryMetadataType& type)
        {
            return (type.flags & kMetadataTypeTrait) != 0U;
        });
    if (!hasTrait) return;
    ByteWriter catalogue;
    catalogue.u32(static_cast<std::uint32_t>(metadata.types.size()));
    for (const LibraryMetadataType& type : metadata.types)
    {
        catalogue.text(type.identityKey);
        writeStringVector(catalogue, type.traitMethodSignatures);
        if (metadata.formatMinor >= 31)
        {
            catalogue.u8(type.traitObjectFlags);
            catalogue.u8(type.traitObjectWordCount);
        }
    }
    appendCatalogueBytes(projection, catalogue);
}

void appendAssociatedItemProjection(
    const LibraryMetadata& metadata,
    std::vector<std::uint8_t>& projection)
{
    const bool hasAssociatedItems = std::ranges::any_of(
        metadata.types,
        [](const LibraryMetadataType& type) { return !type.associatedItems.empty(); });
    if (!hasAssociatedItems) return;
    ByteWriter catalogue;
    catalogue.u32(static_cast<std::uint32_t>(metadata.types.size()));
    for (const LibraryMetadataType& type : metadata.types)
    {
        catalogue.text(type.identityKey);
        catalogue.u32(static_cast<std::uint32_t>(type.associatedItems.size()));
        for (const LibraryMetadataAssociatedItem& item : type.associatedItems)
        {
            catalogue.u8(static_cast<std::uint8_t>(item.kind));
            catalogue.text(item.name);
            catalogue.text(item.typeKey);
            catalogue.u8(item.hasDefault ? 1U : 0U);
            catalogue.u64(item.valueBits);
        }
    }
    appendCatalogueBytes(projection, catalogue);
}

template <typename Callable>
void writeEscapeProjection(ByteWriter& catalogue, const Callable& callable)
{
    catalogue.u32(static_cast<std::uint32_t>(callable.parameterEscapeSummary.size()));
    for (const LibraryMetadataEscapeClass item : callable.parameterEscapeSummary)
        catalogue.u8(static_cast<std::uint8_t>(item));
}

void appendEscapeProjection(
    const LibraryMetadata& metadata,
    std::vector<std::uint8_t>& projection)
{
    if (metadata.formatMinor < 35) return;
    ByteWriter catalogue;
    catalogue.u32(static_cast<std::uint32_t>(metadata.members.size()));
    for (const LibraryMetadataMember& member : metadata.members)
        writeEscapeProjection(catalogue, member);
    catalogue.u32(static_cast<std::uint32_t>(metadata.namespaceFunctions.size()));
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
        writeEscapeProjection(catalogue, function);
    appendCatalogueBytes(projection, catalogue);
}

void appendErrorTypeProjection(
    const LibraryMetadata& metadata,
    std::vector<std::uint8_t>& projection)
{
    if (metadata.formatMinor < 37) return;
    ByteWriter catalogue;
    catalogue.u32(static_cast<std::uint32_t>(metadata.types.size()));
    for (const LibraryMetadataType& type : metadata.types)
    {
        catalogue.text(type.identityKey);
        catalogue.text(type.delegateErrorTypeKey);
    }
    catalogue.u32(static_cast<std::uint32_t>(metadata.members.size()));
    for (const LibraryMetadataMember& member : metadata.members)
    {
        catalogue.text(member.declaringTypeKey);
        catalogue.text(member.signatureKey);
        catalogue.text(member.declaredErrorTypeKey);
    }
    catalogue.u32(static_cast<std::uint32_t>(metadata.namespaceFunctions.size()));
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        catalogue.text(function.namespaceName);
        catalogue.text(function.signatureKey);
        catalogue.text(function.declaredErrorTypeKey);
    }
    appendCatalogueBytes(projection, catalogue);
}

std::vector<std::uint8_t> encodeAbiProjection(const LibraryMetadata& metadata)
{
    LibraryMetadata projected = metadata;
    projected.producerVersion.clear();
    const bool includeTraitImpls =
        projected.formatMinor >= 40 && !projected.traitImpls.empty();
    // Static closure and private imports describe how to link this particular archive; they do
    // not change the public NativeSharp ABI published by the module. Project every current
    // post-1.24 model through 1.24 so the two 1.25 section counts stay outside the public
    // fingerprint when later additive minors (such as 1.26 record flags) are introduced. Type
    // flags are part of every projection, so record/derive policy still changes the public
    // contract while the private binary closure does not leak back into it.
    projected.binaryDependencies.clear();
    projected.platformImports.clear();
    if (projected.formatMinor >= 25) projected.formatMinor = 24;
    std::vector<std::uint8_t> projection =
        encodePayload(projected, kLegacyAbiProjectionSourceRevisionByte, includeTraitImpls);
    appendVariantProjection(metadata, projection);
    appendDropGlueProjection(metadata, projection);
    appendTraitProjection(metadata, projection);
    appendAssociatedItemProjection(metadata, projection);
    appendEscapeProjection(metadata, projection);
    appendErrorTypeProjection(metadata, projection);
    return projection;
}

bool readCount(ByteReader& reader, std::uint32_t& count)
{
    return reader.u32(count) && count <= kMaximumRecordCount;
}

bool decodePayloadPreamble(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    if (!reader.text(metadata.producerVersion) ||
        !reader.text(metadata.moduleName) ||
        !reader.text(metadata.targetTriple))
    {
        error = "truncated metadata identity payload";
        return false;
    }
    std::uint8_t legacySourceRevision = 0U;
    if (metadata.formatMinor >= 23 && !reader.u8(legacySourceRevision))
    {
        error = "truncated metadata source revision byte";
        return false;
    }
    static_cast<void>(legacySourceRevision);
    if (metadata.formatMinor >= 8 &&
        (!reader.u16(metadata.nativeAbiMajor) || !reader.u16(metadata.nativeAbiMinor) ||
         !reader.u16(metadata.runtimeAbiMajor) || !reader.u16(metadata.runtimeAbiMinor) ||
         !reader.u16(metadata.manglingAbiVersion) || !reader.u16(metadata.objectAbiVersion) ||
         !reader.u16(metadata.delegateAbiVersion) || !reader.u16(metadata.exceptionAbiVersion) ||
         !reader.u32(metadata.managedDelegateCellSize)))
    {
        error = "truncated metadata ABI contract";
        return false;
    }

    return true;
}
bool decodeTypeIdentity(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataType& type,
    std::string& error)
{
    std::uint8_t kind = 0;
    if (!reader.u8(kind) || kind > static_cast<std::uint8_t>(LibraryMetadataTypeKind::Delegate) ||
        !reader.u8(type.accessibility) || !reader.u16(type.flags) ||
        !reader.text(type.identityKey) || !reader.text(type.fullyQualifiedName) ||
        !reader.text(type.baseTypeKey) || !readStringVector(reader, type.interfaceTypeKeys) ||
        !reader.u64(type.size) || !reader.u64(type.alignment) ||
        !reader.u64(type.headerSize) || !reader.u64(type.referenceSize) ||
        !reader.u64(type.referenceAlignment) || !reader.u64(type.stableTypeId))
    {
        error = "truncated or invalid metadata type record";
        return false;
    }
    type.kind = static_cast<LibraryMetadataTypeKind>(kind);
    if (metadata.formatMinor >= 2 && !reader.text(type.enumUnderlyingTypeKey))
    {
        error = "truncated or invalid metadata enum underlying type";
        return false;
    }
    return true;
}

bool decodeDelegateType(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataType& type,
    std::string& error)
{
    if (metadata.formatMinor >= 5 &&
        (!reader.u8(type.delegateFlags) ||
         type.delegateFlags > (metadata.formatMinor >= 20 ? 3U : 1U) ||
         !reader.text(type.delegateReturnTypeKey) ||
         !readStringVector(reader, type.delegateParameterTypeKeys)))
    {
        error = "truncated or invalid metadata delegate signature";
        return false;
    }
    if (metadata.formatMinor < 31) return true;

    std::uint8_t returnDynamicTrait = 0;
    std::uint32_t parameterDynamicTraitCount = 0;
    if (!reader.u8(returnDynamicTrait) || returnDynamicTrait > 1U ||
        !readCount(reader, parameterDynamicTraitCount) ||
        parameterDynamicTraitCount != type.delegateParameterTypeKeys.size())
    {
        error = "truncated or invalid metadata delegate dyn signature";
        return false;
    }
    type.delegateReturnDynamicTrait = returnDynamicTrait != 0U;
    type.delegateParameterDynamicTraits.reserve(parameterDynamicTraitCount);
    for (std::uint32_t dynamicIndex = 0;
         dynamicIndex < parameterDynamicTraitCount; ++dynamicIndex)
    {
        std::uint8_t dynamicTrait = 0;
        if (!reader.u8(dynamicTrait) || dynamicTrait > 1U)
        {
            error = "truncated or invalid metadata delegate dyn parameter";
            return false;
        }
        type.delegateParameterDynamicTraits.push_back(dynamicTrait);
    }
    return true;
}

bool decodeVariantCases(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataType& type,
    std::string& error)
{
    if (metadata.formatMinor < 27) return true;

    std::uint32_t variantCaseCount = 0;
    if (!readCount(reader, variantCaseCount))
    {
        error = "invalid metadata variant case count";
        return false;
    }
    type.variantCases.reserve(variantCaseCount);
    for (std::uint32_t caseIndex = 0; caseIndex < variantCaseCount; ++caseIndex)
    {
        LibraryMetadataVariantCase variantCase;
        if (!reader.text(variantCase.name) || !reader.text(variantCase.payloadTypeKey))
        {
            error = "truncated metadata variant case catalogue";
            return false;
        }
        type.variantCases.push_back(std::move(variantCase));
    }
    return true;
}

bool decodeAssociatedItems(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataType& type,
    std::string& error)
{
    if (metadata.formatMinor < 29) return true;

    std::uint32_t associatedCount = 0;
    if (!readCount(reader, associatedCount))
    {
        error = "invalid metadata associated item count";
        return false;
    }
    type.associatedItems.reserve(associatedCount);
    for (std::uint32_t itemIndex = 0; itemIndex < associatedCount; ++itemIndex)
    {
        LibraryMetadataAssociatedItem item;
        std::uint8_t kind = 0;
        std::uint8_t hasDefault = 0;
        if (!reader.u8(kind) ||
            kind > static_cast<std::uint8_t>(LibraryMetadataAssociatedItemKind::Constant) ||
            !reader.text(item.name) || !reader.text(item.typeKey) ||
            !reader.u8(hasDefault) || hasDefault > 1U || !reader.u64(item.valueBits))
        {
            error = "truncated or invalid metadata associated item catalogue";
            return false;
        }
        item.kind = static_cast<LibraryMetadataAssociatedItemKind>(kind);
        item.hasDefault = hasDefault != 0U;
        type.associatedItems.push_back(std::move(item));
    }
    return true;
}

bool decodeTraitType(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataType& type,
    std::string& error)
{
    if (metadata.formatMinor >= 28 &&
        !readStringVector(reader, type.traitMethodSignatures))
    {
        error = "truncated metadata trait method catalogue";
        return false;
    }
    if (!decodeAssociatedItems(reader, metadata, type, error)) return false;
    if (metadata.formatMinor >= 31 &&
        (!reader.u8(type.traitObjectFlags) || !reader.u8(type.traitObjectWordCount)))
    {
        error = "truncated metadata trait-object ABI contract";
        return false;
    }
    return true;
}

bool decodeTypeFields(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataType& type,
    std::string& error)
{
    std::uint32_t fieldCount = 0;
    if (!readCount(reader, fieldCount))
    {
        error = "invalid metadata field count";
        return false;
    }
    type.fields.reserve(fieldCount);
    for (std::uint32_t fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
    {
        LibraryMetadataField field;
        if (!reader.text(field.name) || !reader.text(field.typeKey) ||
            !readOwnership(reader, metadata.formatMinor, field.ownership) ||
            !reader.u64(field.offset) || !reader.u64(field.size) ||
            !reader.u64(field.alignment) || !reader.u32(field.flags))
        {
            error = "truncated or invalid metadata field record";
            return false;
        }
        if (metadata.formatMinor >= 18 &&
            (!reader.u64(field.bitOffset) || !reader.u64(field.bitWidth)))
        {
            error = "truncated or invalid metadata bit-field record";
            return false;
        }
        type.fields.push_back(std::move(field));
    }
    return true;
}

bool decodeType(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataType& type,
    std::string& error)
{
    if (!decodeTypeIdentity(reader, metadata, type, error)) return false;
    if (!decodeDelegateType(reader, metadata, type, error)) return false;
    if (!decodeVariantCases(reader, metadata, type, error)) return false;
    if (!decodeTraitType(reader, metadata, type, error)) return false;
    if (!decodeTypeFields(reader, metadata, type, error)) return false;
    if (metadata.formatMinor >= 36 && !reader.text(type.dropGlueEntryName))
    {
        error = "truncated metadata value drop-glue identity";
        return false;
    }
    return true;
}

bool decodeTypes(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t count = 0;
    if (!readCount(reader, count))
    {
        error = "invalid metadata type count";
        return false;
    }
    metadata.types.clear();
    metadata.types.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataType type;
        if (!decodeType(reader, metadata, type, error)) return false;
        metadata.types.push_back(std::move(type));
    }
    return true;
}

bool decodeParameter(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataParameter& parameter,
    std::string& error)
{
    if (!reader.text(parameter.name) || !reader.text(parameter.typeKey) ||
        !readOwnership(reader, metadata.formatMinor, parameter.ownership))
    {
        error = "truncated or invalid metadata parameter record";
        return false;
    }
    if (metadata.formatMinor >= 4)
    {
        std::uint8_t passKind = 0;
        if (!reader.u8(passKind) ||
            passKind > static_cast<std::uint8_t>(LibraryMetadataParameterPassKind::In))
        {
            error = "truncated or invalid metadata parameter pass kind";
            return false;
        }
        parameter.passKind = static_cast<LibraryMetadataParameterPassKind>(passKind);
    }
    if (metadata.formatMinor >= 24)
    {
        std::uint8_t parameterPack = 0;
        if (!reader.u8(parameterPack) || parameterPack > 1U)
        {
            error = "truncated or invalid metadata parameter-pack flag";
            return false;
        }
        parameter.isParameterPack = parameterPack != 0U;
    }
    if (metadata.formatMinor >= 31)
    {
        std::uint8_t dynamicTrait = 0;
        if (!reader.u8(dynamicTrait) || dynamicTrait > 1U)
        {
            error = "truncated or invalid metadata dynamic-trait parameter flag";
            return false;
        }
        parameter.isDynamicTrait = dynamicTrait != 0U;
    }
    if (metadata.formatMinor >= 3)
    {
        std::uint8_t defaultKind = 0;
        if (!reader.u8(defaultKind) ||
            defaultKind > static_cast<std::uint8_t>(LibraryMetadataDefaultValueKind::String) ||
            !reader.u8(parameter.defaultValueFlags) || parameter.defaultValueFlags > 1U ||
            !reader.u64(parameter.defaultValueBits) || !reader.text(parameter.defaultValueText))
        {
            error = "truncated or invalid metadata parameter default value";
            return false;
        }
        parameter.defaultValueKind = static_cast<LibraryMetadataDefaultValueKind>(defaultKind);
    }
    return true;
}

bool decodeMember(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataMember& member,
    std::string& error)
{
    if (!reader.u8(member.kind) || !reader.u8(member.accessibility) ||
        !reader.u32(member.flags) || !reader.text(member.declaringTypeKey) ||
        !reader.text(member.name) || !reader.text(member.signatureKey) ||
        !reader.text(member.declaredTypeKey) ||
        !readOwnership(reader, metadata.formatMinor, member.declaredOwnership) ||
        !reader.text(member.cAbiName) || !reader.text(member.mangledName))
    {
        error = "truncated or invalid metadata member record";
        return false;
    }
    std::uint32_t parameterCount = 0;
    if (!readCount(reader, parameterCount))
    {
        error = "invalid metadata parameter count";
        return false;
    }
    member.parameters.reserve(parameterCount);
    for (std::uint32_t parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
    {
        LibraryMetadataParameter parameter;
        if (!decodeParameter(reader, metadata, parameter, error)) return false;
        member.parameters.push_back(std::move(parameter));
    }
    if (!reader.u8(member.constantKind) || !reader.u64(member.constantBits) ||
        (metadata.formatMinor >= 9 && !reader.text(member.constantText)) ||
        (metadata.formatMinor >= 10 && !reader.text(member.importDllName)) ||
        (metadata.formatMinor >= 13 && !reader.text(member.inlineSourceText)))
    {
        error = "truncated metadata constant record";
        return false;
    }
    return true;
}

bool decodeMembersAndTraitImpls(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t count = 0;
    if (!readCount(reader, count)) { error = "invalid metadata member count"; return false; }
    metadata.members.clear();
    metadata.members.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataMember member;
        if (!decodeMember(reader, metadata, member, error)) return false;
        metadata.members.push_back(std::move(member));
    }

    // NSMETA 1.40 (NS304-T08-F05): external trait conformances.
    if (metadata.formatMinor >= 40)
    {
        if (!readCount(reader, count)) { error = "invalid metadata trait-impl count"; return false; }
        metadata.traitImpls.clear();
        metadata.traitImpls.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            LibraryMetadataTraitImpl traitImpl;
            if (!reader.text(traitImpl.targetTypeKey) || !reader.text(traitImpl.traitTypeKey))
            {
                error = "truncated or invalid metadata trait-impl record";
                return false;
            }
            std::uint32_t methodCount = 0;
            if (!readCount(reader, methodCount)) { error = "invalid metadata trait-impl method count"; return false; }
            traitImpl.methods.reserve(methodCount);
            for (std::uint32_t methodIndex = 0; methodIndex < methodCount; ++methodIndex)
            {
                LibraryMetadataMember member;
                if (!decodeMember(reader, metadata, member, error)) return false;
                traitImpl.methods.push_back(std::move(member));
            }
            metadata.traitImpls.push_back(std::move(traitImpl));
        }
    }

    return true;
}

bool decodeGenericRecords(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t count = 0;
    if (!readCount(reader, count)) { error = "invalid generic-template count"; return false; }
    metadata.genericTemplates.clear();
    metadata.genericTemplates.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataGenericTemplate generic;
        if (!reader.u8(generic.kind) ||
            (metadata.formatMinor >= 7 && !reader.u8(generic.accessibility)) ||
            !reader.text(generic.canonicalKey) || !reader.text(generic.moduleName) ||
            !reader.text(generic.namespaceName) || !reader.text(generic.ownerName) || !reader.text(generic.name) ||
            !readStringVector(reader, generic.parameters) ||
            (metadata.formatMinor >= 7 && !reader.text(generic.sourceText)) ||
            (metadata.formatMinor >= 11 && !readStringVector(reader, generic.dependencies)))
        {
            error = "truncated generic-template metadata";
            return false;
        }
        metadata.genericTemplates.push_back(std::move(generic));
    }

    if (!readCount(reader, count)) { error = "invalid generic-specialization count"; return false; }
    metadata.genericSpecializations.clear();
    metadata.genericSpecializations.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataGenericSpecialization specialization;
        if (!reader.u8(specialization.kind) || !reader.text(specialization.templateKey) ||
            !reader.text(specialization.specializationKey) || !reader.text(specialization.specializedName) ||
            !readStringVector(reader, specialization.typeArguments))
        {
            error = "truncated generic-specialization metadata";
            return false;
        }
        metadata.genericSpecializations.push_back(std::move(specialization));
    }

    if (metadata.formatMinor >= 12 && !readStringVector(reader, metadata.externalTypeKeys))
    {
        error = "truncated external-type dependency metadata";
        return false;
    }

    return true;
}

bool decodeDispatchAndExportRecords(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t count = 0;
    if (!readCount(reader, count)) { error = "invalid dispatch-table count"; return false; }
    metadata.dispatchTables.clear();
    metadata.dispatchTables.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataDispatch dispatch;
        if (!reader.text(dispatch.typeKey) || !reader.u64(dispatch.stableTypeId) ||
            !reader.text(dispatch.descriptorName) ||
            (metadata.formatMinor >= 6 && !reader.text(dispatch.descriptorAccessorName)) ||
            !reader.text(dispatch.vtableName) ||
            !reader.text(dispatch.destructionEntryName) || !readStringVector(reader, dispatch.slotSignatures) ||
            !readStringVector(reader, dispatch.slotCalleeNames))
        {
            error = "truncated dispatch metadata";
            return false;
        }
        std::uint32_t interfaceCount = 0;
        if (!readCount(reader, interfaceCount)) { error = "invalid dispatch-interface count"; return false; }
        dispatch.interfaces.reserve(interfaceCount);
        for (std::uint32_t interfaceIndex = 0; interfaceIndex < interfaceCount; ++interfaceIndex)
        {
            LibraryMetadataDispatchInterface interfaceRecord;
            if (!reader.text(interfaceRecord.interfaceTypeKey) || !reader.u64(interfaceRecord.stableInterfaceTypeId) ||
                !reader.text(interfaceRecord.methodTableName) ||
                !readStringVector(reader, interfaceRecord.interfaceSignatures) ||
                !readStringVector(reader, interfaceRecord.implementationSignatures) ||
                !readStringVector(reader, interfaceRecord.calleeNames))
            {
                error = "truncated dispatch-interface metadata";
                return false;
            }
            dispatch.interfaces.push_back(std::move(interfaceRecord));
        }
        metadata.dispatchTables.push_back(std::move(dispatch));
    }

    if (!readCount(reader, count)) { error = "invalid C-export count"; return false; }
    metadata.exports.clear();
    metadata.exports.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        LibraryMetadataExport exportRecord;
        if (!reader.text(exportRecord.cAbiName) || !reader.text(exportRecord.declaringTypeKey) ||
            !reader.text(exportRecord.returnTypeKey) || !readStringVector(reader, exportRecord.parameterTypeKeys) ||
            !reader.text(exportRecord.nativeMangledName))
        {
            error = "truncated C-export metadata";
            return false;
        }
        metadata.exports.push_back(std::move(exportRecord));
    }

    metadata.namespaceFunctions.clear();
    return true;
}

bool decodeNamespaceFunctionParameter(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataParameter& parameter,
    std::string& error)
{
    std::uint8_t parameterOwnership = 0;
    std::uint8_t passKind = 0;
    std::uint8_t parameterPack = 0;
    std::uint8_t dynamicTrait = 0;
    std::uint8_t defaultKind = 0;
    if (!reader.text(parameter.name) || !reader.text(parameter.typeKey) ||
        !reader.u8(parameterOwnership) ||
        parameterOwnership > static_cast<std::uint8_t>(LibraryMetadataOwnershipKind::Borrowed) ||
        !reader.u8(passKind) ||
        passKind > static_cast<std::uint8_t>(LibraryMetadataParameterPassKind::In) ||
        (metadata.formatMinor >= 24 &&
         (!reader.u8(parameterPack) || parameterPack > 1U)) ||
        (metadata.formatMinor >= 31 &&
         (!reader.u8(dynamicTrait) || dynamicTrait > 1U)) ||
        !reader.u8(defaultKind) ||
        defaultKind > static_cast<std::uint8_t>(LibraryMetadataDefaultValueKind::String) ||
        !reader.u8(parameter.defaultValueFlags) || parameter.defaultValueFlags > 1U ||
        !reader.u64(parameter.defaultValueBits) || !reader.text(parameter.defaultValueText))
    {
        error = "truncated namespace-function parameter metadata";
        return false;
    }
    parameter.ownership = static_cast<LibraryMetadataOwnershipKind>(parameterOwnership);
    parameter.passKind = static_cast<LibraryMetadataParameterPassKind>(passKind);
    parameter.isParameterPack = parameterPack != 0U;
    parameter.isDynamicTrait = dynamicTrait != 0U;
    parameter.defaultValueKind = static_cast<LibraryMetadataDefaultValueKind>(defaultKind);
    return true;
}

bool decodeNamespaceFunction(
    ByteReader& reader,
    const LibraryMetadata& metadata,
    LibraryMetadataNamespaceFunction& function,
    std::string& error)
{
    if (!reader.u8(function.accessibility) || !reader.u8(function.flags) ||
        function.accessibility > 1U ||
        function.flags > (metadata.formatMinor >= 32 ? 63U :
                              (metadata.formatMinor >= 31 ? 31U : 15U)) ||
        !reader.text(function.namespaceName) || !reader.text(function.name) ||
        !reader.text(function.signatureKey) || !reader.text(function.declaredTypeKey))
    {
        error = "truncated or invalid namespace-function metadata";
        return false;
    }
    std::uint8_t ownership = 0;
    if (!reader.u8(ownership) ||
        ownership > static_cast<std::uint8_t>(LibraryMetadataOwnershipKind::Borrowed) ||
        !reader.text(function.mangledName))
    {
        error = "truncated namespace-function identity";
        return false;
    }
    function.declaredOwnership = static_cast<LibraryMetadataOwnershipKind>(ownership);
    std::uint32_t parameterCount = 0;
    if (!readCount(reader, parameterCount))
    {
        error = "invalid namespace-function parameter count";
        return false;
    }
    function.parameters.reserve(parameterCount);
    for (std::uint32_t parameterIndex = 0;
         parameterIndex < parameterCount;
         ++parameterIndex)
    {
        LibraryMetadataParameter parameter;
        if (!decodeNamespaceFunctionParameter(reader, metadata, parameter, error)) return false;
        function.parameters.push_back(std::move(parameter));
    }
    return true;
}

bool decodeNamespaceFunctions(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor >= 21)
    {
        std::uint32_t count = 0;
        if (!readCount(reader, count))
        {
            error = "invalid namespace-function count";
            return false;
        }
        metadata.namespaceFunctions.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            LibraryMetadataNamespaceFunction function;
            if (!decodeNamespaceFunction(reader, metadata, function, error)) return false;
            metadata.namespaceFunctions.push_back(std::move(function));
        }
    }

    metadata.namespaceStates.clear();
    return true;
}

bool decodeNamespaceStates(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t count = 0;
    if (metadata.formatMinor >= 22)
    {
        if (!readCount(reader, count))
        {
            error = "invalid namespace-state count";
            return false;
        }
        metadata.namespaceStates.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            LibraryMetadataNamespaceState state;
            std::uint8_t initializerKind = 0;
            if (!reader.u8(state.accessibility) || state.accessibility > 1U ||
                !reader.u8(state.flags) ||
                state.flags > (metadata.formatMinor >= 31 ? 3U : 1U) ||
                !reader.u8(initializerKind) ||
                initializerKind > static_cast<std::uint8_t>(
                    LibraryMetadataNamespaceStateInitializerKind::Null) ||
                !reader.text(state.namespaceName) || !reader.text(state.name) ||
                !reader.text(state.declaredTypeKey) ||
                !reader.text(state.storageName) || !reader.text(state.accessorName) ||
                !reader.u64(state.initializerBits) ||
                !reader.text(state.initializerText))
            {
                error = "truncated or invalid namespace-state metadata";
                return false;
            }
            state.initializerKind =
                static_cast<LibraryMetadataNamespaceStateInitializerKind>(
                    initializerKind);
            metadata.namespaceStates.push_back(std::move(state));
        }
    }
    metadata.binaryDependencies.clear();
    metadata.platformImports.clear();
    return true;
}

bool decodeBinaryClosure(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t count = 0;
    if (metadata.formatMinor >= 25)
    {
        if (!readCount(reader, count))
        {
            error = "invalid binary-dependency count";
            return false;
        }
        metadata.binaryDependencies.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            LibraryMetadataBinaryDependency dependency;
            if (!reader.text(dependency.moduleName) ||
                !reader.text(dependency.libraryFileName) ||
                !reader.text(dependency.metadataFileName) ||
                !reader.text(dependency.runtimeFileName) ||
                !reader.raw(dependency.abiFingerprint))
            {
                error = "truncated binary-dependency metadata";
                return false;
            }
            metadata.binaryDependencies.push_back(std::move(dependency));
        }
        if (!readCount(reader, count))
        {
            error = "invalid platform-import count";
            return false;
        }
        metadata.platformImports.reserve(count);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            LibraryMetadataPlatformImport importRecord;
            if (!reader.text(importRecord.dllName) || !reader.text(importRecord.symbolName))
            {
                error = "truncated platform-import metadata";
                return false;
            }
            metadata.platformImports.push_back(std::move(importRecord));
        }
    }
    return true;
}

template<typename ConstraintCollection>
bool decodeLifetimeConstraints(
    ByteReader& reader,
    ConstraintCollection& constraints)
{
    std::uint32_t constraintCount = 0;
    if (!readCount(reader, constraintCount)) return false;
    constraints.clear();
    constraints.reserve(constraintCount);
    for (std::uint32_t index = 0; index < constraintCount; ++index)
    {
        std::string longer;
        std::string shorter;
        if (!reader.text(longer) || !reader.text(shorter)) return false;
        constraints.emplace_back(std::move(longer), std::move(shorter));
    }
    return true;
}

bool decodeTypeLifetimeContract(
    ByteReader& reader,
    LibraryMetadataType& type,
    std::string& error)
{
    std::uint8_t referenceReturn = 0;
    if (!readStringVector(reader, type.lifetimeParameters) ||
        !decodeLifetimeConstraints(reader, type.lifetimeOutlivesConstraints) ||
        !reader.u8(referenceReturn) || referenceReturn > 1U ||
        !reader.text(type.delegateReturnLifetimeName))
    {
        error = "truncated or invalid metadata lifetime type contract";
        return false;
    }
    type.delegateReferenceReturn = referenceReturn != 0U;
    std::uint32_t passCount = 0;
    if (!readCount(reader, passCount))
    {
        error = "invalid metadata delegate pass-kind count";
        return false;
    }
    type.delegateParameterPassKinds.clear();
    type.delegateParameterPassKinds.reserve(passCount);
    for (std::uint32_t index = 0; index < passCount; ++index)
    {
        std::uint8_t passKind = 0;
        if (!reader.u8(passKind) ||
            passKind > static_cast<std::uint8_t>(LibraryMetadataParameterPassKind::In))
        {
            error = "truncated or invalid metadata delegate pass kind";
            return false;
        }
        type.delegateParameterPassKinds.push_back(
            static_cast<LibraryMetadataParameterPassKind>(passKind));
    }
    if (!readStringVector(reader, type.delegateParameterLifetimeNames))
    {
        error = "truncated metadata delegate lifetime bindings";
        return false;
    }
    std::uint32_t fieldCount = 0;
    if (!readCount(reader, fieldCount) || fieldCount != type.fields.size())
    {
        error = "invalid metadata field lifetime count";
        return false;
    }
    for (LibraryMetadataField& field : type.fields)
    {
        if (!reader.text(field.lifetimeName))
        {
            error = "truncated metadata field lifetime binding";
            return false;
        }
    }
    return true;
}

bool decodeTypeLifetimeCatalogue(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t catalogueCount = 0;
    if (!readCount(reader, catalogueCount) || catalogueCount != metadata.types.size())
    {
        error = "invalid metadata lifetime type catalogue count";
        return false;
    }
    for (LibraryMetadataType& type : metadata.types)
    {
        if (!decodeTypeLifetimeContract(reader, type, error)) return false;
    }
    return true;
}

bool decodeMemberLifetimeContract(
    ByteReader& reader,
    LibraryMetadataMember& member,
    std::string& error)
{
    if (!reader.text(member.returnLifetimeName) ||
        !readStringVector(reader, member.lifetimeParameters) ||
        !decodeLifetimeConstraints(reader, member.lifetimeOutlivesConstraints))
    {
        error = "truncated metadata member lifetime contract";
        return false;
    }
    std::uint32_t parameterCount = 0;
    if (!readCount(reader, parameterCount) || parameterCount != member.parameters.size())
    {
        error = "invalid metadata member parameter lifetime count";
        return false;
    }
    for (LibraryMetadataParameter& parameter : member.parameters)
    {
        if (!reader.text(parameter.lifetimeName))
        {
            error = "truncated metadata member parameter lifetime binding";
            return false;
        }
    }
    return true;
}

bool decodeMemberLifetimeCatalogue(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t catalogueCount = 0;
    if (!readCount(reader, catalogueCount) || catalogueCount != metadata.members.size())
    {
        error = "invalid metadata member lifetime catalogue count";
        return false;
    }
    for (LibraryMetadataMember& member : metadata.members)
    {
        if (!decodeMemberLifetimeContract(reader, member, error)) return false;
    }
    return true;
}

bool decodeNamespaceFunctionLifetimeContract(
    ByteReader& reader,
    LibraryMetadataNamespaceFunction& function,
    std::string& error)
{
    if (!reader.text(function.returnLifetimeName) ||
        !readStringVector(reader, function.lifetimeParameters) ||
        !decodeLifetimeConstraints(reader, function.lifetimeOutlivesConstraints))
    {
        error = "truncated metadata namespace-function lifetime contract";
        return false;
    }
    std::uint32_t parameterCount = 0;
    if (!readCount(reader, parameterCount) || parameterCount != function.parameters.size())
    {
        error = "invalid metadata namespace-function parameter lifetime count";
        return false;
    }
    for (LibraryMetadataParameter& parameter : function.parameters)
    {
        if (!reader.text(parameter.lifetimeName))
        {
            error = "truncated metadata namespace-function parameter lifetime binding";
            return false;
        }
    }
    return true;
}

bool decodeNamespaceFunctionLifetimeCatalogue(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t catalogueCount = 0;
    if (!readCount(reader, catalogueCount) ||
        catalogueCount != metadata.namespaceFunctions.size())
    {
        error = "invalid metadata namespace-function lifetime catalogue count";
        return false;
    }
    for (LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        if (!decodeNamespaceFunctionLifetimeContract(reader, function, error)) return false;
    }
    return true;
}

bool decodeLifetimeContracts(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor < 34) return true;
    if (!decodeTypeLifetimeCatalogue(reader, metadata, error)) return false;
    if (!decodeMemberLifetimeCatalogue(reader, metadata, error)) return false;
    return decodeNamespaceFunctionLifetimeCatalogue(reader, metadata, error);
}

template<typename Callable>
bool decodeEscapeSummary(
    ByteReader& reader,
    Callable& callable,
    std::string_view kind,
    std::string& error)
{
    std::uint32_t parameterCount = 0;
    if (!readCount(reader, parameterCount) ||
        parameterCount != callable.parameters.size())
    {
        error = "invalid metadata " + std::string(kind) +
            " escape-summary parameter count";
        return false;
    }
    callable.parameterEscapeSummary.reserve(parameterCount);
    for (std::uint32_t index = 0; index < parameterCount; ++index)
    {
        std::uint8_t raw = 0;
        if (!reader.u8(raw) ||
            raw > static_cast<std::uint8_t>(LibraryMetadataEscapeClass::Unknown))
        {
            error = "truncated or invalid metadata " + std::string(kind) +
                " escape summary";
            return false;
        }
        callable.parameterEscapeSummary.push_back(
            static_cast<LibraryMetadataEscapeClass>(raw));
    }
    return true;
}

template<typename CallableCollection>
bool decodeEscapeCatalogue(
    ByteReader& reader,
    CallableCollection& callables,
    std::string_view kind,
    std::string_view countError,
    std::string& error)
{
    std::uint32_t catalogueCount = 0;
    if (!readCount(reader, catalogueCount) || catalogueCount != callables.size())
    {
        error = std::string(countError);
        return false;
    }
    for (auto& callable : callables)
    {
        if (!decodeEscapeSummary(reader, callable, kind, error)) return false;
    }
    return true;
}

bool decodeEscapeContracts(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor < 35) return true;
    if (!decodeEscapeCatalogue(
            reader,
            metadata.members,
            "member",
            "invalid metadata member escape-summary catalogue count",
            error))
    {
        return false;
    }
    return decodeEscapeCatalogue(
        reader,
        metadata.namespaceFunctions,
        "namespace-function",
        "invalid metadata namespace-function escape-summary catalogue count",
        error);
}

bool decodeErrorContracts(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor >= 37)
    {
        std::uint32_t catalogueCount = 0;
        if (!readCount(reader, catalogueCount) || catalogueCount != metadata.types.size())
        {
            error = "invalid metadata delegate throws catalogue count";
            return false;
        }
        for (LibraryMetadataType& type : metadata.types)
            if (!reader.text(type.delegateErrorTypeKey))
            {
                error = "truncated metadata delegate throws contract";
                return false;
            }
        if (!readCount(reader, catalogueCount) || catalogueCount != metadata.members.size())
        {
            error = "invalid metadata member throws catalogue count";
            return false;
        }
        for (LibraryMetadataMember& member : metadata.members)
            if (!reader.text(member.declaredErrorTypeKey))
            {
                error = "truncated metadata member throws contract";
                return false;
            }
        if (!readCount(reader, catalogueCount) ||
            catalogueCount != metadata.namespaceFunctions.size())
        {
            error = "invalid metadata namespace-function throws catalogue count";
            return false;
        }
        for (LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
            if (!reader.text(function.declaredErrorTypeKey))
            {
                error = "truncated metadata namespace-function throws contract";
                return false;
            }
    }
    return true;
}

bool decodeGenericParameterContract(
    ByteReader& reader,
    LibraryMetadataGenericTemplate::ParameterContract& parameter,
    std::string& error)
{
    if (!reader.text(parameter.name) || !reader.u8(parameter.kind) ||
        !reader.u16(parameter.flags) || !reader.text(parameter.constantTypeKey) ||
        !reader.text(parameter.equalityTypeKey) ||
        !reader.text(parameter.associatedProjectionKey) ||
        !readStringVector(reader, parameter.typeConstraintKeys))
    {
        error = "truncated metadata generic parameter contract";
        return false;
    }
    return true;
}

bool decodeGenericTemplateContract(
    ByteReader& reader,
    LibraryMetadataGenericTemplate& generic,
    std::string& error)
{
    std::uint32_t parameterCount = 0;
    if (!readCount(reader, parameterCount))
    {
        error = "invalid metadata generic parameter-contract count";
        return false;
    }
    generic.parameterContracts.reserve(parameterCount);
    for (std::uint32_t index = 0; index < parameterCount; ++index)
    {
        LibraryMetadataGenericTemplate::ParameterContract parameter;
        if (!decodeGenericParameterContract(reader, parameter, error)) return false;
        generic.parameterContracts.push_back(std::move(parameter));
    }
    if (!readStringVector(reader, generic.lifetimeParameters))
    {
        error = "truncated metadata generic lifetime binders";
        return false;
    }
    std::uint32_t edgeCount = 0;
    if (!readCount(reader, edgeCount))
    {
        error = "invalid metadata generic lifetime edge count";
        return false;
    }
    generic.lifetimeOutlivesConstraints.reserve(edgeCount);
    for (std::uint32_t index = 0; index < edgeCount; ++index)
    {
        std::string longer;
        std::string shorter;
        if (!reader.text(longer) || !reader.text(shorter))
        {
            error = "truncated metadata generic lifetime edge";
            return false;
        }
        generic.lifetimeOutlivesConstraints.emplace_back(
            std::move(longer), std::move(shorter));
    }
    return true;
}

bool decodeGenericTemplateContracts(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t catalogueCount = 0;
    if (!readCount(reader, catalogueCount) ||
        catalogueCount != metadata.genericTemplates.size())
    {
        error = "invalid metadata generic ownership catalogue count";
        return false;
    }
    for (LibraryMetadataGenericTemplate& generic : metadata.genericTemplates)
    {
        if (!decodeGenericTemplateContract(reader, generic, error)) return false;
    }
    return true;
}

bool decodeFfiMemberContract(
    ByteReader& reader,
    LibraryMetadataMember& member,
    std::string& error)
{
    std::uint32_t parameterCount = 0;
    if (!reader.u8(member.ffiUnwindPolicy) || member.ffiUnwindPolicy > 2U ||
        !readCount(reader, parameterCount) ||
        parameterCount != member.parameters.size())
    {
        error = "truncated or invalid metadata FFI member contract";
        return false;
    }
    for (LibraryMetadataParameter& parameter : member.parameters)
    {
        if (!reader.u32(parameter.ffiLengthParameter) ||
            !reader.text(parameter.ffiLengthParameterName) ||
            !reader.u8(parameter.ffiCallbackLifetime) ||
            parameter.ffiCallbackLifetime > 2U ||
            (parameter.ffiLengthParameter != UINT32_MAX &&
             parameter.ffiLengthParameter >= member.parameters.size()))
        {
            error = "truncated or invalid metadata FFI parameter contract";
            return false;
        }
    }
    return true;
}

bool decodeFfiMemberContracts(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    std::uint32_t catalogueCount = 0;
    if (!readCount(reader, catalogueCount) || catalogueCount != metadata.members.size())
    {
        error = "invalid metadata FFI-contract catalogue count";
        return false;
    }
    for (LibraryMetadataMember& member : metadata.members)
    {
        if (!decodeFfiMemberContract(reader, member, error)) return false;
    }
    return true;
}

bool decodeGenericContracts(
    ByteReader& reader,
    LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor < 38) return true;
    if (!decodeGenericTemplateContracts(reader, metadata, error)) return false;
    return decodeFfiMemberContracts(reader, metadata, error);
}

bool decodePayload(
    std::span<const std::uint8_t> payload,
    LibraryMetadata& metadata,
    std::string& error)
{
    ByteReader reader(payload);
    if (!decodePayloadPreamble(reader, metadata, error)) return false;
    if (!decodeTypes(reader, metadata, error)) return false;
    if (!decodeMembersAndTraitImpls(reader, metadata, error)) return false;
    if (!decodeGenericRecords(reader, metadata, error)) return false;
    if (!decodeDispatchAndExportRecords(reader, metadata, error)) return false;
    if (!decodeNamespaceFunctions(reader, metadata, error)) return false;
    if (!decodeNamespaceStates(reader, metadata, error)) return false;
    if (!decodeBinaryClosure(reader, metadata, error)) return false;
    if (!decodeLifetimeContracts(reader, metadata, error)) return false;
    if (!decodeEscapeContracts(reader, metadata, error)) return false;
    if (!decodeErrorContracts(reader, metadata, error)) return false;
    if (!decodeGenericContracts(reader, metadata, error)) return false;
    if (metadata.formatMinor >= 39)
    {
        if (!reader.u32(metadata.languageFeatureFlags) ||
            (metadata.languageFeatureFlags & ~kMetadataLanguageFeatureMask) != 0U)
        {
            error = "truncated or invalid metadata language-feature catalogue";
            return false;
        }
    }
    if (!reader.atEnd())
    {
        error = "metadata payload has trailing bytes";
        return false;
    }

    return true;
}

}
