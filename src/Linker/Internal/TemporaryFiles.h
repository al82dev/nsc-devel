#pragma once

#include <filesystem>

namespace nativesharp::linker
{
class TemporaryFiles final
{
public:
    TemporaryFiles(
        std::filesystem::path directory,
        std::filesystem::path object,
        std::filesystem::path archive);
    ~TemporaryFiles();

    const std::filesystem::path& object() const noexcept;
    const std::filesystem::path& archive() const noexcept;

private:
    std::filesystem::path directory_;
    std::filesystem::path object_;
    std::filesystem::path archive_;
};
}
