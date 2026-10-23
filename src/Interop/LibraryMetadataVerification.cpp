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

bool verifyMetadataEnvelope(
    const LibraryMetadata& metadata,
    std::string& error)
{
    if ((metadata.formatMajor != kFormatMajor || metadata.formatMinor > kFormatMinor) &&
        !metadata.payloadDecodedByCurrentReader)
    {
        error = "metadata format expected NSMETA " + std::to_string(kFormatMajor) +
            ".0 through " + std::to_string(kFormatMajor) + "." +
            std::to_string(kFormatMinor) + ", found " +
            std::to_string(metadata.formatMajor) + "." +
            std::to_string(metadata.formatMinor);
        return false;
    }
    if (metadata.targetId != kWindowsX64Target)
    {
        error = "metadata target ABI mismatch: target id expected " +
            std::to_string(kWindowsX64Target) + ", found " +
            std::to_string(metadata.targetId);
        return false;
    }
    if (metadata.pointerSize != kPointerSize ||
        metadata.pointerAlignment != kPointerAlignment)
    {
        error = "metadata target ABI mismatch: pointer size/alignment expected " +
            std::to_string(kPointerSize) + "/" +
            std::to_string(kPointerAlignment) + ", found " +
            std::to_string(metadata.pointerSize) + "/" +
            std::to_string(metadata.pointerAlignment);
        return false;
    }
    if (metadata.targetTriple != kTargetTriple)
    {
        error = "metadata target ABI mismatch: target triple expected '" +
            std::string(kTargetTriple) + "', found '" + metadata.targetTriple + "'";
        return false;
    }
    if (metadata.formatMinor >= 8 && !verifyFrozenAbiContract(metadata, error)) return false;
    if (metadata.formatMinor >= 8 && metadata.moduleName.empty())
    {
        error = "metadata module identity is empty";
        return false;
    }
    return true;
}
bool typeDirectlyOwnsStorage(const LibraryMetadataType& type)
{
    for (const LibraryMetadataField& field : type.fields)
        if (field.ownership == LibraryMetadataOwnershipKind::Owned) return true;
    return false;
}

bool verifyMetadataTypeValueContract(
    const LibraryMetadata& metadata,
    const LibraryMetadataType& type,
    std::string& error)
{
    const std::uint16_t recordFlags =
        type.flags & (kMetadataTypeRecord | kMetadataTypeRecordDeriveMask);
    if (metadata.formatMinor < 26 && recordFlags != 0U)
    {
        error = "metadata before NSMETA 1.26 contains record derive flags on type '" +
            type.fullyQualifiedName + "'";
        return false;
    }
    if ((recordFlags & kMetadataTypeRecordDeriveMask) != 0U &&
        (recordFlags & kMetadataTypeRecord) == 0U)
    {
        error = "metadata type '" + type.fullyQualifiedName +
            "' publishes record derives without the record flag";
        return false;
    }
    if ((recordFlags & kMetadataTypeRecord) != 0U &&
        (type.kind != LibraryMetadataTypeKind::Struct || (type.flags & (1U << 5U)) != 0U))
    {
        error = "metadata record '" + type.fullyQualifiedName +
            "' must be a non-union struct";
        return false;
    }
    if (metadata.formatMinor < 36 && !type.dropGlueEntryName.empty())
    {
        error = "metadata before NSMETA 1.36 contains value drop glue on type '" +
            type.fullyQualifiedName + "'";
        return false;
    }
    const std::string expectedDropGlue = "__nsc_drop_value_" +
        compilation::symbolFragmentForIdentityKey("type:" + type.identityKey);
    if (!type.dropGlueEntryName.empty() &&
        (type.kind != LibraryMetadataTypeKind::Struct ||
         type.dropGlueEntryName != expectedDropGlue))
    {
        error = "metadata type '" + type.fullyQualifiedName +
            "' has an invalid value drop-glue contract";
        return false;
    }
    if (metadata.formatMinor >= 36 &&
        type.kind == LibraryMetadataTypeKind::Struct &&
        typeDirectlyOwnsStorage(type) && type.dropGlueEntryName.empty())
    {
        error = "metadata owning value type '" + type.fullyQualifiedName +
            "' has no producer drop-glue entry";
        return false;
    }
    return true;
}

bool verifyMetadataTypeCopyAndOpaqueContract(
    const LibraryMetadata& metadata,
    const LibraryMetadataType& type,
    std::string& error)
{
    const bool isCopy = (type.flags & kMetadataTypeCopy) != 0U;
    if (metadata.formatMinor < 33 && isCopy)
    {
        error = "metadata before NSMETA 1.33 contains a Copy flag on type '" +
            type.fullyQualifiedName + "'";
        return false;
    }
    if (isCopy && type.kind != LibraryMetadataTypeKind::Struct)
    {
        error = "metadata Copy capability on '" + type.fullyQualifiedName +
            "' requires a struct value type";
        return false;
    }
    const bool isOpaqueResult = (type.flags & kMetadataTypeOpaqueResult) != 0U;
    if (metadata.formatMinor < 30 && isOpaqueResult)
    {
        error = "metadata before NSMETA 1.30 contains an opaque-result flag on type '" +
            type.fullyQualifiedName + "'";
        return false;
    }
    if (isOpaqueResult &&
        (type.kind != LibraryMetadataTypeKind::Struct ||
         type.fields.size() != 1U || type.interfaceTypeKeys.empty()))
    {
        error = "metadata opaque result '" + type.fullyQualifiedName +
            "' must be a one-field struct implementing its exposed trait";
        return false;
    }
    if (isOpaqueResult &&
        (type.fields.front().name != "__nsc_opaque_value" ||
         type.size != type.fields.front().size ||
         type.alignment != type.fields.front().alignment))
    {
        error = "metadata opaque result '" + type.fullyQualifiedName +
            "' has an inconsistent in-place representation";
        return false;
    }
    return true;
}

bool verifyMetadataTraitMethodContract(
    const LibraryMetadataType& type,
    bool isTrait,
    std::string& error)
{
    if (!isTrait && !type.traitMethodSignatures.empty())
    {
        error = "metadata non-trait '" + type.fullyQualifiedName +
            "' contains a trait method catalogue";
        return false;
    }
    std::unordered_set<std::string> traitSignatures;
    for (const std::string& signature : type.traitMethodSignatures)
    {
        if (signature.empty() || !traitSignatures.insert(signature).second)
        {
            error = "metadata trait '" + type.fullyQualifiedName +
                "' contains an empty or duplicate method signature";
            return false;
        }
    }
    return true;
}

