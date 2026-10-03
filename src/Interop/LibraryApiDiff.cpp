#include "nativesharp/Interop/LibraryApiDiff.h"

#include "nativesharp/Basic/Sha256.h"
#include "nativesharp/Language/ModuleSemantics.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>

namespace nativesharp::interop
{
namespace
{
using Surface = std::map<std::string, std::string>;

constexpr std::uint8_t kProtectedAccessibility = 1U;
constexpr std::uint8_t kPublicAccessibility = 3U;
constexpr std::uint16_t kPartialTypeFlag = 1U << 0U;
constexpr std::uint32_t kSemanticOnlyMemberFlags =
    kMetadataMemberInliningMask | kMetadataMemberNoHeap |
    kMetadataMemberDynamicTrait;

void field(std::string& output, std::string_view value)
{
    output += std::to_string(value.size());
    output.push_back(':');
    output.append(value);
    output.push_back(';');
}

template <typename Integer>
void number(std::string& output, Integer value)
{
    field(output, std::to_string(static_cast<std::uint64_t>(value)));
}

template <typename Values>
void strings(std::string& output, const Values& values, bool sortValues)
{
    std::vector<std::string> ordered(values.begin(), values.end());
    if (sortValues) std::ranges::sort(ordered);
    number(output, ordered.size());
    for (const std::string& value : ordered) field(output, value);
}

void parameterSourceContract(std::string& output, const LibraryMetadataParameter& parameter)
{
    field(output, parameter.name);
    field(output, parameter.typeKey);
    number(output, parameter.ownership);
    number(output, parameter.passKind);
    number(output, parameter.isParameterPack);
    number(output, parameter.isDynamicTrait);
    number(output, parameter.defaultValueKind);
    number(output, parameter.defaultValueFlags);
    number(output, parameter.defaultValueBits);
    field(output, parameter.defaultValueText);
}

void parameterAbiContract(std::string& output, const LibraryMetadataParameter& parameter)
{
    field(output, parameter.typeKey);
    number(output, parameter.passKind);
    number(output, parameter.isParameterPack);
}

std::string canonicalModule(std::string_view module)
{
    return language::canonicalWindowsModuleIdentity({}, module);
}

Surface sourceSurface(const LibraryMetadata& metadata)
{
    Surface result;
    std::string header;
    field(header, canonicalModule(metadata.moduleName));
    number(header, metadata.languageEdition);
    result.emplace("module", std::move(header));

    for (const LibraryMetadataType& type : metadata.types)
    {
        if (type.accessibility != kPublicAccessibility &&
            type.accessibility != kProtectedAccessibility)
        {
            continue;
        }
        std::string contract;
        number(contract, type.kind);
        number(contract, type.flags & ~kPartialTypeFlag);
        field(contract, type.fullyQualifiedName);
        field(contract, type.baseTypeKey);
        strings(contract, type.interfaceTypeKeys, true);
        field(contract, type.enumUnderlyingTypeKey);
        number(contract, type.delegateFlags);
        field(contract, type.delegateReturnTypeKey);
        strings(contract, type.delegateParameterTypeKeys, false);
        number(contract, type.delegateReturnDynamicTrait);
        number(contract, type.delegateParameterDynamicTraits.size());
        for (const std::uint8_t dynamicTrait : type.delegateParameterDynamicTraits)
            number(contract, dynamicTrait);
        number(contract, type.variantCases.size());
        for (const LibraryMetadataVariantCase& variantCase : type.variantCases)
        {
            field(contract, variantCase.name);
            field(contract, variantCase.payloadTypeKey);
        }
        strings(contract, type.traitMethodSignatures, false);
        number(contract, type.associatedItems.size());
        for (const LibraryMetadataAssociatedItem& item : type.associatedItems)
        {
            number(contract, item.kind);
            field(contract, item.name);
            field(contract, item.typeKey);
            number(contract, item.hasDefault);
            number(contract, item.valueBits);
        }
        number(contract, type.traitObjectFlags);
        number(contract, type.traitObjectWordCount);
        result.emplace("type:" + type.identityKey, std::move(contract));
    }

    for (const LibraryMetadataMember& member : metadata.members)
    {
        if (member.accessibility != kPublicAccessibility &&
            member.accessibility != kProtectedAccessibility)
        {
            continue;
        }
        std::string contract;
        number(contract, member.kind);
        number(contract, member.accessibility);
        number(contract, member.flags);
        field(contract, member.name);
        field(contract, member.declaredTypeKey);
        number(contract, member.declaredOwnership);
        number(contract, member.constantKind);
        number(contract, member.constantBits);
        field(contract, member.constantText);
        number(contract, member.parameters.size());
        for (const LibraryMetadataParameter& parameter : member.parameters)
            parameterSourceContract(contract, parameter);
        result.emplace(
            "member:" + member.declaringTypeKey + ":" +
                std::to_string(member.kind) + ":" + member.signatureKey,
            std::move(contract));
    }

    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        std::string contract;
        number(contract, function.flags);
        field(contract, function.name);
        field(contract, function.declaredTypeKey);
        number(contract, function.declaredOwnership);
        number(contract, function.parameters.size());
        for (const LibraryMetadataParameter& parameter : function.parameters)
            parameterSourceContract(contract, parameter);
        result.emplace(
            "namespace-function:" + function.namespaceName + ":" + function.signatureKey,
            std::move(contract));
    }

