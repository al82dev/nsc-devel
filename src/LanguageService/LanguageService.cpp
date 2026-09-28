#include "nativesharp/LanguageService/LanguageService.h"

#include "nativesharp/Basic/Version.h"
#include "nativesharp/Target/TargetInfo.h"

#include <algorithm>
#include <cctype>
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
    struct StoredDocument final
    {
        std::filesystem::path path;
        std::string text;
        std::int64_t version = 0;
    };

    static std::optional<std::pair<std::string, std::filesystem::path>> documentKey(
        const std::filesystem::path& path)
    {
        if (path.empty()) return std::nullopt;
        std::error_code error;
        std::filesystem::path absolute = std::filesystem::absolute(path, error);
        if (error) return std::nullopt;
        absolute = absolute.lexically_normal();
        std::string key = absolute.generic_string();
        std::ranges::transform(key, key.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        return std::pair{std::move(key), std::move(absolute)};
    }

    mutable std::mutex mutex;
    std::unordered_map<std::string, StoredDocument> documents;
};

LanguageService::LanguageService()
    : implementation_(std::make_unique<Implementation>())
{
}

LanguageService::LanguageService(LanguageService&&) noexcept = default;
LanguageService& LanguageService::operator=(LanguageService&&) noexcept = default;
LanguageService::~LanguageService() = default;

LanguageServiceIdentity LanguageService::identity() const noexcept
{
    return LanguageServiceIdentity{
        kCompilerReleaseVersion,
        kLanguageVersion,
        kCompilerStage,
        target::TargetInfo::WindowsX64().llvmTriple(),
        kCompilerBuildId,
        kAnalysisAbiVersion};
}

bool LanguageService::openDocument(
    const std::filesystem::path& path,
    std::string text,
    std::int64_t version,
    std::string& error)
{
    error.clear();
    const auto keyedPath = Implementation::documentKey(path);
    if (!keyedPath.has_value() || version < 0)
    {
        error = version < 0 ? "document version cannot be negative" : "document path is invalid";
        return false;
    }
    std::scoped_lock lock(implementation_->mutex);
    if (implementation_->documents.contains(keyedPath->first))
    {
        error = "document is already open";
        return false;
    }
    implementation_->documents.emplace(
        keyedPath->first,
        Implementation::StoredDocument{keyedPath->second, std::move(text), version});
    return true;
}

bool LanguageService::updateDocument(
    const std::filesystem::path& path,
    std::string text,
    std::int64_t version,
    std::string& error)
{
    error.clear();
    const auto keyedPath = Implementation::documentKey(path);
    if (!keyedPath.has_value() || version < 0)
    {
        error = version < 0 ? "document version cannot be negative" : "document path is invalid";
        return false;
    }
    std::scoped_lock lock(implementation_->mutex);
    const auto found = implementation_->documents.find(keyedPath->first);
    if (found == implementation_->documents.end())
    {
        error = "document is not open";
        return false;
    }
    if (version <= found->second.version)
    {
        error = "document version is not newer than the current version";
        return false;
    }
    found->second.text = std::move(text);
    found->second.version = version;
    return true;
}

bool LanguageService::closeDocument(const std::filesystem::path& path) noexcept
{
    const auto keyedPath = Implementation::documentKey(path);
    if (!keyedPath.has_value()) return false;
    std::scoped_lock lock(implementation_->mutex);
    return implementation_->documents.erase(keyedPath->first) != 0;
}

std::optional<DocumentSnapshot> LanguageService::document(
    const std::filesystem::path& path) const
{
    const auto keyedPath = Implementation::documentKey(path);
    if (!keyedPath.has_value()) return std::nullopt;
    std::scoped_lock lock(implementation_->mutex);
    const auto found = implementation_->documents.find(keyedPath->first);
    if (found == implementation_->documents.end()) return std::nullopt;
    return DocumentSnapshot{found->second.path, found->second.text, found->second.version};
}

std::size_t LanguageService::documentCount() const noexcept
{
    std::scoped_lock lock(implementation_->mutex);
    return implementation_->documents.size();
}
}
