#include "nativesharp/Compilation/FileDigestCache.h"

#include "nativesharp/Basic/ArtifactTransaction.h"

#include <array>
#include <fstream>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

namespace nativesharp::compilation
{
namespace
{
constexpr std::string_view kEntryMagic = "NSCFILEDIGEST1\n";

std::string entryKey(
    const std::filesystem::path& canonical,
    std::uintmax_t size,
    std::filesystem::file_time_type writeTime)
{
    std::string material = "nsc-file-digest-v1\n";
    material += canonical.generic_string();
    material.push_back('\n');
    material += std::to_string(size);
    material.push_back('\n');
    material += std::to_string(writeTime.time_since_epoch().count());
    const std::span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(material.data()), material.size()};
    return basic::sha256Hex(basic::sha256(bytes));
}

bool parseDigest(std::string_view text, basic::Sha256Digest& digest)
{
    if (text.size() != digest.size() * 2U) return false;
    for (std::size_t index = 0; index < digest.size(); ++index)
    {
        unsigned value = 0;
        for (std::size_t half = 0; half < 2U; ++half)
        {
            const char character = text[index * 2U + half];
            value <<= 4U;
            if (character >= '0' && character <= '9')
                value |= static_cast<unsigned>(character - '0');
            else if (character >= 'a' && character <= 'f')
                value |= static_cast<unsigned>(character - 'a' + 10);
            else
                return false;
        }
        digest[index] = static_cast<std::uint8_t>(value);
    }
    return true;
}
} // namespace

void FileDigestCache::setRoot(std::filesystem::path root)
{
    root_ = std::move(root).lexically_normal();
}

bool FileDigestCache::enabled() const noexcept
{
    return !root_.empty();
}

bool FileDigestCache::digest(
    const std::filesystem::path& path,
    basic::Sha256Digest& digest,
    std::uintmax_t& size,
    std::string& error) const
{
    if (root_.empty()) return basic::sha256File(path, digest, size, error);
    std::error_code filesystemError;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, filesystemError);
    std::uintmax_t statSize = 0;
    std::filesystem::file_time_type writeTime{};
    if (!filesystemError) statSize = std::filesystem::file_size(canonical, filesystemError);
    if (!filesystemError) writeTime = std::filesystem::last_write_time(canonical, filesystemError);
    if (filesystemError) return basic::sha256File(path, digest, size, error);

    const std::string key = entryKey(canonical, statSize, writeTime);
    const std::filesystem::path entry = root_ / key.substr(0, 2) / (key + ".digest");
    std::ifstream stream(entry, std::ios::binary);
    if (stream)
    {
        std::array<char, 160> buffer{};
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::string_view stored(buffer.data(), static_cast<std::size_t>(stream.gcount()));
        const std::string prefix = std::string(kEntryMagic) + key + "\n";
        if (stored.starts_with(prefix) &&
            parseDigest(stored.substr(prefix.size(), digest.size() * 2U), digest) &&
            stored.size() == prefix.size() + digest.size() * 2U + 1U && stored.back() == '\n')
        {
            size = statSize;
            error.clear();
            return true;
        }
    }

    if (!basic::sha256File(path, digest, size, error)) return false;
    // A file that changed while it was hashed is not recorded: the stat key would not describe it.
    if (size != statSize) return true;
    std::filesystem::create_directories(entry.parent_path(), filesystemError);
    if (filesystemError) return true;
    const std::string text = std::string(kEntryMagic) + key + "\n" + basic::sha256Hex(digest) + "\n";
    std::string ignoredError;
    (void)basic::writeTextTransactionally(entry, text, ignoredError);
    return true;
}
} // namespace nativesharp::compilation
