#pragma once

#include <filesystem>

namespace nativesharp::linker
{
class TemporaryDirectory final
{
public:
    explicit TemporaryDirectory(std::filesystem::path path, bool keep = false) noexcept;

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    ~TemporaryDirectory();

    const std::filesystem::path& path() const noexcept;

private:
    std::filesystem::path path_;
    bool keep_ = false;
};
}
