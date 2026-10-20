#include "ScopedTemporaryDirectory.h"

#include <chrono>
#include <cstdint>
#include <system_error>
#include <utility>

namespace nativesharp::linker
{
std::unique_ptr<ScopedTemporaryDirectory> ScopedTemporaryDirectory::create(
    std::string_view prefix,
    std::string_view stageLabel,
    std::string_view keepRoot,
    std::string& error)
{
    error.clear();
    std::error_code filesystemError;
    const bool keep = !keepRoot.empty();
    std::filesystem::path root;
    if (keep)
    {
        root = std::filesystem::path(keepRoot);
        std::filesystem::create_directories(root, filesystemError);
        if (filesystemError)
        {
            error = "could not create the NSC_KEEP_LINK_TEMPS directory '" + std::string(keepRoot) +
                "': " + filesystemError.message();
            return nullptr;
        }
        filesystemError.clear();
    }
    else
    {
        root = std::filesystem::temp_directory_path(filesystemError);
        if (filesystemError)
        {
            error = "could not resolve the system temporary directory: " + filesystemError.message();
            return nullptr;
        }
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (std::uint32_t attempt = 0; attempt < 100; ++attempt)
    {
        const std::filesystem::path candidate = root /
            (std::string(prefix) + std::to_string(stamp) + "-" + std::to_string(attempt));
        filesystemError.clear();
        if (std::filesystem::create_directory(candidate, filesystemError))
            return std::unique_ptr<ScopedTemporaryDirectory>(
                new ScopedTemporaryDirectory(candidate, keep));
        if (filesystemError && filesystemError != std::errc::file_exists)
        {
            error = "could not create a " + std::string(stageLabel) +
                " temporary directory: " + filesystemError.message();
            return nullptr;
        }
    }
    error = "could not allocate a unique " + std::string(stageLabel) + " temporary directory";
    return nullptr;
}

ScopedTemporaryDirectory::ScopedTemporaryDirectory(
    std::filesystem::path path,
    const bool keep) noexcept
    : path_(std::move(path)), keep_(keep)
{
}

ScopedTemporaryDirectory::~ScopedTemporaryDirectory()
{
    if (keep_) return;
    std::error_code error;
    std::filesystem::remove_all(path_, error);
}

const std::filesystem::path& ScopedTemporaryDirectory::path() const noexcept
{
    return path_;
}
}
