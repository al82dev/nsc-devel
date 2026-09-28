#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace nativesharp::language_service
{
inline constexpr std::uint32_t kAnalysisAbiVersion = 1;

struct LanguageServiceIdentity final
{
    std::string_view compilerRelease;
    std::string_view languageVersion;
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
    std::size_t documentCount() const noexcept;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};
}
