#pragma once

#include "nativesharp/Frontend/ObjectFieldLayout.h"
#include "nativesharp/Frontend/StructValueLayout.h"

#include "nativesharp/Frontend/ProjectConstructionModel.h"
#include "nativesharp/Frontend/ProjectInheritanceModel.h"
#include "nativesharp/Frontend/ProjectMemberModel.h"
#include "nativesharp/Frontend/ProjectNameLookup.h"
#include "nativesharp/Language/AggregateSemantics.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace nativesharp::diagnostics
{
class DiagnosticEngine;
}

namespace nativesharp::frontend
{
struct ProjectOopHirModelTestAccess;

struct ClassObjectLayout final
{
    std::size_t typeOrdinal = 0;
    std::string typeKey;
    std::uint64_t size = 0;
    std::uint64_t alignment = 1;
    std::uint64_t headerSize = 0;
    std::optional<std::size_t> baseTypeOrdinal;
    std::uint64_t baseSubobjectSize = 0;
    std::uint64_t referenceSize = 0;
    std::uint64_t referenceAlignment = 1;
    std::vector<ObjectFieldLayout> fields;
};
}
