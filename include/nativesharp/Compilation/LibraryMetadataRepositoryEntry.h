#pragma once

#include <filesystem>

namespace nativesharp::compilation
{
class LibraryMetadataRepositoryEntry final
{
public:
    explicit LibraryMetadataRepositoryEntry(std::filesystem::path path);

    const std::filesystem::path& path() const noexcept;

private:
    std::filesystem::path path_;
};
}
