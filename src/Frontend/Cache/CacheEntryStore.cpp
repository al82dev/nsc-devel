#include "CacheEntryStore.h"

#include <charconv>
#include <fstream>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace nativesharp::frontend::internal
{
CacheEntryStore::CacheEntryStore(
    std::atomic<unsigned long long>& temporaryOrdinal,
    std::string_view metadataHeader,
    std::string_view metadataLabel,
    std::string_view publicationLabel,
    bool writeThrough,
    bool extendedWindowsPaths) noexcept
    : temporaryOrdinal_(temporaryOrdinal),
      metadataHeader_(metadataHeader),
      metadataLabel_(metadataLabel),
      publicationLabel_(publicationLabel),
      writeThrough_(writeThrough),
      extendedWindowsPaths_(extendedWindowsPaths)
{
}

bool CacheEntryStore::validKey(std::string_view key) noexcept
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

std::filesystem::path CacheEntryStore::temporarySibling(
    const std::filesystem::path& destination) const
{
#if defined(_WIN32)
    const unsigned long process = ::GetCurrentProcessId();
#else
    const unsigned long process = 0;
#endif
    return destination.string() + ".tmp." + std::to_string(process) + "." +
        std::to_string(temporaryOrdinal_.fetch_add(1, std::memory_order_relaxed));
}

std::filesystem::path CacheEntryStore::filesystemPath(
    const std::filesystem::path& path) const
{
#if defined(_WIN32)
    if (!extendedWindowsPaths_) return path;
    std::error_code filesystemError;
    std::filesystem::path absolute = std::filesystem::absolute(path, filesystemError);
    if (filesystemError) absolute = path;
    const std::wstring value = absolute.native();
    if (value.starts_with(L"\\\\?\\")) return absolute;
    if (value.starts_with(L"\\\\"))
        return std::filesystem::path(L"\\\\?\\UNC\\" + value.substr(2));
    return std::filesystem::path(L"\\\\?\\" + value);
#else
    return path;
#endif
}

bool CacheEntryStore::replaceFileAtomically(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    std::string& error) const
{
#if defined(_WIN32)
    // Cache entries are rebuildable and authenticated on every restore. Keep the rename atomic,
    // but do not force every parallel object and metadata publication through a volume-wide
    // durability barrier. A power-loss partial publication remains a safe miss because metadata
    // is published last and restore verifies both size and SHA-256 before copying any object.
    const unsigned long flags = MOVEFILE_REPLACE_EXISTING |
        (writeThrough_ ? MOVEFILE_WRITE_THROUGH : 0UL);
    if (::MoveFileExW(
            filesystemPath(temporary).c_str(),
            filesystemPath(destination).c_str(),
            flags) != 0)
        return true;
    const unsigned long failure = ::GetLastError();
    std::error_code ignored;
    std::filesystem::remove(filesystemPath(temporary), ignored);
    error = "cannot atomically publish " + std::string(publicationLabel_) + " file '" +
        destination.string() + "' (Win32 error " + std::to_string(failure) + ")";
    return false;
#else
    std::error_code filesystemError;
    std::filesystem::rename(temporary, destination, filesystemError);
    if (!filesystemError) return true;
    std::filesystem::remove(temporary, filesystemError);
    error = "cannot atomically publish " + std::string(publicationLabel_) + " file '" +
        destination.string() + "'";
    return false;
#endif
}

bool CacheEntryStore::readMetadata(
    const std::filesystem::path& path,
    CacheMetadata& metadata) const
{
    std::ifstream stream(filesystemPath(path), std::ios::binary);
    std::string header;
    std::string keyLine;
    std::string digestLine;
    std::string sizeLine;
    std::string trailing;
    if (!stream || !std::getline(stream, header) || !std::getline(stream, keyLine) ||
        !std::getline(stream, digestLine) || !std::getline(stream, sizeLine) ||
        std::getline(stream, trailing) || header != metadataHeader_ ||
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

bool CacheEntryStore::writeMetadataAtomically(
    const std::filesystem::path& destination,
    std::string_view key,
    std::string_view digest,
    std::uintmax_t size,
    std::string& error) const
{
    const std::filesystem::path temporary = temporarySibling(destination);
    {
        std::ofstream stream(
            filesystemPath(temporary), std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            error = "cannot open " + std::string(metadataLabel_) +
                " metadata temporary '" + temporary.string() + "'";
            return false;
        }
        stream << metadataHeader_ << "\nkey " << key << "\nsha256 " << digest <<
            "\nsize " << size << '\n';
        if (!stream)
        {
            std::error_code ignored;
            std::filesystem::remove(filesystemPath(temporary), ignored);
            error = "cannot write " + std::string(metadataLabel_) + " metadata '" +
                temporary.string() + "'";
            return false;
        }
    }
    return replaceFileAtomically(temporary, destination, error);
}
}