bool verifyMetadataAssociatedItemContracts(
    const LibraryMetadata& metadata,
    const LibraryMetadataType& type,
    bool isTrait,
    std::string& error)
{
    if (metadata.formatMinor < 29 && !type.associatedItems.empty())
    {
        error = "metadata before NSMETA 1.29 contains associated items on type '" +
            type.fullyQualifiedName + "'";
        return false;
    }
    if (!type.associatedItems.empty() &&
        type.kind != LibraryMetadataTypeKind::Class &&
        type.kind != LibraryMetadataTypeKind::Struct && !isTrait)
    {
        error = "metadata type '" + type.fullyQualifiedName +
            "' cannot own associated items";
        return false;
    }
    std::unordered_set<std::string> associatedNames;
    for (const LibraryMetadataAssociatedItem& item : type.associatedItems)
    {
        if (item.name.empty() || !associatedNames.insert(item.name).second)
        {
            error = "metadata type '" + type.fullyQualifiedName +
                "' contains an empty or duplicate associated item name";
            return false;
        }
        if (item.kind == LibraryMetadataAssociatedItemKind::Type)
        {
            if (item.hasDefault != !item.typeKey.empty() || item.valueBits != 0U)
            {
                error = "metadata associated type '" + type.fullyQualifiedName + "." +
                    item.name + "' has inconsistent default data";
                return false;
            }
        }
        else if (item.typeKey.empty() || (!item.hasDefault && item.valueBits != 0U))
        {
            error = "metadata associated constant '" + type.fullyQualifiedName + "." +
                item.name + "' has inconsistent type/default data";
            return false;
        }
        if (!isTrait && !item.hasDefault)
        {
            error = "metadata implementation type '" + type.fullyQualifiedName +
                "' contains an undefined associated item '" + item.name + "'";
            return false;
        }
    }
    return true;
}

bool verifyMetadataTypeTraitContract(
    const LibraryMetadata& metadata,
    const LibraryMetadataType& type,
    std::string& error)
{
    const bool isTrait = (type.flags & kMetadataTypeTrait) != 0U;
    if (metadata.formatMinor < 28 && isTrait)
    {
        error = "metadata before NSMETA 1.28 contains a trait flag on type '" +
            type.fullyQualifiedName + "'";
        return false;
    }
    if (isTrait && type.kind != LibraryMetadataTypeKind::Interface)
    {
        error = "metadata trait '" + type.fullyQualifiedName +
            "' must use the interface ABI kind";
        return false;
    }
    constexpr std::uint8_t kKnownTraitObjectFlags =
        kMetadataTraitObjectObjectSafe |
        kMetadataTraitObjectDescriptorMap |
        kMetadataTraitObjectNullIsZero |
        kMetadataTraitObjectNoConversionAllocation;
    if (metadata.formatMinor < 31 &&
        (type.traitObjectFlags != 0U || type.traitObjectWordCount != 0U))
    {
        error = "metadata before NSMETA 1.31 contains a trait-object ABI contract on type '" +
            type.fullyQualifiedName + "'";
        return false;
    }
    if (!isTrait && (type.traitObjectFlags != 0U || type.traitObjectWordCount != 0U))
    {
        error = "metadata non-trait '" + type.fullyQualifiedName +
            "' publishes a trait-object ABI contract";
        return false;
    }
    if (isTrait && metadata.formatMinor >= 31 &&
        ((type.traitObjectFlags & ~kKnownTraitObjectFlags) != 0U ||
         (type.traitObjectFlags & (kMetadataTraitObjectDescriptorMap |
                                   kMetadataTraitObjectNullIsZero |
                                   kMetadataTraitObjectNoConversionAllocation)) !=
             (kMetadataTraitObjectDescriptorMap |
              kMetadataTraitObjectNullIsZero |
              kMetadataTraitObjectNoConversionAllocation) ||
         type.traitObjectWordCount != 1U))
    {
        error = "metadata trait '" + type.fullyQualifiedName +
            "' has an invalid Windows x64 dyn representation";
        return false;
    }
    if (!verifyMetadataTraitMethodContract(type, isTrait, error)) return false;
    return verifyMetadataAssociatedItemContracts(metadata, type, isTrait, error);
}

bool verifyMetadataTypeVariantAndIdentity(
    const LibraryMetadata& metadata,
    const LibraryMetadataType& type,
    std::unordered_set<std::string>& typeIdentities,
    std::string& error)
{
    const bool isVariant = (type.flags & kMetadataTypeVariant) != 0U;
    if (metadata.formatMinor < 27 && (isVariant || !type.variantCases.empty()))
    {
        error = "metadata before NSMETA 1.27 contains tagged-variant data on type '" +
            type.fullyQualifiedName + "'";
        return false;
    }
    if (isVariant &&
        (type.kind != LibraryMetadataTypeKind::Struct ||
         (type.flags & (1U << 5U)) != 0U ||
         (type.flags & kMetadataTypeRecord) != 0U))
    {
        error = "metadata variant '" + type.fullyQualifiedName +
            "' must be a non-union, non-record struct";
        return false;
    }
    if (isVariant && type.variantCases.empty())
    {
        error = "metadata variant '" + type.fullyQualifiedName +
            "' has an empty case catalogue";
        return false;
    }
    if (!isVariant && !type.variantCases.empty())
    {
        error = "metadata type '" + type.fullyQualifiedName +
            "' publishes variant cases without the variant flag";
        return false;
    }
    std::unordered_set<std::string> variantCaseNames;
    for (const LibraryMetadataVariantCase& variantCase : type.variantCases)
    {
        if (variantCase.name.empty() || !variantCaseNames.insert(variantCase.name).second)
        {
            error = "metadata variant '" + type.fullyQualifiedName +
                "' has an empty or duplicate case name";
            return false;
        }
    }
    if (type.identityKey.empty() || type.fullyQualifiedName.empty())
    {
        error = "metadata contains a type with an empty canonical identity or name";
        return false;
    }
    if (!typeIdentities.insert(type.identityKey).second)
    {
        error = "metadata contains duplicate type identity '" + type.identityKey + "'";
        return false;
    }
    return true;
}

bool verifyMetadataTypeDeclarations(
    const LibraryMetadata& metadata,
    std::string& error)
{
    // Un checksum valido demuestra integridad de bytes, no coherencia del modelo.
    // Un fichero puede estar firmado por su propio productor y aun publicar dos
    // identidades iguales o tablas paralelas de longitudes distintas. Esas formas
    // se rechazan aqui antes de materializar fuente sintetica o comparar simbolos
    // fisicos.
    std::unordered_set<std::string> typeIdentities;
    for (const LibraryMetadataType& type : metadata.types)
    {
        if (!verifyMetadataTypeValueContract(metadata, type, error)) return false;
        if (!verifyMetadataTypeCopyAndOpaqueContract(metadata, type, error)) return false;
        if (!verifyMetadataTypeTraitContract(metadata, type, error)) return false;
        if (!verifyMetadataTypeVariantAndIdentity(
                metadata, type, typeIdentities, error))
        {
            return false;
        }
    }
    return true;
}

