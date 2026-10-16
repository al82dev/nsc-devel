#include "nativesharp/Frontend/TemplateParseCache.h"

#include "nativesharp/Basic/Sha256.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <fstream>
#include <span>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace nativesharp::frontend
{
namespace
{
std::atomic<unsigned long long> temporaryOrdinal = 0;

std::filesystem::path temporarySibling(const std::filesystem::path& destination)
{
#if defined(_WIN32)
    const unsigned long process = ::GetCurrentProcessId();
#else
    const unsigned long process = 0;
#endif
    return destination.string() + ".tmp." + std::to_string(process) + "." +
        std::to_string(temporaryOrdinal.fetch_add(1, std::memory_order_relaxed));
}

bool replaceFileAtomically(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    std::string& error)
{
#if defined(_WIN32)
    if (::MoveFileExW(temporary.c_str(), destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
        return true;
    const unsigned long failure = ::GetLastError();
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    error = "cannot atomically publish template-cache file '" + destination.string() +
        "' (Win32 error " + std::to_string(failure) + ")";
    return false;
#else
    std::error_code filesystemError;
    std::filesystem::rename(temporary, destination, filesystemError);
    if (!filesystemError) return true;
    std::filesystem::remove(temporary, filesystemError);
    error = "cannot atomically publish template-cache file '" + destination.string() + "'";
    return false;
#endif
}

bool validKey(std::string_view key) noexcept
{
    if (key.size() != 64U) return false;
    for (const char character : key)
    {
        if ((character < '0' || character > '9') &&
            (character < 'a' || character > 'f'))
            return false;
    }
    return true;
}

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

struct CacheMetadata final
{
    std::string key;
    std::string digest;
    std::uintmax_t size = 0;
};

bool readMetadata(const std::filesystem::path& path, CacheMetadata& metadata)
{
    std::ifstream stream(path, std::ios::binary);
    std::string header;
    std::string keyLine;
    std::string digestLine;
    std::string sizeLine;
    std::string trailing;
    if (!stream || !std::getline(stream, header) || !std::getline(stream, keyLine) ||
        !std::getline(stream, digestLine) || !std::getline(stream, sizeLine) ||
        std::getline(stream, trailing) || header != "NSCTPL 1" ||
        !keyLine.starts_with("key ") || !digestLine.starts_with("sha256 ") ||
        !sizeLine.starts_with("size "))
        return false;
    metadata.key = keyLine.substr(4U);
    metadata.digest = digestLine.substr(7U);
    if (!validKey(metadata.key) || !validKey(metadata.digest)) return false;
    const std::string_view sizeText(sizeLine.data() + 5U, sizeLine.size() - 5U);
    const auto [end, parseError] = std::from_chars(
        sizeText.data(), sizeText.data() + sizeText.size(), metadata.size);
    return parseError == std::errc{} && end == sizeText.data() + sizeText.size();
}

bool writeMetadataAtomically(
    const std::filesystem::path& destination,
    std::string_view key,
    std::string_view digest,
    std::uintmax_t size,
    std::string& error)
{
    const std::filesystem::path temporary = temporarySibling(destination);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "cannot open template-cache metadata temporary '" + temporary.string() + "'";
            return false;
        }
        stream << "NSCTPL 1\nkey " << key << "\nsha256 " << digest <<
            "\nsize " << size << '\n';
        if (!stream)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            error = "cannot write template-cache metadata '" + temporary.string() + "'";
            return false;
        }
    }
    return replaceFileAtomically(temporary, destination, error);
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

bool TemplateParseCache::restore(
    std::string_view key,
    std::string& payload,
    bool& hit,
    std::string& /*error*/) const
{
    hit = false;
    payload.clear();
    if (!enabled() || !validKey(key)) return true;

    const std::filesystem::path payloadFile = payloadPath(root_, key);
    const std::filesystem::path metadataFile = metadataPath(root_, key);
    CacheMetadata metadata;
    if (!readMetadata(metadataFile, metadata))
    {
        // Missing entry is a valid cold miss, not an error.
        return true;
    }
    if (metadata.key != key)
    {
        quarantineInvalidEntry(root_, key, payloadFile, metadataFile);
        return true;
    }

    std::error_code filesystemError;
    const std::uintmax_t size = std::filesystem::file_size(payloadFile, filesystemError);
    if (filesystemError || size != metadata.size)
    {
        quarantineInvalidEntry(root_, key, payloadFile, metadataFile);
        return true;
    }
    std::ifstream stream(payloadFile, std::ios::binary);
    payload.resize(static_cast<std::size_t>(size));
    if (!stream.read(payload.data(), static_cast<std::streamsize>(size)))
    {
        quarantineInvalidEntry(root_, key, payloadFile, metadataFile);
        return true;
    }
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()};
    const std::string digest = basic::sha256Hex(basic::sha256(bytes));
    if (digest != metadata.digest)
    {
        quarantineInvalidEntry(root_, key, payloadFile, metadataFile);
        return true;
    }
    hit = true;
    return true;
}

bool TemplateParseCache::store(
    std::string_view key,
    std::string_view payload,
    std::string& error) const
{
    if (!enabled() || !validKey(key))
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

    const std::filesystem::path temporary = temporarySibling(payloadFile);
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
    if (!replaceFileAtomically(temporary, payloadFile, error)) return false;
    return writeMetadataAtomically(metadataFile, key, digest, payload.size(), error);
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

    struct Entry final
    {
        std::string key;
        std::filesystem::path payload;
        std::filesystem::path metadata;
        std::filesystem::file_time_type publication;
        std::uintmax_t size;
    };
    std::vector<Entry> entries;
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
        if (!validKey(key)) continue;
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
        entries.push_back(Entry{key, payload, entry.path(), publication, size});
    }
    if (filesystemError)
    {
        error = "cannot iterate template cache for eviction: " + filesystemError.message();
        return false;
    }
    if (totalBytes <= maximumBytes) return true;

    std::sort(entries.begin(), entries.end(),
        [](const Entry& left, const Entry& right) { return left.publication < right.publication; });
    for (const Entry& entry : entries)
    {
        if (totalBytes <= maximumBytes) break;
        std::error_code ignored;
        std::filesystem::remove(entry.metadata, ignored);
        std::filesystem::remove(entry.payload, ignored);
        ++evictedEntries;
        evictedBytes += entry.size;
        totalBytes -= entry.size;
    }
    return true;
}
} // namespace nativesharp::frontend