    for (const LibraryMetadataNamespaceState& state : metadata.namespaceStates)
    {
        std::string contract;
        number(contract, state.flags);
        field(contract, state.declaredTypeKey);
        number(contract, state.initializerKind);
        number(contract, state.initializerBits);
        field(contract, state.initializerText);
        result.emplace(
            "namespace-state:" + state.namespaceName + ":" + state.name,
            std::move(contract));
    }

    for (const LibraryMetadataGenericTemplate& generic : metadata.genericTemplates)
    {
        if (generic.accessibility != kPublicAccessibility) continue;
        std::string contract;
        number(contract, generic.kind);
        field(contract, canonicalModule(generic.moduleName));
        field(contract, generic.namespaceName);
        field(contract, generic.ownerName);
        field(contract, generic.name);
        strings(contract, generic.parameters, false);
        strings(contract, generic.dependencies, true);
        // A reusable generic is compiled in the consumer. Its declaration/body is therefore a
        // public compilation input, not a private producer implementation detail.
        field(contract, generic.sourceText);
        result.emplace("generic:" + generic.canonicalKey, std::move(contract));
    }

    for (const LibraryMetadataExport& exported : metadata.exports)
    {
        std::string contract;
        field(contract, exported.declaringTypeKey);
        field(contract, exported.returnTypeKey);
        strings(contract, exported.parameterTypeKeys, false);
        result.emplace("extern-c:" + exported.cAbiName, std::move(contract));
    }
    return result;
}

Surface abiSurface(const LibraryMetadata& metadata)
{
    Surface result;
    std::string header;
    field(header, canonicalModule(metadata.moduleName));
    field(header, metadata.targetTriple);
    number(header, metadata.targetId);
    number(header, metadata.pointerSize);
    number(header, metadata.pointerAlignment);
    number(header, metadata.nativeAbiMajor);
    number(header, metadata.nativeAbiMinor);
    number(header, metadata.runtimeAbiMajor);
    number(header, metadata.runtimeAbiMinor);
    number(header, metadata.manglingAbiVersion);
    number(header, metadata.objectAbiVersion);
    number(header, metadata.delegateAbiVersion);
    number(header, metadata.exceptionAbiVersion);
    number(header, metadata.managedDelegateCellSize);
    result.emplace("module", std::move(header));

    for (const LibraryMetadataType& type : metadata.types)
    {
        std::string contract;
        number(contract, type.kind);
        number(contract, type.flags & ~kPartialTypeFlag);
        field(contract, type.baseTypeKey);
        strings(contract, type.interfaceTypeKeys, false);
        number(contract, type.size);
        number(contract, type.alignment);
        number(contract, type.headerSize);
        number(contract, type.referenceSize);
        number(contract, type.referenceAlignment);
        number(contract, type.stableTypeId);
        field(contract, type.enumUnderlyingTypeKey);
        number(contract, type.delegateFlags);
        field(contract, type.delegateReturnTypeKey);
        strings(contract, type.delegateParameterTypeKeys, false);
        number(contract, type.variantCases.size());
        for (const LibraryMetadataVariantCase& variantCase : type.variantCases)
        {
            field(contract, variantCase.name);
            field(contract, variantCase.payloadTypeKey);
        }
        strings(contract, type.traitMethodSignatures, false);
        number(contract, type.associatedItems.size());
        for (const LibraryMetadataAssociatedItem& item : type.associatedItems)
        {
            number(contract, item.kind);
            field(contract, item.name);
            field(contract, item.typeKey);
            number(contract, item.hasDefault);
            number(contract, item.valueBits);
        }
        number(contract, type.traitObjectFlags);
        number(contract, type.traitObjectWordCount);
        number(contract, type.fields.size());
        for (const LibraryMetadataField& fieldRecord : type.fields)
        {
            field(contract, fieldRecord.name);
            field(contract, fieldRecord.typeKey);
            number(contract, fieldRecord.offset);
            number(contract, fieldRecord.size);
            number(contract, fieldRecord.alignment);
            number(contract, fieldRecord.flags);
            number(contract, fieldRecord.bitOffset);
            number(contract, fieldRecord.bitWidth);
        }
        result.emplace("type:" + type.identityKey, std::move(contract));
    }

    for (const LibraryMetadataMember& member : metadata.members)
    {
        std::string contract;
        number(contract, member.kind);
        number(contract, member.flags & ~kSemanticOnlyMemberFlags);
        field(contract, member.declaredTypeKey);
        field(contract, member.cAbiName);
        field(contract, member.mangledName);
        number(contract, member.parameters.size());
        for (const LibraryMetadataParameter& parameter : member.parameters)
            parameterAbiContract(contract, parameter);
        result.emplace(
            "member:" + member.declaringTypeKey + ":" +
                std::to_string(member.kind) + ":" + member.signatureKey,
            std::move(contract));
    }

    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        std::string contract;
        number(contract, function.flags & static_cast<std::uint8_t>(~16U));
        field(contract, function.declaredTypeKey);
        field(contract, function.mangledName);
        number(contract, function.parameters.size());
        for (const LibraryMetadataParameter& parameter : function.parameters)
            parameterAbiContract(contract, parameter);
        result.emplace(
            "namespace-function:" + function.namespaceName + ":" + function.signatureKey,
            std::move(contract));
    }

