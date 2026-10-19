#pragma once

#include "nativesharp/Frontend/VirtualMethodInfo.h"

#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ClassVirtualTable final
{
    std::size_t typeOrdinal = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::vector<std::size_t> slotMemberOrdinals;
    std::vector<bool> slotAbstract;
    bool isAbstract = false;
    bool isSealed = false;
};
}