bool verifyMetadataReferenceAndDelegateContracts(
    const LibraryMetadata& metadata,
    std::string& error)
{
    for (const LibraryMetadataType& type : metadata.types)
    {
        if ((type.kind == LibraryMetadataTypeKind::Class ||
             type.kind == LibraryMetadataTypeKind::Interface ||
             (metadata.formatMinor >= 8 &&
              type.kind == LibraryMetadataTypeKind::Delegate)) &&
            (type.referenceSize != metadata.pointerSize ||
             type.referenceAlignment != metadata.pointerAlignment))
        {
            error = "metadata reference ABI for type '" + type.fullyQualifiedName + "' is " +
                std::to_string(type.referenceSize) + "/" +
                std::to_string(type.referenceAlignment) +
                ", but the target pointer ABI is " +
                std::to_string(metadata.pointerSize) + "/" +
                std::to_string(metadata.pointerAlignment);
            return false;
        }
        if (metadata.formatMinor >= 5 &&
            type.kind == LibraryMetadataTypeKind::Delegate &&
            type.delegateReturnTypeKey.empty())
        {
            error = "metadata delegate '" + type.fullyQualifiedName +
                "' does not provide its callable signature";
            return false;
        }
        if (metadata.formatMinor < 31) continue;
        const bool isDelegate = type.kind == LibraryMetadataTypeKind::Delegate;
        const bool dynSignaturePresent = type.delegateReturnDynamicTrait ||
            !type.delegateParameterDynamicTraits.empty();
        if (!isDelegate && dynSignaturePresent)
        {
            error = "metadata non-delegate '" + type.fullyQualifiedName +
                "' publishes a delegate dyn signature";
            return false;
        }
        if (isDelegate &&
            type.delegateParameterDynamicTraits.size() !=
                type.delegateParameterTypeKeys.size())
        {
            error = "metadata delegate '" + type.fullyQualifiedName +
                "' has an incomplete dyn parameter catalogue";
            return false;
        }
        for (const std::uint8_t value : type.delegateParameterDynamicTraits)
        {
            if (value <= 1U) continue;
            error = "metadata delegate '" + type.fullyQualifiedName +
                "' contains an invalid dyn parameter marker";
            return false;
        }
    }
    return true;
}

bool verifyDispatchInterfaceMetadata(
    const LibraryMetadataDispatch& dispatch,
    const LibraryMetadataDispatchInterface& interface,
    const std::unordered_map<std::string, const LibraryMetadataMember*>& membersByCallee,
    std::string& error)
{
    if (interface.interfaceSignatures.size() !=
            interface.implementationSignatures.size() ||
        interface.interfaceSignatures.size() != interface.calleeNames.size())
    {
        error = "metadata interface dispatch contract '" +
            interface.interfaceTypeKey + "' on '" + dispatch.typeKey +
            "' has inconsistent slot counts";
        return false;
    }
    for (std::size_t slot = 0; slot < interface.interfaceSignatures.size(); ++slot)
    {
        if (interface.interfaceSignatures[slot].empty())
        {
            error = "metadata interface dispatch contract '" +
                interface.interfaceTypeKey + "' on '" + dispatch.typeKey +
                "' has an empty interface signature at slot " + std::to_string(slot);
            return false;
        }
        const std::string& callee = interface.calleeNames[slot];
        const auto localCallee = membersByCallee.find(callee);
        if (!callee.empty() && localCallee != membersByCallee.end() &&
            localCallee->second->signatureKey != interface.implementationSignatures[slot])
        {
            error = "metadata interface dispatch slot " + std::to_string(slot) +
                " for '" + interface.interfaceTypeKey + "' on '" + dispatch.typeKey +
                "' names callee '" + callee + "' with signature '" +
                localCallee->second->signatureKey + "', expected '" +
                interface.implementationSignatures[slot] + "'";
            return false;
        }
    }
    return true;
}

bool verifyDispatchSlots(
    const LibraryMetadataDispatch& dispatch,
    const std::unordered_map<std::string, const LibraryMetadataMember*>& membersByCallee,
    std::string& error)
{
    if (dispatch.slotSignatures.size() != dispatch.slotCalleeNames.size())
    {
        error = "metadata dispatch contract for '" + dispatch.typeKey + "' has " +
            std::to_string(dispatch.slotSignatures.size()) + " slot signatures but " +
            std::to_string(dispatch.slotCalleeNames.size()) + " callees";
        return false;
    }
    for (std::size_t slot = 0; slot < dispatch.slotSignatures.size(); ++slot)
    {
        if (dispatch.slotSignatures[slot].empty())
        {
            error = "metadata dispatch contract for '" + dispatch.typeKey +
                "' has an empty signature at slot " + std::to_string(slot);
            return false;
        }
        const std::string& callee = dispatch.slotCalleeNames[slot];
        const auto localCallee = membersByCallee.find(callee);
        if (!callee.empty() && localCallee != membersByCallee.end() &&
            localCallee->second->signatureKey != dispatch.slotSignatures[slot])
        {
            error = "metadata dispatch slot " + std::to_string(slot) + " for '" +
                dispatch.typeKey + "' names callee '" + callee + "' with signature '" +
                localCallee->second->signatureKey + "', expected '" +
                dispatch.slotSignatures[slot] + "'";
            return false;
        }
    }
    return true;
}

bool verifyDispatchRecord(
    const LibraryMetadataDispatch& dispatch,
    const std::unordered_map<std::string, const LibraryMetadataType*>& typesByKey,
    const std::unordered_map<std::string, const LibraryMetadataMember*>& membersByCallee,
    std::unordered_set<std::string>& dispatchTypes,
    std::string& error)
{
    if (!dispatchTypes.insert(dispatch.typeKey).second)
    {
        error = "metadata contains duplicate dispatch contract for '" +
            dispatch.typeKey + "'";
        return false;
    }
    const auto dispatchedType = typesByKey.find(dispatch.typeKey);
    if (dispatchedType == typesByKey.end() ||
        dispatchedType->second->kind != LibraryMetadataTypeKind::Class)
    {
        error = "metadata dispatch contract refers to unknown or non-class type '" +
            dispatch.typeKey + "'";
        return false;
    }
    if (dispatch.descriptorAccessorName.empty())
    {
        error = "metadata dispatch contract for '" + dispatch.typeKey +
            "' does not provide its class-descriptor accessor";
        return false;
    }
    if (dispatch.destructionEntryName.empty())
    {
        error = "metadata dispatch contract for '" + dispatch.typeKey +
            "' does not provide its producer-owned destruction entry";
        return false;
    }
    if (!verifyDispatchSlots(dispatch, membersByCallee, error)) return false;
    for (const LibraryMetadataDispatchInterface& interface : dispatch.interfaces)
    {
        if (!verifyDispatchInterfaceMetadata(
                dispatch, interface, membersByCallee, error))
        {
            return false;
        }
    }
    return true;
}

// Dispatch tables by type key (the first table of each key, as the former linear search) and,
// for each table, its slots by signature in ascending slot order. Built once per metadata.
struct MetadataDispatchIndex final
{
    std::unordered_map<std::string, const LibraryMetadataDispatch*> tablesByType;
    std::unordered_map<
        const LibraryMetadataDispatch*,
        std::unordered_map<std::string, std::vector<std::size_t>>> slotsBySignature;
};

