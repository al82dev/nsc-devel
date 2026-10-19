#pragma once

#include "nativesharp/Frontend/ObjectFieldLayout.h"
#include "nativesharp/Frontend/StructValueLayout.h"
#include "nativesharp/Frontend/ClassObjectLayout.h"
#include "nativesharp/Frontend/ObjectInitializationStepKind.h"
#include "nativesharp/Frontend/ObjectInitializationStep.h"

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

struct ObjectAllocationPlan final
{
    std::size_t constructionOrdinal = 0;
    std::size_t targetTypeOrdinal = 0;
    std::string targetTypeKey;
    std::uint64_t objectSize = 0;
    std::uint64_t objectAlignment = 1;
    std::uint64_t referenceSize = 0;
    std::uint64_t referenceAlignment = 1;
    bool allocationFailureReturnsNull = true;
    bool zeroInitializesStorage = true;
    bool selectedImplicitDefault = false;
    std::optional<std::size_t> selectedConstructorMemberOrdinal;
    std::optional<std::size_t> constructorEntryMemberOrdinal;
    std::vector<ObjectInitializationStep> initializationSteps;
};
}
