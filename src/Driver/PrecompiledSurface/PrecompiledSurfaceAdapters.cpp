#include "../Internal/DriverInternal.h"

namespace nativesharp::driver
{
using target::BuiltinLayoutKind;

namespace public_surface_support
{
std::string escapedStringLiteral(std::string_view value)
{
    return ::nativesharp::driver::escapedStringLiteral(value);
}

bool sourceTypeForMetadataKey(std::string typeKey, std::string &sourceType, std::string &error)
{
    return ::nativesharp::driver::sourceTypeForMetadataKey(std::move(typeKey), sourceType, error);
}

bool sourceTypeForMetadataKey(const interop::LibraryMetadata &metadata, const std::string &typeKey,
                              std::string &sourceType, std::string &error)
{
    return ::nativesharp::driver::sourceTypeForMetadataKey(metadata, typeKey, sourceType, error);
}

std::string ownershipPrefix(interop::LibraryMetadataOwnershipKind ownership)
{
    return ::nativesharp::driver::ownershipPrefix(ownership);
}

std::string ownershipPrefix(interop::LibraryMetadataOwnershipKind ownership, std::string_view lifetime)
{
    return ::nativesharp::driver::ownershipPrefix(ownership, lifetime);
}

std::string lifetimeAnnotation(std::string_view lifetime)
{
    return ::nativesharp::driver::lifetimeAnnotation(lifetime);
}

std::string metadataBaseTypeKey(std::string typeKey)
{
    return ::nativesharp::driver::metadataBaseTypeKey(std::move(typeKey));
}

void appendParserKnownTypeNames(const interop::LibraryMetadata &metadata, const std::string &writtenTypeKey,
                                std::vector<std::string> &knownTypeNames)
{
    ::nativesharp::driver::appendParserKnownTypeNames(metadata, writtenTypeKey, knownTypeNames);
}

const interop::LibraryMetadataType *metadataTypeForKey(const interop::LibraryMetadata &metadata,
                                                       const std::string &typeKey)
{
    return ::nativesharp::driver::metadataTypeForKey(metadata, typeKey);
}

bool isPubliclyReachableMetadataType(const interop::LibraryMetadata &metadata,
                                     const interop::LibraryMetadataType &type) noexcept
{
    return ::nativesharp::driver::isPubliclyReachableMetadataType(metadata, type);
}

bool canMaterializePrecompiledDeclaration(const interop::LibraryMetadata &metadata,
                                          const interop::LibraryMetadataMember &member)
{
    return ::nativesharp::driver::canMaterializePrecompiledDeclaration(metadata, member);
}

void collectMaterializedContractTypes(const interop::LibraryMetadata &metadata, const std::string &typeKey,
                                      std::set<std::string> &identities)
{
    std::vector<std::string> pending = {typeKey};
    while (!pending.empty())
    {
        std::string currentKey = std::move(pending.back());
        pending.pop_back();
        const interop::LibraryMetadataType *type = ::nativesharp::driver::metadataTypeForKey(metadata, currentKey);
        if (type == nullptr || !identities.insert(type->identityKey).second)
            continue;
        if (type->kind == interop::LibraryMetadataTypeKind::Struct)
        {
            for (auto field = type->fields.rbegin(); field != type->fields.rend(); ++field)
                pending.push_back(field->typeKey);
        }
        else if (type->kind == interop::LibraryMetadataTypeKind::Class ||
                 type->kind == interop::LibraryMetadataTypeKind::Interface)
        {
            for (auto interfaceType = type->interfaceTypeKeys.rbegin(); interfaceType != type->interfaceTypeKeys.rend();
                 ++interfaceType)
                pending.push_back(*interfaceType);
            if (!type->baseTypeKey.empty())
                pending.push_back(type->baseTypeKey);
        }
        else if (type->kind == interop::LibraryMetadataTypeKind::Delegate)
        {
            for (auto parameter = type->delegateParameterTypeKeys.rbegin();
                 parameter != type->delegateParameterTypeKeys.rend(); ++parameter)
                pending.push_back(*parameter);
            pending.push_back(type->delegateErrorTypeKey);
            pending.push_back(type->delegateReturnTypeKey);
        }
    }
}

std::string_view sourceAccessibility(std::uint8_t accessibility) noexcept
{
    return ::nativesharp::driver::sourceAccessibility(accessibility);
}

const interop::LibraryMetadataMember *metadataFieldMember(const interop::LibraryMetadata &metadata,
                                                          const interop::LibraryMetadataType &type,
                                                          std::string_view name)
{
    return ::nativesharp::driver::metadataFieldMember(metadata, type, name);
}

const interop::LibraryMetadataMember *metadataEventForBacking(const interop::LibraryMetadata &metadata,
                                                              const interop::LibraryMetadataType &type,
                                                              const interop::LibraryMetadataField &field)
{
    return ::nativesharp::driver::metadataEventForBacking(metadata, type, field);
}

std::string enumConstantSource(const interop::LibraryMetadataType &type, std::uint64_t bits)
{
    return ::nativesharp::driver::enumConstantSource(type, bits);
}

bool metadataConstantSource(const interop::LibraryMetadataMember &member, std::string &source, std::string &error)
{
    return ::nativesharp::driver::metadataConstantSource(member, source, error);
}

std::string_view metadataOperatorToken(std::string_view name) noexcept
{
    return ::nativesharp::driver::metadataOperatorToken(name);
}

std::string parameterPassPrefix(interop::LibraryMetadataParameterPassKind passKind, std::string_view lifetime)
{
    return ::nativesharp::driver::parameterPassPrefix(passKind, lifetime);
}

bool sourceDefaultValue(const interop::LibraryMetadataParameter &parameter, std::string &source, std::string &error)
{
    return ::nativesharp::driver::sourceDefaultValue(parameter, source, error);
}
} // namespace public_surface_support
} // namespace nativesharp::driver