/// Builds both indexes of metadata's dispatch tables.
MetadataDispatchIndex buildMetadataDispatchIndex(const LibraryMetadata& metadata)
{
    MetadataDispatchIndex index;
    for (const LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
    {
        index.tablesByType.emplace(dispatch.typeKey, &dispatch);
        auto& slots = index.slotsBySignature[&dispatch];
        for (std::size_t slot = 0; slot < dispatch.slotSignatures.size(); ++slot)
            slots[dispatch.slotSignatures[slot]].push_back(slot);
    }
    return index;
}

const LibraryMetadataDispatch* findMetadataDispatch(
    const MetadataDispatchIndex& index,
    const std::string& typeKey)
{
    const auto found = index.tablesByType.find(typeKey);
    return found == index.tablesByType.end() ? nullptr : found->second;
}

std::optional<std::size_t> findMetadataDispatchSlot(
    const MetadataDispatchIndex& index,
    const LibraryMetadataDispatch& dispatch,
    const LibraryMetadataMember& member,
    std::uint32_t newSlotFlag)
{
    // Only the slots whose signature matches, in the order the former scan met them.
    const auto table = index.slotsBySignature.find(&dispatch);
    if (table == index.slotsBySignature.end()) return std::nullopt;
    const auto matching = table->second.find(member.signatureKey);
    if (matching == table->second.end()) return std::nullopt;
    for (const std::size_t slot : matching->second)
    {
        if ((member.flags & newSlotFlag) == 0U) return slot;
        if (slot < dispatch.slotCalleeNames.size() &&
            dispatch.slotCalleeNames[slot] == member.mangledName)
        {
            return slot;
        }
    }
    return std::nullopt;
}

bool verifyVirtualMemberDispatch(
    const MetadataDispatchIndex& dispatchIndex,
    const LibraryMetadataMember& member,
    const std::unordered_map<std::string, const LibraryMetadataType*>& typesByKey,
    std::string& error)
{
    constexpr std::uint8_t kMethodKind = 2U;
    constexpr std::uint32_t kVirtual = 1U << 3U;
    constexpr std::uint32_t kOverride = 1U << 4U;
    constexpr std::uint32_t kAbstract = 1U << 5U;
    constexpr std::uint32_t kNewSlot = 1U << 19U;
    if (member.kind != kMethodKind ||
        (member.flags & (kVirtual | kOverride | kAbstract)) == 0U)
    {
        return true;
    }
    const auto declaringType = typesByKey.find(member.declaringTypeKey);
    if (declaringType == typesByKey.end() ||
        declaringType->second->kind == LibraryMetadataTypeKind::Interface)
    {
        return true;
    }
    const LibraryMetadataDispatch* dispatch =
        findMetadataDispatch(dispatchIndex, member.declaringTypeKey);
    if (dispatch == nullptr)
    {
        error = "metadata virtual member '" + member.declaringTypeKey + "." +
            member.signatureKey + "' has no declaring-type dispatch contract";
        return false;
    }
    const std::optional<std::size_t> slot =
        findMetadataDispatchSlot(dispatchIndex, *dispatch, member, kNewSlot);
    if (!slot.has_value())
    {
        error = "metadata virtual member '" + member.declaringTypeKey + "." +
            member.signatureKey + "' has no matching dispatch slot";
        return false;
    }
    const std::string& callee = dispatch->slotCalleeNames[*slot];
    if ((member.flags & kAbstract) != 0U)
    {
        if (callee.empty()) return true;
        error = "metadata abstract member '" + member.declaringTypeKey + "." +
            member.signatureKey + "' unexpectedly owns dispatch callee '" + callee + "'";
        return false;
    }
    if (!member.mangledName.empty() && callee == member.mangledName) return true;
    error = "metadata concrete virtual member '" + member.declaringTypeKey + "." +
        member.signatureKey + "' publishes callee '" + member.mangledName +
        "' but its dispatch slot names '" + callee + "'";
    return false;
}

bool verifyMetadataDispatchContracts(
    const LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor < 6) return true;
    std::unordered_map<std::string, const LibraryMetadataType*> typesByKey;
    for (const LibraryMetadataType& type : metadata.types)
        typesByKey.emplace("type:" + type.identityKey, &type);
    std::unordered_map<std::string, const LibraryMetadataMember*> membersByCallee;
    for (const LibraryMetadataMember& member : metadata.members)
    {
        if (member.mangledName.empty()) continue;
        if (!membersByCallee.emplace(member.mangledName, &member).second)
        {
            error = "metadata contains duplicate NativeSharp callee '" +
                member.mangledName + "'";
            return false;
        }
    }
    std::unordered_set<std::string> dispatchTypes;
    for (const LibraryMetadataDispatch& dispatch : metadata.dispatchTables)
    {
        if (!verifyDispatchRecord(
                dispatch, typesByKey, membersByCallee, dispatchTypes, error))
        {
            return false;
        }
    }
    const MetadataDispatchIndex dispatchIndex = buildMetadataDispatchIndex(metadata);
    for (const LibraryMetadataMember& member : metadata.members)
        if (!verifyVirtualMemberDispatch(dispatchIndex, member, typesByKey, error)) return false;
    return true;
}

bool verifyGenericParameterContracts(
    const LibraryMetadataGenericTemplate& generic,
    std::string& error)
{
    if (generic.parameterContracts.size() != generic.parameters.size())
    {
        error = "metadata generic template '" + generic.canonicalKey +
            "' has a parameter-contract count that differs from its generic arity";
        return false;
    }
    std::unordered_set<std::string> parameterNames;
    for (std::size_t ordinal = 0; ordinal < generic.parameterContracts.size(); ++ordinal)
    {
        const LibraryMetadataGenericTemplate::ParameterContract& parameter =
            generic.parameterContracts[ordinal];
        const bool constantParameter = parameter.kind == 1U || parameter.kind == 3U;
        if (parameter.name != generic.parameters[ordinal] ||
            !parameterNames.insert(parameter.name).second || parameter.kind > 3U ||
            (parameter.flags & ~kMetadataGenericConstraintMask) != 0U ||
            (constantParameter && parameter.constantTypeKey.empty()) ||
            (!constantParameter && !parameter.constantTypeKey.empty()))
        {
            error = "metadata generic template '" + generic.canonicalKey +
                "' contains an invalid parameter ownership/constraint contract";
            return false;
        }
        std::unordered_set<std::string> typeConstraints;
        for (const std::string& constraint : parameter.typeConstraintKeys)
        {
            if (constraint.empty() || !typeConstraints.insert(constraint).second)
            {
                error = "metadata generic template '" + generic.canonicalKey +
                    "' contains an invalid or duplicate nominal constraint";
                return false;
            }
        }
    }
    return true;
}

bool verifyGenericLifetimeContracts(
    const LibraryMetadataGenericTemplate& generic,
    std::string& error)
{
    std::unordered_set<std::string> lifetimeNames;
    for (const std::string& lifetime : generic.lifetimeParameters)
    {
        if (lifetime.empty() || !lifetimeNames.insert(lifetime).second)
        {
            error = "metadata generic template '" + generic.canonicalKey +
                "' contains an invalid lifetime binder";
            return false;
        }
    }
    std::set<std::pair<std::string, std::string>> lifetimeEdges;
    for (const auto& edge : generic.lifetimeOutlivesConstraints)
    {
        if (!lifetimeNames.contains(edge.first) ||
            !lifetimeNames.contains(edge.second) ||
            edge.first == edge.second || !lifetimeEdges.insert(edge).second)
        {
            error = "metadata generic template '" + generic.canonicalKey +
                "' contains an invalid lifetime outlives edge";
            return false;
        }
    }
    return true;
}

