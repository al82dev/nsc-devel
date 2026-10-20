#pragma once

#include "nativesharp/Frontend/ProjectOopMirDispatchTable.h"

namespace nativesharp::frontend
{
struct ProjectOopMirDispatchTable::InterfaceTable final
{
    std::size_t interfaceTypeOrdinal = 0;
    std::string interfaceTypeKey;
    std::uint64_t stableInterfaceTypeId = 0;
    std::string methodTableName;
    std::vector<std::size_t> interfaceMemberOrdinals;
    std::vector<std::size_t> implementationMemberOrdinals;
    std::vector<std::string> calleeNames;
};
}
