#include "nativesharp/Frontend/TemplateParseCache.h"
#include "Cache/CacheEntryStore.h"
#include "Internal/TemplateParseCacheEntry.h"

#include "nativesharp/Basic/Sha256.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <span>
#include <system_error>
#include <vector>

namespace nativesharp::frontend
{
namespace
{
std::atomic<unsigned long long> temporaryOrdinal = 0;
// EN: Publications are atomic renames without MOVEFILE_WRITE_THROUGH, like the object and code-partition stores:
//     every entry is rebuildable, its metadata is published last, and `readTemplateEntry` checks the size and the
//     SHA-256 before using a payload, so a publication lost to a power failure is a miss. The write-through flag
//     forced a durability barrier on every corpus and syntax publication.
// ES: Las publicaciones son renombres atómicos sin MOVEFILE_WRITE_THROUGH, igual que los almacenes de objetos y de
//     particiones: cada entrada se puede reconstruir, su metadata se publica al final y `readTemplateEntry`
//     comprueba el tamaño y el SHA-256 antes de usar un payload, así que una publicación perdida por un corte de
//     energía es un fallo de caché. La bandera write-through forzaba una barrera de durabilidad en cada publicación
//     del corpus y de la sintaxis.
internal::CacheEntryStore templateEntryStore(
    temporaryOrdinal, "NSCTPL 1", "template-cache", "template-cache", false, false);

std::filesystem::path entryDirectory(
    const std::filesystem::path& root,
    std::string_view key)
{
    return root / "templates" / std::string(key.substr(0, 2));
}

std::filesystem::path payloadPath(
    const std::filesystem::path& root,
    std::string_view key)
{
    return entryDirectory(root, key) / (std::string(key) + ".tpl");
}

std::filesystem::path metadataPath(
    const std::filesystem::path& root,
    std::string_view key)
{
    return entryDirectory(root, key) / (std::string(key) + ".meta");
}

void quarantineInvalidEntry(
    const std::filesystem::path& root,
    std::string_view key,
    const std::filesystem::path& payload,
    const std::filesystem::path& metadata) noexcept
{
    std::error_code filesystemError;
    const std::filesystem::path quarantine = root / "quarantine";
    std::filesystem::create_directories(quarantine, filesystemError);
    if (filesystemError) return;
    const std::string suffix = std::to_string(
        temporaryOrdinal.fetch_add(1, std::memory_order_relaxed));
    const std::filesystem::path quarantinedMetadata =
        quarantine / (std::string(key) + "." + suffix + ".meta");
    const std::filesystem::path quarantinedPayload =
        quarantine / (std::string(key) + "." + suffix + ".tpl");
    filesystemError.clear();
    std::filesystem::rename(metadata, quarantinedMetadata, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(payload, quarantinedPayload, filesystemError);
}

bool locateTemplateEntry(
    const std::filesystem::path& root,
    std::string_view key,
    const std::filesystem::path& payload,
    const std::filesystem::path& metadata,
    TemplateParseCacheLookupReason& reason)
{
    std::error_code filesystemError;
    const bool metadataExists = std::filesystem::exists(metadata, filesystemError);
    if (filesystemError)
    {
        reason = TemplateParseCacheLookupReason::Unavailable;
        return false;
    }
    const bool payloadExists = std::filesystem::exists(payload, filesystemError);
    if (filesystemError)
    {
        reason = TemplateParseCacheLookupReason::Unavailable;
        return false;
    }
    if (!metadataExists && !payloadExists)
    {
        reason = TemplateParseCacheLookupReason::Cold;
        return false;
    }
    if (metadataExists && payloadExists) return true;
    reason = TemplateParseCacheLookupReason::IncompletePublication;
    quarantineInvalidEntry(root, key, payload, metadata);
    return false;
}

TemplateParseCacheLookupReason readTemplateEntry(
    const std::filesystem::path& root,
    std::string_view key,
    const std::filesystem::path& payloadFile,
    const std::filesystem::path& metadataFile,
    std::string& payload)
{
    internal::CacheMetadata metadata;
    if (!templateEntryStore.readMetadata(metadataFile, metadata) || metadata.key != key)
    {
        quarantineInvalidEntry(root, key, payloadFile, metadataFile);
        return TemplateParseCacheLookupReason::Corruption;
    }
    std::error_code filesystemError;
    const std::uintmax_t size = std::filesystem::file_size(payloadFile, filesystemError);
    if (filesystemError || size != metadata.size)
    {
        quarantineInvalidEntry(root, key, payloadFile, metadataFile);
        return TemplateParseCacheLookupReason::Corruption;
    }
    std::ifstream stream(payloadFile, std::ios::binary);
    payload.resize(static_cast<std::size_t>(size));
    if (!stream.read(payload.data(), static_cast<std::streamsize>(size)))
    {
        quarantineInvalidEntry(root, key, payloadFile, metadataFile);
        return TemplateParseCacheLookupReason::Corruption;
    }
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()};
    const std::string digest = basic::sha256Hex(basic::sha256(bytes));
    if (digest == metadata.digest) return TemplateParseCacheLookupReason::Hit;
    quarantineInvalidEntry(root, key, payloadFile, metadataFile);
    return TemplateParseCacheLookupReason::Corruption;
}

using internal::CacheMetadata;
using internal::TemplateParseCacheEntry;

void evictOldestEntries(
    std::vector<TemplateParseCacheEntry>& entries,
    std::uintmax_t maximumBytes,
    std::uintmax_t& totalBytes,
    std::size_t& evictedEntries,
    std::uintmax_t& evictedBytes)
{
    std::sort(entries.begin(), entries.end(),
        [](const TemplateParseCacheEntry& left, const TemplateParseCacheEntry& right)
        {
            return left.publication < right.publication;
        });
    for (const TemplateParseCacheEntry& entry : entries)
    {
        if (totalBytes <= maximumBytes) break;
        std::error_code ignored;
        std::filesystem::remove(entry.metadata, ignored);
        std::filesystem::remove(entry.payload, ignored);
        ++evictedEntries;
        evictedBytes += entry.size;
        totalBytes -= entry.size;
    }
}
} // namespace

void TemplateParseCache::setRoot(std::filesystem::path root)
{
    root_ = std::move(root).lexically_normal();
}

bool TemplateParseCache::enabled() const noexcept
{
    return !root_.empty();
}

const std::filesystem::path& TemplateParseCache::root() const noexcept
{
    return root_;
}

const char* templateParseCacheLookupReasonName(
    TemplateParseCacheLookupReason reason) noexcept
{
    switch (reason)
    {
        case TemplateParseCacheLookupReason::Hit: return "hit";
        case TemplateParseCacheLookupReason::Disabled: return "disabled";
        case TemplateParseCacheLookupReason::InvalidKey: return "invalid-key";
        case TemplateParseCacheLookupReason::Cold: return "cold";
        case TemplateParseCacheLookupReason::IncompletePublication:
            return "incomplete-publication";
        case TemplateParseCacheLookupReason::Corruption: return "corruption";
        case TemplateParseCacheLookupReason::Unavailable: return "unavailable";
    }
    return "unavailable";
}

bool TemplateParseCache::restore(
    std::string_view key,
    std::string& payload,
    bool& hit,
    std::string& error) const
{
    TemplateParseCacheLookupReason ignoredReason =
        TemplateParseCacheLookupReason::Cold;
    return restore(key, payload, hit, ignoredReason, error);
}

bool TemplateParseCache::restore(
    std::string_view key,
    std::string& payload,
    bool& hit,
    TemplateParseCacheLookupReason& reason,
    std::string& /*error*/) const
{
    hit = false;
    payload.clear();
    if (!enabled())
    {
        reason = TemplateParseCacheLookupReason::Disabled;
        return true;
    }
    if (!internal::CacheEntryStore::validKey(key))
    {
        reason = TemplateParseCacheLookupReason::InvalidKey;
        return true;
    }

    const std::filesystem::path payloadFile = payloadPath(root_, key);
    const std::filesystem::path metadataFile = metadataPath(root_, key);
    if (!locateTemplateEntry(root_, key, payloadFile, metadataFile, reason)) return true;
    reason = readTemplateEntry(root_, key, payloadFile, metadataFile, payload);
    hit = reason == TemplateParseCacheLookupReason::Hit;
    if (!hit) payload.clear();
    return true;
}

void TemplateParseCache::quarantine(std::string_view key) const noexcept
{
    if (!enabled() || !internal::CacheEntryStore::validKey(key)) return;
    quarantineInvalidEntry(
        root_, key, payloadPath(root_, key), metadataPath(root_, key));
}

bool TemplateParseCache::store(
    std::string_view key,
    std::string_view payload,
    std::string& error) const
{
    if (!enabled() || !internal::CacheEntryStore::validKey(key))
    {
        error = "template-cache store requires an enabled cache and a 64-char hex key";
        return false;
    }
    const std::filesystem::path payloadFile = payloadPath(root_, key);
    const std::filesystem::path metadataFile = metadataPath(root_, key);
    std::error_code filesystemError;
    std::filesystem::create_directories(payloadFile.parent_path(), filesystemError);
    if (filesystemError)
    {
        error = "cannot create template-cache directory '" +
            payloadFile.parent_path().string() + "'";
        return false;
    }

    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()};
    const std::string digest = basic::sha256Hex(basic::sha256(bytes));