bool verifyGenericTemplateDependencies(
    const LibraryMetadataGenericTemplate& generic,
    std::string& error)
{
    std::unordered_set<std::string> dependencyKeys;
    for (const std::string& dependency : generic.dependencies)
    {
        if (dependency.empty() || !dependencyKeys.insert(dependency).second)
        {
            error = dependency.empty()
                ? "metadata generic template '" + generic.canonicalKey +
                    "' contains an empty closure dependency"
                : "metadata generic template '" + generic.canonicalKey +
                    "' contains duplicate closure dependency '" + dependency + "'";
            return false;
        }
    }
    return true;
}

bool verifyGenericTemplateRecord(
    const LibraryMetadata& metadata,
    const LibraryMetadataGenericTemplate& generic,
    std::unordered_set<std::string>& templateKeys,
    std::unordered_map<std::string, const LibraryMetadataGenericTemplate*>& templateByKey,
    std::string& error)
{
    constexpr std::uint8_t kPublicAccessibility = 3U;
    if (generic.canonicalKey.empty() || !templateKeys.insert(generic.canonicalKey).second)
    {
        error = generic.canonicalKey.empty()
            ? "metadata contains a generic template with an empty canonical key"
            : "metadata contains duplicate generic template '" + generic.canonicalKey + "'";
        return false;
    }
    if (generic.accessibility > kPublicAccessibility || generic.sourceText.empty())
    {
        error = "metadata generic template '" + generic.canonicalKey +
            "' does not provide a valid materializable declaration";
        return false;
    }
    if (metadata.formatMinor >= 38 &&
        (!verifyGenericParameterContracts(generic, error) ||
         !verifyGenericLifetimeContracts(generic, error)))
    {
        return false;
    }
    templateByKey.emplace(generic.canonicalKey, &generic);
    return metadata.formatMinor < 11 ||
        verifyGenericTemplateDependencies(generic, error);
}

bool verifyGenericTemplateClosure(
    const LibraryMetadataGenericTemplate& generic,
    const LibraryMetadataGenericTemplate& publicRoot,
    const std::string& sameModulePrefix,
    const std::string& moduleName,
    const std::unordered_map<std::string, const LibraryMetadataGenericTemplate*>& templateByKey,
    std::vector<std::string>& chain,
    std::unordered_set<std::string>& visiting,
    std::unordered_set<std::string>& verifiedTemplates,
    std::string& error)
{
    // A template whose walk already completed has a closure without missing same-module
    // dependencies (a missing one would have stopped the verification), so walking it again
    // cannot report anything.
    if (verifiedTemplates.contains(generic.canonicalKey)) return true;
    if (!visiting.insert(generic.canonicalKey).second) return true;
    chain.push_back(generic.canonicalKey);
    for (const std::string& dependencyKey : generic.dependencies)
    {
        const auto dependency = templateByKey.find(dependencyKey);
        if (dependency == templateByKey.end())
        {
            if (!dependencyKey.starts_with(sameModulePrefix)) continue;
            error = "reusable template closure is incomplete\n  public root: " +
                publicRoot.canonicalKey + "\n  required template: " + dependencyKey +
                "\n  owner: " + moduleName + "\n  dependency chain: ";
            for (const std::string& entry : chain) error += entry + " -> ";
            error += dependencyKey;
            return false;
        }
        if (!verifyGenericTemplateClosure(
                *dependency->second,
                publicRoot,
                sameModulePrefix,
                moduleName,
                templateByKey,
                chain,
                visiting,
                verifiedTemplates,
                error))
        {
            return false;
        }
    }
    chain.pop_back();
    visiting.erase(generic.canonicalKey);
    verifiedTemplates.insert(generic.canonicalKey);
    return true;
}

bool verifyGenericTemplates(
    const LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor < 7) return true;
    constexpr std::uint8_t kPublicAccessibility = 3U;
    std::unordered_set<std::string> templateKeys;
    std::unordered_map<std::string, const LibraryMetadataGenericTemplate*> templateByKey;
    for (const LibraryMetadataGenericTemplate& generic : metadata.genericTemplates)
    {
        if (!verifyGenericTemplateRecord(
                metadata, generic, templateKeys, templateByKey, error))
        {
            return false;
        }
    }
    if (metadata.formatMinor < 11) return true;
    const std::string sameModulePrefix = metadata.moduleName + "::";
    // Shared by every public root: a closure verified from one root is not walked again.
    std::unordered_set<std::string> verifiedTemplates;
    for (const LibraryMetadataGenericTemplate& publicRoot : metadata.genericTemplates)
    {
        if (publicRoot.accessibility != kPublicAccessibility) continue;
        std::vector<std::string> chain;
        std::unordered_set<std::string> visiting;
        if (!verifyGenericTemplateClosure(
                publicRoot,
                publicRoot,
                sameModulePrefix,
                metadata.moduleName,
                templateByKey,
                chain,
                visiting,
                verifiedTemplates,
                error))
        {
            return false;
        }
    }
    return true;
}

bool verifyGenericSpecializations(
    const LibraryMetadata& metadata,
    std::unordered_set<std::string>& specializedTypeKeys,
    std::string& error)
{
    std::unordered_set<std::string> specializationKeys;
    for (const LibraryMetadataGenericSpecialization& specialization :
         metadata.genericSpecializations)
    {
        const bool incomplete = specialization.templateKey.empty() ||
            specialization.specializationKey.empty() ||
            specialization.specializedName.empty() ||
            specialization.typeArguments.empty();
        if (incomplete ||
            !specializationKeys.insert(specialization.specializationKey).second)
        {
            error = incomplete
                ? "metadata contains an incomplete generic specialization declaration"
                : "metadata contains duplicate generic specialization '" +
                    specialization.specializationKey + "'";
            return false;
        }
        const std::optional<std::string> typeKey = specializedTypeKey(specialization);
        if (typeKey.has_value() && !specializedTypeKeys.insert(*typeKey).second)
        {
            error = "metadata contains duplicate closed generic type declaration '" +
                *typeKey + "'";
            return false;
        }
    }
    return true;
}

bool collectVerifiedExternalTypes(
    const LibraryMetadata& metadata,
    const std::unordered_set<std::string>& declaredTypeKeys,
    const std::unordered_set<std::string>& specializedTypeKeys,
    std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
    if (metadata.formatMinor < 12) return true;
    for (const std::string& typeKey : metadata.externalTypeKeys)
    {
        const std::string base = metadataBaseTypeKey(typeKey);
        const std::optional<std::string> module = metadataTypeModule(base);
        if (base != typeKey || !module.has_value() || *module == metadata.moduleName ||
            !externalTypeKeys.insert(typeKey).second)
        {
            error = base != typeKey || !module.has_value()
                ? "metadata contains malformed external type dependency '" + typeKey + "'"
                : *module == metadata.moduleName
                    ? "metadata marks its own type '" + typeKey +
                        "' as an external dependency"
                    : "metadata contains duplicate external type dependency '" + typeKey + "'";
            return false;
        }
        if (declaredTypeKeys.contains(typeKey) || specializedTypeKeys.contains(typeKey))
        {
            error = "metadata both declares and imports type '" + typeKey + "'";
            return false;
        }
    }
    return true;
}