    for (const LibraryMetadataNamespaceState& state : metadata.namespaceStates)
    {
        std::string contract;
        number(contract, state.flags & static_cast<std::uint8_t>(~2U));
        field(contract, state.declaredTypeKey);
        field(contract, state.storageName);
        field(contract, state.accessorName);
        result.emplace(
            "namespace-state:" + state.namespaceName + ":" + state.name,
            std::move(contract));
    }

    for (const LibraryMetadataGenericSpecialization& specialization :
         metadata.genericSpecializations)
    {
        std::string contract;
        number(contract, specialization.kind);
        field(contract, specialization.templateKey);
        field(contract, specialization.specializedName);
        strings(contract, specialization.typeArguments, false);
        result.emplace("specialization:" + specialization.specializationKey, std::move(contract));
    }

    for (const LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
    {
        std::string contract;
        number(contract, dispatch.stableTypeId);
        field(contract, dispatch.descriptorName);
        field(contract, dispatch.descriptorAccessorName);
        field(contract, dispatch.vtableName);
        field(contract, dispatch.destructionEntryName);
        strings(contract, dispatch.slotSignatures, false);
        strings(contract, dispatch.slotCalleeNames, false);
        number(contract, dispatch.interfaces.size());
        for (const LibraryMetadataDispatchInterface& interface : dispatch.interfaces)
        {
            field(contract, interface.interfaceTypeKey);
            number(contract, interface.stableInterfaceTypeId);
            field(contract, interface.methodTableName);
            strings(contract, interface.interfaceSignatures, false);
            strings(contract, interface.implementationSignatures, false);
            strings(contract, interface.calleeNames, false);
        }
        result.emplace("dispatch:" + dispatch.typeKey, std::move(contract));
    }

    for (const LibraryMetadataExport& exported : metadata.exports)
    {
        std::string contract;
        field(contract, exported.declaringTypeKey);
        field(contract, exported.returnTypeKey);
        strings(contract, exported.parameterTypeKeys, false);
        field(contract, exported.nativeMangledName);
        result.emplace("extern-c:" + exported.cAbiName, std::move(contract));
    }
    return result;
}

std::string fingerprint(std::string_view schema, const Surface& surface)
{
    std::string projection;
    field(projection, schema);
    number(projection, surface.size());
    for (const auto& [identity, contract] : surface)
    {
        field(projection, identity);
        field(projection, contract);
    }
    const auto bytes = std::span(
        reinterpret_cast<const std::uint8_t*>(projection.data()), projection.size());
    return basic::sha256Hex(basic::sha256(bytes));
}

void compareSurface(
    std::string_view domain,
    const Surface& oldSurface,
    const Surface& newSurface,
    LibraryApiChangeImpact breakingImpact,
    std::vector<LibraryApiChange>& changes)
{
    for (const auto& [identity, oldContract] : oldSurface)
    {
        const auto found = newSurface.find(identity);
        if (found == newSurface.end())
        {
            changes.push_back({breakingImpact, std::string(domain), identity, "removed"});
        }
        else if (found->second != oldContract)
        {
            changes.push_back({breakingImpact, std::string(domain), identity, "contract changed"});
        }
    }
    for (const auto& [identity, ignored] : newSurface)
    {
        (void)ignored;
        if (!oldSurface.contains(identity))
            changes.push_back({LibraryApiChangeImpact::Additive, std::string(domain), identity, "added"});
    }
}
}

