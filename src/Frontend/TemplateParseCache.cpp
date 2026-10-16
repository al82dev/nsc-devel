#include "nativesharp/Frontend/TemplateParseCache.h"

#include "nativesharp/Basic/Sha256.h"

#include <atomic>
#include <charconv>
#include <fstream>
#include <span>
#include <system_error>

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
} // namespace nativesharp::frontend
