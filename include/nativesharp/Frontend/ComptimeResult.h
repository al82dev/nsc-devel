#pragma once

#include "nativesharp/Frontend/ComptimeAttributeDirective.h"
#include "nativesharp/Frontend/ComptimeAuthoredDiagnostic.h"
#include "nativesharp/Frontend/ComptimeDependency.h"
#include "nativesharp/Frontend/ComptimeEvaluation.h"
#include "nativesharp/Frontend/MacroExpansionEngine.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
struct ComptimeResult final
{
    bool success = false;
    std::string source;
    std::string error;
    std::vector<ComptimeEvaluation> evaluations;
    std::vector<ComptimeDependency> dependencies;
    std::vector<ComptimeAttributeDirective> attributes;
    std::vector<MacroExpansionRecord> macroExpansions;
    std::vector<ComptimeAuthoredDiagnostic> authoredDiagnostics;
    std::vector<std::pair<std::string, std::string>> declaredCapabilities;
    std::size_t executedSteps = 0;
    std::size_t cacheHits = 0;
    std::size_t cacheMisses = 0;
    std::size_t memoryUsed = 0;
    std::string reproducibilityDigest;
};
}
