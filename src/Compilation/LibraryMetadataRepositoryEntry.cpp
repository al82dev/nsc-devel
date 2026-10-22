#include "nativesharp/Compilation/LibraryMetadataRepositoryEntry.h"

#include <utility>

namespace nativesharp::compilation
{
LibraryMetadataRepositoryEntry::LibraryMetadataRepositoryEntry(std::filesystem::path path)
    : path_(std::move(path))
{
}

const std::filesystem::path& LibraryMetadataRepositoryEntry::path() const noexcept
{
    return path_;
}
}
