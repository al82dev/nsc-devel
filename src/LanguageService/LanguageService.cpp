#include "nativesharp/LanguageService/LanguageService.h"

#include "nativesharp/Basic/Version.h"
#include "nativesharp/Frontend/ProjectCompilation.h"
#include "nativesharp/Target/TargetInfo.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

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
    std::shared_ptr<frontend::ProjectCompilation> semanticCompilation;
};

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
    std::unordered_map<std::string, std::shared_ptr<frontend::ProjectCompilation>> lastGoodSemantic;
    std::atomic_uint64_t nextGeneration = 1;
};

AnalysisSnapshot::AnalysisSnapshot(std::shared_ptr<Implementation> implementation) noexcept
    : implementation_(std::move(implementation))
{
}

bool AnalysisSnapshot::available() const noexcept
{
    return implementation_ != nullptr;
}

std::uint64_t AnalysisSnapshot::generation() const noexcept
{
    return implementation_ != nullptr ? implementation_->generation : 0;
}

const std::filesystem::path& AnalysisSnapshot::projectPath() const noexcept
{
    static const std::filesystem::path empty;
    return implementation_ != nullptr ? implementation_->projectPath : empty;
}

bool AnalysisSnapshot::currentAnalysisSucceeded() const noexcept
{
    return implementation_ != nullptr && implementation_->currentAnalysisSucceeded;
}

bool AnalysisSnapshot::semanticModelAvailable() const noexcept
{
    return implementation_ != nullptr && implementation_->semanticCompilation != nullptr;
}

bool AnalysisSnapshot::usedPreviousSemanticModel() const noexcept
{
    return implementation_ != nullptr && implementation_->usedPreviousSemanticModel;
}

bool AnalysisSnapshot::cancelled() const noexcept
{
    return implementation_ != nullptr && implementation_->cancelled;
}

std::string_view AnalysisSnapshot::error() const noexcept
{
    return implementation_ != nullptr ? std::string_view(implementation_->error) : std::string_view{};
}

std::span<const LanguageDiagnostic> AnalysisSnapshot::diagnostics() const noexcept
{
    return implementation_ != nullptr
        ? std::span<const LanguageDiagnostic>(implementation_->diagnostics)
        : std::span<const LanguageDiagnostic>{};
}

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

AnalysisSnapshot LanguageService::analyzeProject(
    const std::filesystem::path& project,
    std::size_t jobs,
    std::function<bool()> cancellationCheck)
{
    auto result = std::make_shared<AnalysisSnapshot::Implementation>();
    result->generation = implementation_->nextGeneration.fetch_add(1, std::memory_order_relaxed);
    const auto keyedProject = Implementation::documentKey(project);
    if (!keyedProject.has_value())
    {
        result->error = "project path is invalid";
        return AnalysisSnapshot(std::move(result));
    }
    result->projectPath = keyedProject->second;

    std::unordered_map<std::string, std::string> overlays;
    {
        std::scoped_lock lock(implementation_->mutex);
        overlays.reserve(implementation_->documents.size());
        for (const auto& [key, document] : implementation_->documents)
        {
            overlays.emplace(key, document.text);
        }
    }

    auto compilation = std::make_shared<frontend::ProjectCompilation>();
    compilation->setCompilationJobs(std::max<std::size_t>(jobs, 1));
    compilation->setCancellationCheck(cancellationCheck);
    compilation->setSourceBufferProvider(
        [overlays = std::move(overlays)](const std::filesystem::path& path)
            -> std::optional<std::string> {
            const auto keyedPath = Implementation::documentKey(path);
            if (!keyedPath.has_value()) return std::nullopt;
            const auto found = overlays.find(keyedPath->first);
            return found != overlays.end() ? std::optional<std::string>(found->second) : std::nullopt;
        });

    result->currentCompilation = compilation;
    result->currentAnalysisSucceeded =
        compilation->compileProjectMethodBodies(result->projectPath, result->error);
    result->cancelled = cancellationCheck && cancellationCheck();
    for (const diagnostics::Diagnostic& diagnostic : compilation->diagnostics().diagnostics())
    {
        if (auto converted = makeLanguageDiagnostic(diagnostic, compilation->sourceManager()))
        {
            result->diagnostics.push_back(std::move(*converted));
        }
    }

    std::scoped_lock lock(implementation_->mutex);
    if (result->currentAnalysisSucceeded && !compilation->diagnostics().hasErrors())
    {
        result->semanticCompilation = compilation;
        implementation_->lastGoodSemantic[keyedProject->first] = compilation;
    }
    else
    {
        const auto previous = implementation_->lastGoodSemantic.find(keyedProject->first);
        if (previous != implementation_->lastGoodSemantic.end())
        {
            result->semanticCompilation = previous->second;
            result->usedPreviousSemanticModel = true;
        }
    }
    return AnalysisSnapshot(std::move(result));
}
}
