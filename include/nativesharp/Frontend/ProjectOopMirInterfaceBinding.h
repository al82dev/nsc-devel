#pragma once

#include "nativesharp/Language/ErrorSemantics.h"

#include "nativesharp/Frontend/InliningPolicy.h"

#include "nativesharp/Frontend/ProjectOopHirModel.h"
#include "nativesharp/Frontend/ProjectDestructionModel.h"
#include "nativesharp/Frontend/ProjectInterfaceModel.h"
#include "nativesharp/Frontend/ProjectVirtualDispatchModel.h"
#include "nativesharp/Frontend/TemplateParseCache.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace nativesharp::frontend
{
// Item 398: the complete class/interface binding consumed by runtime metadata and interface
// cleanup dispatch.  This is deliberately independent of ProjectInterfaceModel so every backend
// decision is available at the MIR boundary.
struct ProjectOopMirInterfaceBinding final
{
    std::size_t implementingTypeOrdinal = 0;
    std::size_t interfaceTypeOrdinal = 0;
    std::string interfaceTypeKey;
    std::uint64_t stableInterfaceTypeId = 0;
    std::string methodTableName;
    std::vector<std::size_t> interfaceMemberOrdinals;
    std::vector<std::size_t> implementationMemberOrdinals;
    std::vector<std::string> calleeNames;
};
}
