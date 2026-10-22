#pragma once

#include "nativesharp/Frontend/TemplateParseCacheLookupReason.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::frontend
{
// NSC305-C01. The C1 template parse cache: stores/restores the serialized parsed-template payload
// keyed by the C1 cache key, with a metadata file (magic + key + SHA-256 + size), atomic publication
// and corruption quarantine. Follows the CompilationArtifactCache layout so both share one on-disk
// convention. This class only moves bytes; the AST serialization is a separate concern.
class TemplateParseCache final
{
public:
    void setRoot(std::filesystem::path root);
    bool enabled() const noexcept;
    const std::filesystem::path& root() const noexcept;

    // Restores the payload for `key`. On success `hit` is true and `payload` holds the bytes; a
    // valid miss sets `hit` false. Corruption quarantines the entry and sets `hit` false.
    bool restore(
        std::string_view key,
        std::string& payload,
        bool& hit,
        std::string& error) const;

    bool restore(
        std::string_view key,
        std::string& payload,
        bool& hit,
        TemplateParseCacheLookupReason& reason,
        std::string& error) const;

    void quarantine(std::string_view key) const noexcept;

    // Stores `payload` under `key` (atomic publication).
    bool store(
        std::string_view key,
        std::string_view payload,
        std::string& error) const;

    // NSC305-C05. Evicts entries (oldest publication first) until the total stays under
    // `maximumBytes`. Reports the evicted count and bytes.
    bool prune(
        std::uintmax_t maximumBytes,
        std::size_t& evictedEntries,
        std::uintmax_t& evictedBytes,
        std::string& error) const;

private:
    std::filesystem::path root_;
};
} // namespace nativesharp::frontend
