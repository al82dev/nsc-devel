#pragma once

#include "nativesharp/Frontend/ComptimeAttributeDirective.h"
#include "nativesharp/Frontend/ComptimeAuthoredDiagnostic.h"
#include "nativesharp/Frontend/ComptimeDependency.h"
#include "nativesharp/Frontend/MacroExpansionEngine.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace nativesharp::frontend
{
struct ProjectComptimeRecord final
{
    std::size_t projectOrdinal = 0;
    std::size_t sourceOrdinal = 0;
    std::string moduleName;
    std::string relativeSourcePath;
    std::size_t evaluationCount = 0;
    std::size_t executedSteps = 0;
    std::size_t cacheHits = 0;
    std::size_t cacheMisses = 0;
    std::string reproducibilityDigest;
    std::vector<ComptimeDependency> dependencies;
    std::vector<ComptimeAttributeDirective> attributes;
    std::vector<MacroExpansionRecord> macroExpansions;
    std::vector<ComptimeAuthoredDiagnostic> authoredDiagnostics;
    std::vector<std::pair<std::string, std::string>> declaredCapabilities;
};
}