bool verifyGenericAndExternalTypeCatalogues(
    const LibraryMetadata& metadata,
    std::unordered_set<std::string>& declaredTypeKeys,
    std::unordered_set<std::string>& specializedTypeKeys,
    std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
    if (!verifyGenericTemplates(metadata, error)) return false;
    if (!verifyGenericSpecializations(metadata, specializedTypeKeys, error)) return false;
    for (const LibraryMetadataType& type : metadata.types)
        declaredTypeKeys.insert("type:" + type.identityKey);
    return collectVerifiedExternalTypes(
        metadata, declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error);
}

bool verifyMetadataTypeReference(
    const LibraryMetadata& metadata,
    const std::string& writtenTypeKey,
    std::string_view owner,
    const std::unordered_set<std::string>& declaredTypeKeys,
    const std::unordered_set<std::string>& specializedTypeKeys,
    const std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
    if (writtenTypeKey.starts_with("fn:"))
    {
        const std::optional<language::FunctionType> functionType =
            language::parseFunctionTypeIdentity(writtenTypeKey);
        if (!functionType.has_value())
        {
            error = "metadata " + std::string(owner) +
                " contains malformed function type identity '" + writtenTypeKey + "'";
            return false;
        }
        for (const std::string& parameter : functionType->parameters)
            if (!verifyMetadataTypeReference(
                    metadata,
                    parameter,
                    owner,
                    declaredTypeKeys,
                    specializedTypeKeys,
                    externalTypeKeys,
                    error))
            {
                return false;
            }
        if (!verifyMetadataTypeReference(
                metadata,
                functionType->result,
                owner,
                declaredTypeKeys,
                specializedTypeKeys,
                externalTypeKeys,
                error))
        {
            return false;
        }
        if (!functionType->errorType.empty() &&
            !verifyMetadataTypeReference(
                metadata,
                functionType->errorType,
                owner,
                declaredTypeKeys,
                specializedTypeKeys,
                externalTypeKeys,
                error))
        {
            return false;
        }
        return true;
    }
    const std::string typeKey = metadataBaseTypeKey(writtenTypeKey);
    if (!typeKey.starts_with("type:") || declaredTypeKeys.contains(typeKey) ||
        specializedTypeKeys.contains(typeKey) || externalTypeKeys.contains(typeKey))
    {
        return true;
    }
    if (metadata.formatMinor >= 12)
    {
        const std::optional<std::string> module = metadataTypeModule(typeKey);
        if (looksLikeCompilerSpecializedType(typeKey))
        {
            error = "metadata " + std::string(owner) + " refers to closed generic type '" +
                typeKey + "' but no generic specialization declares how to materialize it "
                "and no external type dependency assigns it to a provider";
        }
        else if (module.has_value() && *module != metadata.moduleName)
        {
            error = "metadata " + std::string(owner) + " refers to external type '" +
                typeKey + "' but does not record that LibraryReference dependency";
        }
        else
        {
            error = "metadata " + std::string(owner) + " refers to local type '" +
                typeKey + "' but declares neither the type nor its closed specialization";
        }
        return false;
    }
    if (!looksLikeCompilerSpecializedType(typeKey)) return true;
    error = "metadata " + std::string(owner) + " refers to closed generic type '" +
        typeKey + "' but no generic specialization declares how to materialize it";
    return false;
}

