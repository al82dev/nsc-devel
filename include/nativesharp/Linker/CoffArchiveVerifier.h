#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace nativesharp::linker
{
struct CoffArchiveInfo final
{
    std::size_t memberCount = 0;
    std::vector<std::string> symbolNames;
};

class CoffArchiveVerifier final
{
public:
    bool verify(const std::filesystem::path& path);
    const CoffArchiveInfo& info() const noexcept;
    const std::string& error() const noexcept;

private:
    CoffArchiveInfo info_;
    std::string error_;
};
}
