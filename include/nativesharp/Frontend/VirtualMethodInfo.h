#pragma once

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
struct VirtualMethodInfo final
{
    std::size_t memberOrdinal = 0;
    std::size_t slotOrdinal = 0;
    std::size_t rootVirtualMemberOrdinal = 0;
    std::optional<std::size_t> overriddenMemberOrdinal;
};
}
