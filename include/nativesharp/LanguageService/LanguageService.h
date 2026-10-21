#pragma once

#include "nativesharp/LanguageService/AnalysisSnapshot.h"
#include "nativesharp/LanguageService/DocumentSnapshot.h"
#include "nativesharp/LanguageService/LanguageServiceIdentity.h"
#include "nativesharp/LanguageService/ProtocolTypes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::language_service
{
inline constexpr std::uint32_t kAnalysisAbiVersion = 3;

// Stable facade for editor analysis. Internal lexer, parser and semantic model classes remain
// implementation details so the future compiler rewrite does not leak into nsc-lsp.
class LanguageService final
{
public:
    LanguageService();
    LanguageService(const LanguageService&) = delete;
    LanguageService& operator=(const LanguageService&) = delete;
    LanguageService(LanguageService&&) noexcept;
    LanguageService& operator=(LanguageService&&) noexcept;
    ~LanguageService();

    LanguageServiceIdentity identity() const noexcept;
    bool openDocument(
        const std::filesystem::path& path,
        std::string text,
        std::int64_t version,
        std::string& error);
    bool updateDocument(
        const std::filesystem::path& path,
        std::string text,
        std::int64_t version,
        std::string& error);
    bool closeDocument(const std::filesystem::path& path) noexcept;
    std::optional<DocumentSnapshot> document(const std::filesystem::path& path) const;
    std::optional<LanguageTextEdit> formatDocument(
        const std::filesystem::path& path,
        DocumentFormattingOptions options = {}) const;
    std::vector<LanguageCodeAction> codeActions(
        const std::filesystem::path& path,
        TextRange range,
        std::span<const std::string> diagnosticCodes,
        DocumentFormattingOptions options = {}) const;
    std::size_t documentCount() const noexcept;
    AnalysisSnapshot analyzeProject(
        const std::filesystem::path& project,
        std::size_t jobs = 1,
        std::function<bool()> cancellationCheck = {});
    MemberCompletionResult memberCompletions(
        const MemberCompletionRequest& request,
        std::size_t jobs = 1);

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};
}
