#include "TemporaryFile.h"

#include <system_error>
#include <utility>

namespace nativesharp::driver
{
TemporaryFile::TemporaryFile(std::filesystem::path path) : path_(std::move(path))
{
}

TemporaryFile::~TemporaryFile()
{
    std::error_code error;
    std::filesystem::remove(path_, error);
}
} // namespace nativesharp::driver
