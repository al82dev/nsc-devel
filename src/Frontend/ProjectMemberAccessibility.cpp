#include "nativesharp/Frontend/ProjectMemberAccessibility.h"

namespace nativesharp::frontend
{
namespace
{
bool sameProject(
    std::size_t leftTypeOrdinal,
    std::size_t rightTypeOrdinal,
    const ProjectNameLookup& nameLookup) noexcept
{
    if (leftTypeOrdinal >= nameLookup.namedTypes().size() ||
        rightTypeOrdinal >= nameLookup.namedTypes().size())
    {
        return false;
    }
    return nameLookup.namedTypes()[leftTypeOrdinal].projectOrdinal ==
        nameLookup.namedTypes()[rightTypeOrdinal].projectOrdinal;
}
}

bool ProjectMemberAccessibility::isAccessible(
    const ClassMemberSymbol& member,
    std::size_t accessingTypeOrdinal,
    std::optional<std::size_t> receiverTypeOrdinal,
    const ProjectNameLookup& nameLookup,
    const ProjectInheritanceModel& inheritanceModel) noexcept
{
    switch (member.accessibility)
    {
        case ClassMemberAccessibility::Public:
            return true;

        case ClassMemberAccessibility::Private:
            return accessingTypeOrdinal == member.declaringTypeOrdinal;

        case ClassMemberAccessibility::Internal:
            return sameProject(accessingTypeOrdinal, member.declaringTypeOrdinal, nameLookup);

        case ClassMemberAccessibility::Protected:
        {
            if (accessingTypeOrdinal == member.declaringTypeOrdinal)
            {
                return true;
            }
            if (!inheritanceModel.isDerivedFrom(accessingTypeOrdinal, member.declaringTypeOrdinal))
            {
                return false;
            }

            // C#-style protected instance rule: from a derived class, a protected member
            // may be selected through this/derived receivers, not through an arbitrary
            // expression whose compile-time type is merely the base class.
            if (!receiverTypeOrdinal.has_value())
            {
                return true;
            }
            return *receiverTypeOrdinal == accessingTypeOrdinal ||
                inheritanceModel.isDerivedFrom(*receiverTypeOrdinal, accessingTypeOrdinal);
        }
    }
    return false;
}

bool ProjectMemberAccessibility::isConstructorAccessible(
    const ClassMemberSymbol& constructor,
    std::size_t accessingTypeOrdinal,
    const ProjectNameLookup& nameLookup,
    const ProjectInheritanceModel& inheritanceModel) noexcept
{
    return isAccessible(
        constructor,
        accessingTypeOrdinal,
        std::nullopt,
        nameLookup,
        inheritanceModel);
}
}
