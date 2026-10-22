#pragma once

#include "nativesharp/Compilation/LibraryMetadataRepositoryEntry.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace nativesharp::compilation
{
class LibraryMetadataRepository final
{
public:
    LibraryMetadataRepositoryEntry& entryFor(const std::filesystem::path& path);
    const LibraryMetadataRepositoryEntry* find(const std::filesystem::path& path) const;
    std::size_t size() const noexcept;

private:
    static std::filesystem::path canonicalPath(const std::filesystem::path& path);
    static std::string pathKey(const std::filesystem::path& path);

    std::unordered_map<std::string, std::unique_ptr<LibraryMetadataRepositoryEntry>> entries_;
};
}
