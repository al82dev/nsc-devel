#include "TemporaryFiles.h"

#include <system_error>
#include <utility>

namespace nativesharp::linker
{
TemporaryFiles::TemporaryFiles(
    std::filesystem::path directory,
    std::filesystem::path object,
    std::filesystem::path archive)
    : directory_(std::move(directory)), object_(std::move(object)), archive_(std::move(archive))
{
}

TemporaryFiles::~TemporaryFiles()
{
    std::error_code ignored;
    std::filesystem::remove_all(directory_, ignored);
}

const std::filesystem::path& TemporaryFiles::object() const noexcept
{
    return object_;
}

const std::filesystem::path& TemporaryFiles::archive() const noexcept
{
    return archive_;
}
}
