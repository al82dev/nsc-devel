#pragma once

#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <optional>

namespace nativesharp::frontend
{
class ProjectMemberAccessibility final
{
public:
    static bool isAccessible(
        const ClassMemberSymbol& member,
        std::size_t accessingTypeOrdinal,
        std::optional<std::size_t> receiverTypeOrdinal,
        const ProjectNameLookup& nameLookup,
        const ProjectInheritanceModel& inheritanceModel) noexcept;

    static bool isConstructorAccessible(
        const ClassMemberSymbol& constructor,
        std::size_t accessingTypeOrdinal,
        const ProjectNameLookup& nameLookup,
        const ProjectInheritanceModel& inheritanceModel) noexcept;
};
}