    const std::filesystem::path temporary = templateEntryStore.temporarySibling(payloadFile);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "cannot open template-cache payload temporary '" + temporary.string() + "'";
            return false;
        }
        stream.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        if (!stream)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            error = "cannot write template-cache payload '" + temporary.string() + "'";
            return false;
        }
    }
    if (!templateEntryStore.replaceFileAtomically(temporary, payloadFile, error)) return false;
    return templateEntryStore.writeMetadataAtomically(
        metadataFile, key, digest, payload.size(), error);
}

// NSC305-C05. Evicts entries (oldest publication first) until the total is under `maximumBytes`.
bool TemplateParseCache::prune(
    std::uintmax_t maximumBytes,
    std::size_t& evictedEntries,
    std::uintmax_t& evictedBytes,
    std::string& error) const
{
    evictedEntries = 0;
    evictedBytes = 0;
    error.clear();
    if (!enabled()) return true;
    const std::filesystem::path templates = root_ / "templates";
    std::error_code filesystemError;
    const bool templatesExist = std::filesystem::is_directory(templates, filesystemError);
    if (filesystemError)
    {
        if (filesystemError == std::errc::no_such_file_or_directory) return true;
        error = "cannot inspect template cache for eviction: " + filesystemError.message();
        return false;
    }
    if (!templatesExist) return true;

    std::vector<TemplateParseCacheEntry> entries;
    std::uintmax_t totalBytes = 0;
    std::filesystem::recursive_directory_iterator iterator(
        templates, std::filesystem::directory_options::skip_permission_denied, filesystemError);
    const std::filesystem::recursive_directory_iterator end;
    while (!filesystemError && iterator != end)
    {
        const std::filesystem::directory_entry entry = *iterator;
        iterator.increment(filesystemError);
        const bool regular = entry.is_regular_file(filesystemError);
        if (filesystemError)
        {
            filesystemError.clear();
            continue;
        }
        if (!regular || entry.path().extension() != ".meta") continue;
        const std::string key = entry.path().stem().string();
        if (!internal::CacheEntryStore::validKey(key)) continue;
        const std::filesystem::path payload = payloadPath(root_, key);
        const std::uintmax_t size = std::filesystem::file_size(payload, filesystemError);
        if (filesystemError)
        {
            filesystemError.clear();
            continue;
        }
        std::filesystem::file_time_type publication =
            std::filesystem::last_write_time(entry, filesystemError);
        if (filesystemError)
        {
            filesystemError.clear();
            continue;
        }
        totalBytes += size;
        entries.push_back(TemplateParseCacheEntry{key, payload, entry.path(), publication, size});
    }
    if (filesystemError)
    {
        error = "cannot iterate template cache for eviction: " + filesystemError.message();
        return false;
    }
    if (totalBytes <= maximumBytes) return true;

    evictOldestEntries(entries, maximumBytes, totalBytes, evictedEntries, evictedBytes);
    return true;
}
} // namespace nativesharp::frontend