LibraryApiDiffReport LibraryApiDiffer::compare(
    const LibraryMetadata& oldMetadata,
    const LibraryMetadata& newMetadata)
{
    LibraryApiDiffReport report;
    const Surface oldSource = sourceSurface(oldMetadata);
    const Surface newSource = sourceSurface(newMetadata);
    const Surface oldAbi = abiSurface(oldMetadata);
    const Surface newAbi = abiSurface(newMetadata);
    report.oldSourceFingerprint = fingerprint("NativeSharp.source-api.v1", oldSource);
    report.newSourceFingerprint = fingerprint("NativeSharp.source-api.v1", newSource);
    report.oldAbiSurfaceFingerprint = fingerprint("NativeSharp.abi-surface.v1", oldAbi);
    report.newAbiSurfaceFingerprint = fingerprint("NativeSharp.abi-surface.v1", newAbi);

    compareSurface(
        "source", oldSource, newSource,
        LibraryApiChangeImpact::SourceBreaking, report.changes);
    compareSurface(
        "abi", oldAbi, newAbi,
        LibraryApiChangeImpact::AbiBreaking, report.changes);

    report.sourceCompatible = std::ranges::none_of(report.changes, [](const LibraryApiChange& change)
    {
        return change.impact == LibraryApiChangeImpact::SourceBreaking;
    });
    report.abiCompatible = std::ranges::none_of(report.changes, [](const LibraryApiChange& change)
    {
        return change.impact == LibraryApiChangeImpact::AbiBreaking;
    });
    std::ranges::sort(report.changes, [](const LibraryApiChange& left, const LibraryApiChange& right)
    {
        return std::tie(left.domain, left.identity, left.impact, left.detail) <
            std::tie(right.domain, right.identity, right.impact, right.detail);
    });
    return report;
}

std::string LibraryApiDiffer::sourceFingerprint(const LibraryMetadata& metadata)
{
    return fingerprint("NativeSharp.source-api.v1", sourceSurface(metadata));
}

std::string LibraryApiDiffer::abiSurfaceFingerprint(const LibraryMetadata& metadata)
{
    return fingerprint("NativeSharp.abi-surface.v1", abiSurface(metadata));
}

const char* LibraryApiDiffer::impactName(LibraryApiChangeImpact impact) noexcept
{
    switch (impact)
    {
        case LibraryApiChangeImpact::Additive: return "additive";
        case LibraryApiChangeImpact::SourceBreaking: return "source-breaking";
        case LibraryApiChangeImpact::AbiBreaking: return "abi-breaking";
    }
    return "unknown";
}

std::string LibraryApiDiffer::classification(const LibraryApiDiffReport& report)
{
    if (report.changes.empty()) return "identical";
    if (report.sourceCompatible && report.abiCompatible) return "additive";
    if (!report.sourceCompatible && !report.abiCompatible) return "source-and-abi-breaking";
    return report.sourceCompatible ? "abi-breaking" : "source-breaking";
}
}
