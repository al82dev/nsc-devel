#pragma once

#include "SemanticIndex.h"
#include "nativesharp/LanguageService/AnalysisSnapshot.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace nativesharp::language_service
{
class AnalysisSnapshot::Implementation final
{
public:
    std::uint64_t generation = 0;
    std::filesystem::path projectPath;
    bool currentAnalysisSucceeded = false;
    bool usedPreviousSemanticModel = false;
    bool cancelled = false;
    std::string error;
    std::vector<LanguageDiagnostic> diagnostics;
    std::shared_ptr<frontend::ProjectCompilation> currentCompilation;
    std::shared_ptr<SemanticIndex> semanticIndex;
};
}
