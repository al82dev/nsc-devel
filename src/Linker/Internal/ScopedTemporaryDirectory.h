#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace nativesharp::linker
{
class ScopedTemporaryDirectory final
{
public:
    static std::unique_ptr<ScopedTemporaryDirectory> create(
        std::string_view prefix,
        std::string_view stageLabel,
        std::string_view keepRoot,
        std::string& error);

    ScopedTemporaryDirectory(const ScopedTemporaryDirectory&) = delete;
    ScopedTemporaryDirectory& operator=(const ScopedTemporaryDirectory&) = delete;

    ~ScopedTemporaryDirectory();

    const std::filesystem::path& path() const noexcept;

private:
    explicit ScopedTemporaryDirectory(std::filesystem::path path, bool keep) noexcept;

    std::filesystem::path path_;
    bool keep_ = false;
};
}
