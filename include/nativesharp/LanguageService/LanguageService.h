#pragma once

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
inline constexpr std::uint32_t kAnalysisAbiVersion = 2;

struct LanguageServiceIdentity final
{
    std::string_view compilerRelease;
    std::string_view compilerStage;
    std::string_view target;
    std::string_view buildId;
    std::uint32_t analysisAbi = 0;
};

struct DocumentSnapshot final
{
    std::filesystem::path path;
    std::string text;
    std::int64_t version = 0;
};

class AnalysisSnapshot final
{
public:
    AnalysisSnapshot() noexcept = default;

    bool available() const noexcept;
    std::uint64_t generation() const noexcept;
    const std::filesystem::path& projectPath() const noexcept;
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

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};
}
