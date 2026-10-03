#include "nativesharp/Interop/LibraryMetadataMerger.h"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <string_view>
#include <tuple>
#include <utility>

namespace nativesharp::interop
{
namespace
{
template <typename T, typename Equal>
bool vectorEqual(const std::vector<T>& left, const std::vector<T>& right, Equal equal)
{
    return left.size() == right.size() &&
        std::ranges::equal(left, right, std::move(equal));
}

bool equalField(const LibraryMetadataField& left, const LibraryMetadataField& right)
{
    return std::tie(left.name, left.typeKey, left.ownership, left.offset, left.size,
               left.alignment, left.flags, left.bitOffset, left.bitWidth) ==
        std::tie(right.name, right.typeKey, right.ownership, right.offset, right.size,
            right.alignment, right.flags, right.bitOffset, right.bitWidth);
}

bool equalType(const LibraryMetadataType& left, const LibraryMetadataType& right)
{
    const auto equalVariantCase = [](const LibraryMetadataVariantCase& a,
                                     const LibraryMetadataVariantCase& b)
    {
        return std::tie(a.name, a.payloadTypeKey) == std::tie(b.name, b.payloadTypeKey);
    };
    return std::tie(left.kind, left.accessibility, left.flags, left.identityKey,
               left.fullyQualifiedName, left.baseTypeKey, left.interfaceTypeKeys,
               left.size, left.alignment, left.headerSize, left.referenceSize,
               left.referenceAlignment, left.stableTypeId, left.enumUnderlyingTypeKey,
               left.delegateFlags, left.delegateReturnTypeKey,
               left.delegateParameterTypeKeys) ==
            std::tie(right.kind, right.accessibility, right.flags, right.identityKey,
                right.fullyQualifiedName, right.baseTypeKey, right.interfaceTypeKeys,
                right.size, right.alignment, right.headerSize, right.referenceSize,
                right.referenceAlignment, right.stableTypeId, right.enumUnderlyingTypeKey,
                right.delegateFlags, right.delegateReturnTypeKey,
                right.delegateParameterTypeKeys) &&
        vectorEqual(left.variantCases, right.variantCases, equalVariantCase) &&
        vectorEqual(left.fields, right.fields, equalField);
}

bool equalParameter(const LibraryMetadataParameter& left, const LibraryMetadataParameter& right)
{
    return std::tie(left.name, left.typeKey, left.ownership, left.passKind,
               left.isParameterPack,
               left.defaultValueKind, left.defaultValueFlags, left.defaultValueBits,
               left.defaultValueText) ==
        std::tie(right.name, right.typeKey, right.ownership, right.passKind,
            right.isParameterPack,
            right.defaultValueKind, right.defaultValueFlags, right.defaultValueBits,
            right.defaultValueText);
}

bool equalMember(const LibraryMetadataMember& left, const LibraryMetadataMember& right)
{
    return std::tie(left.kind, left.accessibility, left.flags, left.declaringTypeKey,
               left.name, left.signatureKey, left.declaredTypeKey,
               left.declaredOwnership, left.cAbiName, left.mangledName,
               left.constantKind, left.constantBits, left.constantText,
               left.importDllName, left.inlineSourceText) ==
            std::tie(right.kind, right.accessibility, right.flags, right.declaringTypeKey,
                right.name, right.signatureKey, right.declaredTypeKey,
                right.declaredOwnership, right.cAbiName, right.mangledName,
                right.constantKind, right.constantBits, right.constantText,
                right.importDllName, right.inlineSourceText) &&
        vectorEqual(left.parameters, right.parameters, equalParameter);
}

bool equalNamespaceFunction(
    const LibraryMetadataNamespaceFunction& left,
    const LibraryMetadataNamespaceFunction& right)
{
    return std::tie(left.accessibility, left.flags, left.namespaceName, left.name,
               left.signatureKey, left.declaredTypeKey, left.declaredOwnership,
               left.mangledName) ==
            std::tie(right.accessibility, right.flags, right.namespaceName, right.name,
                right.signatureKey, right.declaredTypeKey, right.declaredOwnership,
                right.mangledName) &&
        vectorEqual(left.parameters, right.parameters, equalParameter);
}

bool equalNamespaceState(
    const LibraryMetadataNamespaceState& left,
    const LibraryMetadataNamespaceState& right)
{
    return std::tie(left.accessibility, left.flags, left.initializerKind,
               left.namespaceName, left.name, left.declaredTypeKey,
               left.storageName, left.accessorName, left.initializerBits,
               left.initializerText) ==
        std::tie(right.accessibility, right.flags, right.initializerKind,
            right.namespaceName, right.name, right.declaredTypeKey,
            right.storageName, right.accessorName, right.initializerBits,
            right.initializerText);
}
bool equalTemplate(
    const LibraryMetadataGenericTemplate& left,
    const LibraryMetadataGenericTemplate& right)
{
    return std::tie(left.kind, left.accessibility, left.canonicalKey, left.moduleName,
               left.namespaceName, left.ownerName, left.name, left.parameters,
               left.sourceText, left.dependencies) ==
        std::tie(right.kind, right.accessibility, right.canonicalKey, right.moduleName,
            right.namespaceName, right.ownerName, right.name, right.parameters,
            right.sourceText, right.dependencies);
}

bool equalSpecialization(
    const LibraryMetadataGenericSpecialization& left,
    const LibraryMetadataGenericSpecialization& right)
{
    return std::tie(left.kind, left.templateKey, left.specializationKey,
               left.specializedName, left.typeArguments) ==
        std::tie(right.kind, right.templateKey, right.specializationKey,
            right.specializedName, right.typeArguments);
}

bool equalDispatchInterface(
    const LibraryMetadataDispatchInterface& left,
    const LibraryMetadataDispatchInterface& right)
{
    return std::tie(left.interfaceTypeKey, left.stableInterfaceTypeId,
               left.methodTableName, left.interfaceSignatures,
               left.implementationSignatures, left.calleeNames) ==
        std::tie(right.interfaceTypeKey, right.stableInterfaceTypeId,
            right.methodTableName, right.interfaceSignatures,
            right.implementationSignatures, right.calleeNames);
}

bool equalDispatch(const LibraryMetadataDispatch& left, const LibraryMetadataDispatch& right)
{
    return std::tie(left.typeKey, left.stableTypeId, left.descriptorName,
               left.descriptorAccessorName, left.vtableName, left.destructionEntryName,
               left.slotSignatures, left.slotCalleeNames) ==
            std::tie(right.typeKey, right.stableTypeId, right.descriptorName,
                right.descriptorAccessorName, right.vtableName,
                right.destructionEntryName, right.slotSignatures,
                right.slotCalleeNames) &&
        vectorEqual(left.interfaces, right.interfaces, equalDispatchInterface);
}

bool equalExport(const LibraryMetadataExport& left, const LibraryMetadataExport& right)
{
    return std::tie(left.cAbiName, left.declaringTypeKey, left.returnTypeKey,
               left.parameterTypeKeys, left.nativeMangledName) ==
        std::tie(right.cAbiName, right.declaringTypeKey, right.returnTypeKey,
            right.parameterTypeKeys, right.nativeMangledName);
}

bool equalBinaryDependency(
    const LibraryMetadataBinaryDependency& left,
    const LibraryMetadataBinaryDependency& right)
{
    return std::tie(left.moduleName, left.libraryFileName, left.metadataFileName,
               left.runtimeFileName, left.abiFingerprint) ==
        std::tie(right.moduleName, right.libraryFileName, right.metadataFileName,
            right.runtimeFileName, right.abiFingerprint);
}

bool equalPlatformImport(
    const LibraryMetadataPlatformImport& left,
    const LibraryMetadataPlatformImport& right)
{
    return std::tie(left.dllName, left.symbolName) ==
        std::tie(right.dllName, right.symbolName);
}

bool sameHeader(const LibraryMetadata& left, const LibraryMetadata& right)
{
    return std::tie(left.formatMajor, left.formatMinor, left.targetId, left.pointerSize,
               left.pointerAlignment, left.producerVersion, left.moduleName,
               left.targetTriple, left.languageEdition, left.nativeAbiMajor, left.nativeAbiMinor,
               left.runtimeAbiMajor, left.runtimeAbiMinor, left.manglingAbiVersion,
               left.objectAbiVersion, left.delegateAbiVersion, left.exceptionAbiVersion,
               left.managedDelegateCellSize) ==
        std::tie(right.formatMajor, right.formatMinor, right.targetId, right.pointerSize,
            right.pointerAlignment, right.producerVersion, right.moduleName,
            right.targetTriple, right.languageEdition, right.nativeAbiMajor, right.nativeAbiMinor,
            right.runtimeAbiMajor, right.runtimeAbiMinor, right.manglingAbiVersion,
            right.objectAbiVersion, right.delegateAbiVersion, right.exceptionAbiVersion,
            right.managedDelegateCellSize);
}

template <typename Record, typename Key, typename Equal>
bool canonicalize(
    std::vector<Record>& records,
    Key key,
    Equal equal,
    std::string_view category,
    std::string& error)
{
    std::sort(records.begin(), records.end(), [&](const Record& left, const Record& right) {
        return key(left) < key(right);
    });
    std::vector<Record> canonical;
    canonical.reserve(records.size());
    for (Record& record : records)
    {
        if (canonical.empty() || key(canonical.back()) != key(record))
        {
            canonical.push_back(std::move(record));
            continue;
        }
        if (!equal(canonical.back(), record))
        {
            error = "NSMETA merge conflict for " + std::string(category) + " '" +
                std::string(key(record)) + "'";
            return false;
        }
    }
    records = std::move(canonical);
    return true;
}

void canonicalizeStrings(std::vector<std::string>& values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

bool canonicalizeModel(LibraryMetadata& metadata, std::string& error)
{
    for (LibraryMetadataType& type : metadata.types)
    {
        canonicalizeStrings(type.interfaceTypeKeys);
        std::sort(type.fields.begin(), type.fields.end(), [](const auto& left, const auto& right) {
            return std::tie(left.offset, left.name, left.typeKey) <
                std::tie(right.offset, right.name, right.typeKey);
        });
    }
    for (LibraryMetadataGenericTemplate& generic : metadata.genericTemplates)
        canonicalizeStrings(generic.dependencies);
    for (LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
    {
        if (!canonicalize(
                dispatch.interfaces,
                [](const auto& item) -> const std::string& { return item.interfaceTypeKey; },
                equalDispatchInterface,
                "dispatch interface",
                error))
            return false;
    }
    canonicalizeStrings(metadata.externalTypeKeys);

    if (!canonicalize(
            metadata.types,
            [](const auto& item) -> const std::string& { return item.identityKey; },
            equalType,
            "type",
            error))
        return false;
    if (!canonicalize(
            metadata.members,
            [](const auto& item) {
                return item.declaringTypeKey + "\n" + std::to_string(item.kind) + "\n" +
                    item.signatureKey + "\n" + item.name;
            },
            equalMember,
            "member",
            error))
        return false;
    if (!canonicalize(
            metadata.namespaceFunctions,
            [](const auto& item) {
                return item.namespaceName + std::string(1, static_cast<char>(31)) +
                    item.signatureKey;
            },
            equalNamespaceFunction,
            "namespace function",
            error))
        return false;
    if (!canonicalize(
            metadata.namespaceStates,
            [](const auto& item) {
                return item.namespaceName + std::string(1, static_cast<char>(31)) +
                    item.name;
            },
            equalNamespaceState,
            "namespace state",
            error))
        return false;
    if (!canonicalize(
            metadata.genericTemplates,
            [](const auto& item) -> const std::string& { return item.canonicalKey; },
            equalTemplate,
            "generic template",
            error))
        return false;
    if (!canonicalize(
            metadata.genericSpecializations,
            [](const auto& item) -> const std::string& { return item.specializationKey; },
            equalSpecialization,
            "generic specialization",
            error))
        return false;
    if (!canonicalize(
            metadata.dispatchTables,
            [](const auto& item) -> const std::string& { return item.typeKey; },
            equalDispatch,
            "dispatch table",
            error))
        return false;
    if (!canonicalize(
        metadata.exports,
        [](const auto& item) -> const std::string& { return item.cAbiName; },
        equalExport,
        "C ABI export",
        error))
        return false;
    if (!canonicalize(
            metadata.binaryDependencies,
            [](const auto& item) -> const std::string& { return item.moduleName; },
            equalBinaryDependency,
            "binary dependency",
            error))
        return false;
    return canonicalize(
        metadata.platformImports,
        [](const auto& item) { return item.dllName + "\n" + item.symbolName; },
        equalPlatformImport,
        "platform import",
        error);
}

template <typename Record>
void append(std::vector<Record>& destination, const std::vector<Record>& source)
{
    destination.insert(destination.end(), source.begin(), source.end());
}
}

bool LibraryMetadataMerger::merge(
    std::span<const LibraryMetadata> fragments,
    LibraryMetadata& merged,
    std::string& error) const
{
    error.clear();
    if (fragments.empty())
    {
        error = "NSMETA merge requires at least one fragment";
        return false;
    }

    LibraryMetadata candidate = fragments.front();
    candidate.abiFingerprint = {};
    candidate.types.clear();
    candidate.members.clear();
    candidate.namespaceFunctions.clear();
    candidate.namespaceStates.clear();
    candidate.genericTemplates.clear();
    candidate.genericSpecializations.clear();
    candidate.externalTypeKeys.clear();
    candidate.dispatchTables.clear();
    candidate.exports.clear();
    candidate.binaryDependencies.clear();
    candidate.platformImports.clear();

    for (std::size_t ordinal = 0; ordinal < fragments.size(); ++ordinal)
    {
        const LibraryMetadata& fragment = fragments[ordinal];
        if (!sameHeader(fragments.front(), fragment))
        {
            error = "NSMETA fragment " + std::to_string(ordinal) +
                " has a header incompatible with module '" +
                fragments.front().moduleName + "'";
            return false;
        }
        append(candidate.types, fragment.types);
        append(candidate.members, fragment.members);
        append(candidate.namespaceFunctions, fragment.namespaceFunctions);
        append(candidate.namespaceStates, fragment.namespaceStates);
        append(candidate.genericTemplates, fragment.genericTemplates);
        append(candidate.genericSpecializations, fragment.genericSpecializations);
        append(candidate.externalTypeKeys, fragment.externalTypeKeys);
        append(candidate.dispatchTables, fragment.dispatchTables);
        append(candidate.exports, fragment.exports);
        append(candidate.binaryDependencies, fragment.binaryDependencies);
        append(candidate.platformImports, fragment.platformImports);
    }
    if (!canonicalizeModel(candidate, error)) return false;
    merged = std::move(candidate);
    return true;
}
}
