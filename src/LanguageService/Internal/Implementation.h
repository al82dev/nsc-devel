#pragma once

#include "SemanticIndex.h"
#include "StoredDocument.h"
#include "nativesharp/LanguageService/LanguageService.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace nativesharp::language_service
{
class LanguageService::Implementation final
{
public:
    using StoredDocument = language_service::StoredDocument;

    static std::optional<std::pair<std::string, std::filesystem::path>> documentKey(
        const std::filesystem::path& path);

    mutable std::mutex mutex;
    std::unordered_map<std::string, StoredDocument> documents;
    std::unordered_map<std::string, std::shared_ptr<SemanticIndex>> lastGoodSemantic;
    std::unordered_map<std::string, AnalysisSnapshot> lastProjectSnapshots;
    std::atomic_uint64_t nextGeneration = 1;
};
}