bool verifyMetadataTypeReferences(
    const LibraryMetadata& metadata,
    const std::unordered_set<std::string>& declaredTypeKeys,
    const std::unordered_set<std::string>& specializedTypeKeys,
    const std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
    for (const LibraryMetadataType& type : metadata.types)
    {
        const std::string owner = "type '" + type.identityKey + "'";
        if (!verifyMetadataTypeReference(
                metadata, type.baseTypeKey, owner,
                declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
            return false;
        for (const std::string& interfaceTypeKey : type.interfaceTypeKeys)
            if (!verifyMetadataTypeReference(
                    metadata, interfaceTypeKey, owner,
                    declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
                return false;
        for (const LibraryMetadataField& field : type.fields)
            if (!verifyMetadataTypeReference(
                    metadata, field.typeKey, owner + " field '" + field.name + "'",
                    declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
                return false;
        if (!verifyMetadataTypeReference(
                metadata, type.delegateReturnTypeKey, owner,
                declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
            return false;
        if (!verifyMetadataTypeReference(
                metadata, type.delegateErrorTypeKey, owner + " throws contract",
                declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
            return false;
        for (const std::string& parameterTypeKey : type.delegateParameterTypeKeys)
            if (!verifyMetadataTypeReference(
                    metadata, parameterTypeKey, owner,
                    declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
                return false;
        for (const LibraryMetadataVariantCase& variantCase : type.variantCases)
            if (!verifyMetadataTypeReference(
                    metadata,
                    variantCase.payloadTypeKey,
                    owner + " variant case '" + variantCase.name + "'",
                    declaredTypeKeys,
                    specializedTypeKeys,
                    externalTypeKeys,
                    error))
                return false;
    }
    return true;
}

bool verifyMetadataCallableTypeReferences(
    const LibraryMetadata& metadata,
    const std::unordered_set<std::string>& declaredTypeKeys,
    const std::unordered_set<std::string>& specializedTypeKeys,
    const std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
    for (const LibraryMetadataMember& member : metadata.members)
    {
        const std::string owner =
            "member '" + member.declaringTypeKey + "." + member.signatureKey + "'";
        if (!verifyMetadataTypeReference(
                metadata, member.declaredTypeKey, owner,
                declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
            return false;
        if (!verifyMetadataTypeReference(
                metadata, member.declaredErrorTypeKey, owner + " throws contract",
                declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
            return false;
        for (const LibraryMetadataParameter& parameter : member.parameters)
            if (!verifyMetadataTypeReference(
                    metadata,
                    parameter.typeKey,
                    owner + " parameter '" + parameter.name + "'",
                    declaredTypeKeys,
                    specializedTypeKeys,
                    externalTypeKeys,
                    error))
                return false;
    }
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        const std::string owner = "namespace function '" +
            function.namespaceName + "." + function.signatureKey + "'";
        if (!verifyMetadataTypeReference(
                metadata, function.declaredTypeKey, owner,
                declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
            return false;
        if (!verifyMetadataTypeReference(
                metadata, function.declaredErrorTypeKey, owner + " throws contract",
                declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
            return false;
        for (const LibraryMetadataParameter& parameter : function.parameters)
            if (!verifyMetadataTypeReference(
                    metadata,
                    parameter.typeKey,
                    owner + " parameter '" + parameter.name + "'",
                    declaredTypeKeys,
                    specializedTypeKeys,
                    externalTypeKeys,
                    error))
                return false;
    }
    return true;
}

bool verifyMetadataRemainingTypeReferences(
    const LibraryMetadata& metadata,
    const std::unordered_set<std::string>& declaredTypeKeys,
    const std::unordered_set<std::string>& specializedTypeKeys,
    const std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
    for (const LibraryMetadataNamespaceState& state : metadata.namespaceStates)
    {
        const std::string owner =
            "namespace state '" + state.namespaceName + "." + state.name + "'";
        if (!verifyMetadataTypeReference(
                metadata, state.declaredTypeKey, owner,
                declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
            return false;
    }
    for (const LibraryMetadataGenericSpecialization& specialization :
         metadata.genericSpecializations)
    {
        const std::string owner =
            "generic specialization '" + specialization.specializationKey + "'";
        for (const std::string& argument : specialization.typeArguments)
            if (argument.starts_with("type:") &&
                !verifyMetadataTypeReference(
                    metadata, argument, owner,
                    declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
                return false;
    }
    for (const LibraryMetadataExport& exportRecord : metadata.exports)
    {
        const std::string owner = "export '" + exportRecord.cAbiName + "'";
        if (!verifyMetadataTypeReference(
                metadata, exportRecord.returnTypeKey, owner,
                declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
            return false;
        for (const std::string& parameterTypeKey : exportRecord.parameterTypeKeys)
            if (!verifyMetadataTypeReference(
                    metadata, parameterTypeKey, owner,
                    declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
                return false;
    }
    return true;
}

bool verifyAllMetadataTypeReferences(
    const LibraryMetadata& metadata,
    const std::unordered_set<std::string>& declaredTypeKeys,
    const std::unordered_set<std::string>& specializedTypeKeys,
    const std::unordered_set<std::string>& externalTypeKeys,
    std::string& error)
{
    if (metadata.formatMinor < 7) return true;
    if (!verifyMetadataTypeReferences(
            metadata, declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
        return false;
    if (!verifyMetadataCallableTypeReferences(
            metadata, declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error))
        return false;
    return verifyMetadataRemainingTypeReferences(
        metadata, declaredTypeKeys, specializedTypeKeys, externalTypeKeys, error);
}

bool verifyNamespaceAndExportCatalogues(
    const LibraryMetadata& metadata,
    std::string& error)
{
    std::unordered_set<std::string> namespaceFunctionKeys;
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        const std::string key = function.namespaceName +
            std::string(1, static_cast<char>(31)) + function.signatureKey;
        if (function.namespaceName.empty() || function.name.empty() ||
            function.signatureKey.empty() || function.declaredTypeKey.empty() ||
            function.mangledName.empty() ||
            !namespaceFunctionKeys.insert(key).second)
        {
            error = function.mangledName.empty()
                ? "metadata namespace function has no physical identity"
                : "metadata contains an incomplete or duplicate namespace function '" +
                    function.namespaceName + "." + function.signatureKey + "'";
            return false;
        }
    }
    std::unordered_set<std::string> namespaceStateKeys;
    for (const LibraryMetadataNamespaceState& state : metadata.namespaceStates)
    {
        const std::string key = state.namespaceName +
            std::string(1, static_cast<char>(31)) + state.name;
        if (state.accessibility != 1U || state.namespaceName.empty() ||
            state.name.empty() || state.declaredTypeKey.empty() ||
            state.storageName.empty() || state.accessorName.empty() ||
            !namespaceStateKeys.insert(key).second)
        {
            error = "metadata contains an incomplete or duplicate namespace state '" +
                state.namespaceName + "." + state.name + "'";
            return false;
        }
    }
    std::unordered_set<std::string> exportNames;
    for (const LibraryMetadataExport& exportRecord : metadata.exports)
    {
        if (exportRecord.cAbiName.empty() ||
            !exportNames.insert(exportRecord.cAbiName).second)
        {
            error = exportRecord.cAbiName.empty()
                ? "metadata contains a C ABI export with an empty symbol name"
                : "metadata contains duplicate C ABI export '" +
                    exportRecord.cAbiName + "'";
            return false;
        }
    }
    return true;
}

bool isLeafMetadataFileName(const std::string& value)
{
    if (value.empty()) return false;
    const std::filesystem::path path(value);
    return path == path.filename() && path.filename() != "." && path.filename() != "..";
}

bool isEmptyMetadataFingerprint(const std::array<std::uint8_t, 32>& fingerprint)
{
    for (const std::uint8_t byte : fingerprint)
        if (byte != 0U) return false;
    return true;
}

bool verifyBinaryClosureMetadata(
    const LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor < 25) return true;
    std::unordered_set<std::string> dependencyModules;
    for (const LibraryMetadataBinaryDependency& dependency : metadata.binaryDependencies)
    {
        const bool validRuntime = dependency.runtimeFileName.empty() ||
            isLeafMetadataFileName(dependency.runtimeFileName);
        if (dependency.moduleName.empty() || dependency.moduleName == metadata.moduleName ||
            !isLeafMetadataFileName(dependency.libraryFileName) ||
            !isLeafMetadataFileName(dependency.metadataFileName) ||
            !validRuntime || isEmptyMetadataFingerprint(dependency.abiFingerprint) ||
            !dependencyModules.insert(dependency.moduleName).second)
        {
            error = "metadata contains an incomplete, recursive, or duplicate binary "
                "dependency for module '" + dependency.moduleName + "'";
            return false;
        }
    }
    std::unordered_set<std::string> platformImportKeys;
    for (const LibraryMetadataPlatformImport& importRecord : metadata.platformImports)
    {
        const std::string key =
            foldedAscii(importRecord.dllName) + "\n" + importRecord.symbolName;
        if (importRecord.dllName.empty() || importRecord.symbolName.empty() ||
            !platformImportKeys.insert(key).second)
        {
            error = "metadata contains an incomplete or duplicate private platform import '" +
                importRecord.dllName + "!" + importRecord.symbolName + "'";
            return false;
        }
    }
    return true;
}

bool verifyInlineAndVolatileMemberMetadata(
    const LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor >= 13)
    {
        constexpr std::uint8_t kMethodKind = 2U;
        constexpr std::uint32_t kImportedC = 1U << 20U;
        for (const LibraryMetadataMember& member : metadata.members)
        {
            const std::uint32_t policy =
                (member.flags & kMetadataMemberInliningMask) >>
                kMetadataMemberInliningShift;
            if (!member.inlineSourceText.empty() &&
                (member.kind != kMethodKind || (member.flags & kImportedC) != 0U ||
                 (policy != static_cast<std::uint32_t>(frontend::InliningPolicy::Preferred) &&
                  policy != static_cast<std::uint32_t>(frontend::InliningPolicy::Required))))
            {
                error = "metadata member '" + member.declaringTypeKey + "." +
                    member.signatureKey +
                    "' carries an inline body without a compatible inline policy";
                return false;
            }
        }
    }
    if (metadata.formatMinor >= 14)
    {
        constexpr std::uint8_t kFieldKind = 0U;
        for (const LibraryMetadataMember& member : metadata.members)
        {
            if ((member.flags & kMetadataMemberVolatile) != 0U &&
                member.kind != kFieldKind)
            {
                error = "metadata member '" + member.declaringTypeKey + "." +
                    member.signatureKey + "' carries volatile on a non-field member";
                return false;
            }
        }
    }
    return true;
}

bool verifyReferenceReturnMemberMetadata(
    const LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor < 15) return true;
    constexpr std::uint8_t kMethodKind = 2U;
    constexpr std::uint32_t kPropertyAccessor = 1U << 12U;
    constexpr std::uint32_t kIndexerAccessor = 1U << 13U;
    constexpr std::uint32_t kGetter = 1U << 14U;
    constexpr std::uint32_t kSetter = 1U << 15U;
    for (const LibraryMetadataMember& member : metadata.members)
    {
        if ((member.flags & kMetadataMemberReferenceReturn) != 0U &&
            (member.kind != kMethodKind ||
             (member.flags & kPropertyAccessor) != 0U ||
             (member.flags & kSetter) != 0U ||
             ((member.flags & kIndexerAccessor) != 0U &&
              (member.flags & kGetter) == 0U)))
        {
            error = "metadata member '" + member.declaringTypeKey + "." +
                member.signatureKey +
                "' carries ref-return outside an ordinary method or indexer getter";
            return false;
        }
    }
    return true;
}

bool verifyNoHeapAndVariadicMemberMetadata(
    const LibraryMetadata& metadata,
    std::string& error)
{
    constexpr std::uint8_t kMethodKind = 2U;
    constexpr std::uint8_t kConstructorKind = 3U;
    if (metadata.formatMinor >= 16)
    {
        for (const LibraryMetadataMember& member : metadata.members)
        {
            if ((member.flags & kMetadataMemberNoHeap) != 0U &&
                member.kind != kMethodKind && member.kind != kConstructorKind)
            {
                error = "metadata member '" + member.declaringTypeKey + "." +
                    member.signatureKey +
                    "' carries noheap outside a method or constructor";
                return false;
            }
        }
    }
    if (metadata.formatMinor < 19) return true;
    constexpr std::uint32_t kExternC = 1U << 8U;
    constexpr std::uint32_t kImportedC = 1U << 20U;
    for (const LibraryMetadataMember& member : metadata.members)
    {
        if ((member.flags & kMetadataMemberVariadic) != 0U &&
            (member.kind != kMethodKind ||
             (member.flags & kExternC) == 0U ||
             (member.flags & kImportedC) == 0U ||
             member.parameters.empty()))
        {
            error = "metadata member '" + member.declaringTypeKey + "." +
                member.signatureKey +
                "' carries variadic outside an imported C method with fixed parameters";
            return false;
        }
    }
    return true;
}

// describeOwner returns the owner text of the error message; it runs only when the check fails.
template<typename Callable, typename DescribeOwner>
bool verifyParameterPackMetadata(
    const Callable& callable,
    const DescribeOwner& describeOwner,
    std::string& error)
{
    for (std::size_t index = 0; index < callable.parameters.size(); ++index)
    {
        const LibraryMetadataParameter& parameter = callable.parameters[index];
        if (!parameter.isParameterPack) continue;
        if (index + 1U != callable.parameters.size() ||
            parameter.passKind != LibraryMetadataParameterPassKind::Value ||
            parameter.defaultValueKind != LibraryMetadataDefaultValueKind::None ||
            parameter.typeKey.find("ReadOnlySpan__g1__") == std::string::npos)
        {
            error = "metadata " + describeOwner() + " parameter '" +
                parameter.name + "' carries an invalid params ReadOnlySpan<T> contract";
            return false;
        }
    }
    return true;
}

bool verifyParameterPackContracts(
    const LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor < 24) return true;
    for (const LibraryMetadataMember& member : metadata.members)
    {
        if (!verifyParameterPackMetadata(
                member,
                [&member]
                {
                    return "member '" + member.declaringTypeKey + "." + member.signatureKey + "'";
                },
                error))
        {
            return false;
        }
    }
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
    {
        if (!verifyParameterPackMetadata(
                function,
                [&function]
                {
                    return "namespace function '" + function.namespaceName + "." +
                        function.signatureKey + "'";
                },
                error))
        {
            return false;
        }
    }
    return true;
}

bool verifyVersionedMemberContracts(
    const LibraryMetadata& metadata,
    std::string& error)
{
    if (!verifyInlineAndVolatileMemberMetadata(metadata, error)) return false;
    if (!verifyReferenceReturnMemberMetadata(metadata, error)) return false;
    if (!verifyNoHeapAndVariadicMemberMetadata(metadata, error)) return false;
    return verifyParameterPackContracts(metadata, error);
}

// describeOwner returns the owner text of the error message; it runs only when the check fails.
template<typename Callable, typename DescribeOwner>
bool verifyLifetimeMetadata(
    const Callable& callable,
    const DescribeOwner& describeOwner,
    std::string& error)
{
    std::unordered_set<std::string> declared;
    for (const std::string& lifetime : callable.lifetimeParameters)
    {
        if (lifetime.empty() || !declared.insert(lifetime).second)
        {
            error = "metadata " + describeOwner() +
                " contains an empty or duplicate lifetime parameter";
            return false;
        }
    }
    for (const auto& [longer, shorter] : callable.lifetimeOutlivesConstraints)
    {
        if (!declared.contains(longer) ||
            (shorter != "static" && !declared.contains(shorter)))
        {
            error = "metadata " + describeOwner() +
                " contains an undeclared lifetime outlives edge";
            return false;
        }
    }
    return true;
}

bool verifyLifetimeMetadataContracts(
    const LibraryMetadata& metadata,
    std::string& error)
{
    if (metadata.formatMinor < 34) return true;
    for (const LibraryMetadataType& type : metadata.types)
    {
        if (!verifyLifetimeMetadata(
                type, [&type] { return "type '" + type.identityKey + "'"; }, error))
            return false;
        if (type.kind == LibraryMetadataTypeKind::Delegate &&
            (type.delegateParameterPassKinds.size() !=
                 type.delegateParameterTypeKeys.size() ||
             type.delegateParameterLifetimeNames.size() !=
                 type.delegateParameterTypeKeys.size()))
        {
            error = "metadata delegate '" + type.identityKey +
                "' has inconsistent lifetime parameter vectors";
            return false;
        }
    }
    for (const LibraryMetadataMember& member : metadata.members)
        if (!verifyLifetimeMetadata(
                member,
                [&member]
                {
                    return "member '" + member.declaringTypeKey + "." + member.signatureKey + "'";
                },
                error))
            return false;
    for (const LibraryMetadataNamespaceFunction& function : metadata.namespaceFunctions)
        if (!verifyLifetimeMetadata(
                function,
                [&function]
                {
                    return "namespace function '" + function.namespaceName + "." +
                        function.signatureKey + "'";
                },
                error))
            return false;
    return true;
}

}
