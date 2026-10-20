#include "TemporaryDirectory.h"

#include <system_error>
#include <utility>

namespace nativesharp::linker
{
TemporaryDirectory::TemporaryDirectory(std::filesystem::path path, const bool keep) noexcept
    : path_(std::move(path)), keep_(keep)
{
}

TemporaryDirectory::~TemporaryDirectory()
{
    if (keep_)
    {
        return;
    }
    std::error_code error;
    std::filesystem::remove_all(path_, error);
}

const std::filesystem::path& TemporaryDirectory::path() const noexcept
{
    return path_;
}
}
