#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "nativesharp/Language/DebugAvailability.h"
#include "nativesharp/Language/DebugEntityKind.h"
#include "nativesharp/Language/DebugOptimization.h"

namespace nativesharp::language
{
struct SemanticDebugEntry final
{
    std::string stableName;
    std::string typeName;
    DebugEntityKind kind = DebugEntityKind::Local;
    DebugOptimization optimization = DebugOptimization::O0;
    DebugAvailability availability = DebugAvailability::Live;
    std::size_t scopeBeginLine = 0;
    std::size_t scopeEndLine = 0;
    std::vector<std::size_t> sequencePointLines;
    bool sourceGenerated = false;
    bool hasInlineSite = false;
    bool valueInvented = false;
};
}
