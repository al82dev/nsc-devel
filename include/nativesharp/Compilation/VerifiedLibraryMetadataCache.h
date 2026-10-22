#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nativesharp::compilation
{
class VerifiedLibraryMetadataCache final
{
public:
    void setRoot(std::filesystem::path root);

    bool restore(
        std::string_view key,
        std::vector<std::uint8_t>& payload,
        bool& hit,
        std::string& error) const;
    bool store(
        std::string_view key,
        std::span<const std::uint8_t> payload,
        std::string& error) const;
    void quarantine(std::string_view key) const noexcept;

private:
    std::filesystem::path root_;
};
}
