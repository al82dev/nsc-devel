#include "nativesharp/Compilation/LibraryMetadataRepository.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace nativesharp::compilation
{
LibraryMetadataRepositoryEntry& LibraryMetadataRepository::entryFor(
    const std::filesystem::path& path)
{
    const std::filesystem::path canonical = canonicalPath(path);
    const std::string key = pathKey(canonical);
    auto [iterator, inserted] = entries_.try_emplace(key);
    if (inserted)
    {
        iterator->second = std::make_unique<LibraryMetadataRepositoryEntry>(canonical);
    }
    return *iterator->second;
}

const LibraryMetadataRepositoryEntry* LibraryMetadataRepository::find(
    const std::filesystem::path& path) const
{
    const auto iterator = entries_.find(pathKey(canonicalPath(path)));
    return iterator == entries_.end() ? nullptr : iterator->second.get();
}

std::size_t LibraryMetadataRepository::size() const noexcept
{
    return entries_.size();
}

std::filesystem::path LibraryMetadataRepository::canonicalPath(
    const std::filesystem::path& path)
{
    std::error_code filesystemError;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, filesystemError);
    if (!filesystemError) return canonical;

    filesystemError.clear();
    canonical = std::filesystem::absolute(path, filesystemError);
    return (filesystemError ? path : canonical).lexically_normal();
}

std::string LibraryMetadataRepository::pathKey(const std::filesystem::path& path)
{
    std::string key = path.generic_string();
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return key;
}
}
