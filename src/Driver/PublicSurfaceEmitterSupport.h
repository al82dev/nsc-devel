#pragma once

#include "nativesharp/Interop/LibraryMetadata.h"

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::driver::public_surface_support
{
std::string escapedStringLiteral(std::string_view value);
bool sourceTypeForMetadataKey(std::string typeKey, std::string& sourceType, std::string& error);
bool sourceTypeForMetadataKey(
    const interop::LibraryMetadata& metadata,
    const std::string& typeKey,
    std::string& sourceType,
    std::string& error);
std::string ownershipPrefix(interop::LibraryMetadataOwnershipKind ownership);
std::string ownershipPrefix(
    interop::LibraryMetadataOwnershipKind ownership,
    std::string_view lifetime);
std::string lifetimeAnnotation(std::string_view lifetime);
std::string metadataBaseTypeKey(std::string typeKey);
void appendParserKnownTypeNames(
    const interop::LibraryMetadata& metadata,
    const std::string& writtenTypeKey,
    std::vector<std::string>& knownTypeNames);
const interop::LibraryMetadataType* metadataTypeForKey(
    const interop::LibraryMetadata& metadata,
    const std::string& typeKey);
bool isPubliclyReachableMetadataType(
    const interop::LibraryMetadata& metadata,
    const interop::LibraryMetadataType& type) noexcept;
bool canMaterializePrecompiledDeclaration(
    const interop::LibraryMetadata& metadata,
    const interop::LibraryMetadataMember& member);
void collectMaterializedContractTypes(
    const interop::LibraryMetadata& metadata,
    const std::string& typeKey,
    std::set<std::string>& identities);
std::string_view sourceAccessibility(std::uint8_t accessibility) noexcept;
const interop::LibraryMetadataMember* metadataFieldMember(
    const interop::LibraryMetadata& metadata,
    const interop::LibraryMetadataType& type,
    std::string_view name);
const interop::LibraryMetadataMember* metadataEventForBacking(
    const interop::LibraryMetadata& metadata,
    const interop::LibraryMetadataType& type,
    const interop::LibraryMetadataField& field);
std::string enumConstantSource(
    const interop::LibraryMetadataType& type,
    std::uint64_t bits);
bool metadataConstantSource(
    const interop::LibraryMetadataMember& member,
    std::string& source,
    std::string& error);
std::string_view metadataOperatorToken(std::string_view name) noexcept;
std::string parameterPassPrefix(
    interop::LibraryMetadataParameterPassKind passKind,
    std::string_view lifetime = {});
bool sourceDefaultValue(
    const interop::LibraryMetadataParameter& parameter,
    std::string& source,
    std::string& error);
} // namespace nativesharp::driver::public_surface_support
