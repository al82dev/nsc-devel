#pragma once

#include "nativesharp/Frontend/ObjectFieldLayout.h"
#include "nativesharp/Frontend/StructValueLayout.h"
#include "nativesharp/Frontend/ClassObjectLayout.h"
#include "nativesharp/Frontend/ObjectInitializationStepKind.h"

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

struct ObjectInitializationStep final
{
    ObjectInitializationStepKind kind = ObjectInitializationStepKind::RequestStorage;
    std::optional<std::size_t> constructorMemberOrdinal;
};
}
