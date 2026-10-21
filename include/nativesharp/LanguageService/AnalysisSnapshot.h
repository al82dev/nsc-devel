#pragma once

#include "nativesharp/LanguageService/ProtocolTypes.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace nativesharp::language_service
{
class AnalysisSnapshot final
{
public:
    AnalysisSnapshot() noexcept = default;

    bool available() const noexcept;
    std::uint64_t generation() const noexcept;
    const std::filesystem::path& projectPath() const noexcept;
    std::optional<std::int64_t> analyzedDocumentVersion(
        const std::filesystem::path& path) const;
    bool currentAnalysisSucceeded() const noexcept;
    bool semanticModelAvailable() const noexcept;
    bool usedPreviousSemanticModel() const noexcept;
    bool cancelled() const noexcept;
    std::string_view error() const noexcept;
    std::span<const LanguageDiagnostic> diagnostics() const noexcept;
    std::vector<LanguageSymbol> documentSymbols(const std::filesystem::path& path) const;
    std::vector<LanguageSymbol> workspaceSymbols(
        std::string_view query,
        std::size_t limit = 100) const;
    std::optional<LanguageSymbol> symbolAt(
        const std::filesystem::path& path,
        TextPosition position) const;
    std::vector<LanguageLocation> definitions(
        const std::filesystem::path& path,
        TextPosition position) const;
    std::vector<CompletionItem> completions(
        const std::filesystem::path& path,
        std::string_view prefix,
        std::size_t limit = 100) const;
    MemberCompletionResult memberCompletions(
        const MemberCompletionRequest& request) const;
    std::optional<SignatureHelp> signatureHelp(
        const std::filesystem::path& path,
        TextPosition position) const;
    std::vector<LanguageLocation> references(
        const std::filesystem::path& path,
        TextPosition position,
        bool includeDeclaration = true) const;
    std::optional<TextRange> prepareRename(
        const std::filesystem::path& path,
        TextPosition position) const;
    RenameResult rename(
        const std::filesystem::path& path,
        TextPosition position,
        std::string_view newName) const;
    std::vector<LanguageSemanticToken> semanticTokens(
        const std::filesystem::path& path) const;

private:
    class Implementation;
    explicit AnalysisSnapshot(std::shared_ptr<Implementation> implementation) noexcept;

    std::shared_ptr<Implementation> implementation_;
    friend class LanguageService;
};
}
