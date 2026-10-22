#include "nativesharp/Compilation/VerifiedLibraryMetadataCache.h"

#include "nativesharp/Basic/ArtifactTransaction.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <system_error>
#include <utility>

namespace nativesharp::compilation
{
namespace
{
constexpr std::string_view kCacheMagic = "NSCNSMETA1\n";
constexpr std::uintmax_t kMaximumMetadataBytes = 100ULL + 128ULL * 1024ULL * 1024ULL;
std::atomic<std::uint64_t> quarantineOrdinal{0};

bool validKey(std::string_view key)
{
    return key.size() == 64U && std::ranges::all_of(key, [](unsigned char character) {
        return std::isdigit(character) != 0 ||
            (character >= static_cast<unsigned char>('a') &&
             character <= static_cast<unsigned char>('f'));
    });
}

std::filesystem::path cachePath(const std::filesystem::path& root, std::string_view key)
{
    return root / std::string(key.substr(0, 2)) / (std::string(key) + ".nsmeta-model");
}
}

void VerifiedLibraryMetadataCache::setRoot(std::filesystem::path root)
{
    root_ = std::move(root).lexically_normal();
}

bool VerifiedLibraryMetadataCache::restore(
    std::string_view key,
    std::vector<std::uint8_t>& payload,
    bool& hit,
    std::string& error) const
{
    hit = false;
    payload.clear();
    error.clear();
    if (root_.empty() || !validKey(key)) return true;
    const std::filesystem::path source = cachePath(root_, key);
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(source, filesystemError)) return !filesystemError;
    const std::uintmax_t size = std::filesystem::file_size(source, filesystemError);
    if (filesystemError || size > kCacheMagic.size() + 65U + kMaximumMetadataBytes)
    {
        error = "verified NSMETA cache entry has invalid size";
        quarantine(key);
        return false;
    }
    std::ifstream stream(source, std::ios::binary);
    std::string stored(static_cast<std::size_t>(size), '\0');
    if (!stream.read(stored.data(), static_cast<std::streamsize>(stored.size())))
    {
        error = "cannot read verified NSMETA cache entry";
        quarantine(key);
        return false;
    }
    const std::string prefix = std::string(kCacheMagic) + std::string(key) + "\n";
    if (!stored.starts_with(prefix))
    {
        error = "verified NSMETA cache entry header mismatch";
        quarantine(key);
        return false;
    }
    payload.assign(stored.begin() + static_cast<std::ptrdiff_t>(prefix.size()), stored.end());
    hit = true;
    return true;
}

bool VerifiedLibraryMetadataCache::store(
    std::string_view key,
    std::span<const std::uint8_t> payload,
    std::string& error) const
{
    error.clear();
    if (root_.empty() || !validKey(key)) return true;
    const std::filesystem::path destination = cachePath(root_, key);
    std::error_code filesystemError;
    std::filesystem::create_directories(destination.parent_path(), filesystemError);
    if (filesystemError)
    {
        error = "cannot create verified NSMETA cache directory";
        return false;
    }
    std::vector<std::uint8_t> stored;
    const std::string prefix = std::string(kCacheMagic) + std::string(key) + "\n";
    stored.reserve(prefix.size() + payload.size());
    stored.insert(stored.end(), prefix.begin(), prefix.end());
    stored.insert(stored.end(), payload.begin(), payload.end());
    return basic::writeFileTransactionally(destination, stored, error);
}

void VerifiedLibraryMetadataCache::quarantine(std::string_view key) const noexcept
{
    if (root_.empty() || !validKey(key)) return;
    const std::filesystem::path source = cachePath(root_, key);
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(source, filesystemError)) return;
    const std::filesystem::path quarantineRoot = root_ / "quarantine";
    std::filesystem::create_directories(quarantineRoot, filesystemError);
    if (filesystemError) return;
    const std::string suffix = std::to_string(
        quarantineOrdinal.fetch_add(1, std::memory_order_relaxed));
    const std::filesystem::path destination =
        quarantineRoot / (std::string(key) + "." + suffix + ".nsmeta-model");
    filesystemError.clear();
    std::filesystem::rename(source, destination, filesystemError);
    if (!filesystemError) return;
    filesystemError.clear();
    std::filesystem::remove(source, filesystemError);
}
}
