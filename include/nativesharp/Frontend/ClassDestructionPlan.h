#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nativesharp::frontend
{
struct ClassDestructionPlan final
{
    std::size_t typeOrdinal = 0;
    std::optional<std::size_t> explicitDestructorMemberOrdinal;
    std::vector<std::size_t> destructorMemberOrdinals;
    std::string destructionEntryName;
    std::string objectDestructionEntryName;
    std::string crossModuleCreateEntryName;
    std::string crossModuleDestroyEntryName;
    std::string crossModuleDeallocateEntryName;
    std::uint16_t crossModuleLifetimeAbiMajor = 0;
};
}
