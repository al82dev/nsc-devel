#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace nativesharp::llvm_backend
{
struct CoffObjectInfo final
{
    std::uint16_t machine = 0;
    std::uint16_t sectionCount = 0;
    std::uint16_t optionalHeaderSize = 0;
};

class CoffObjectVerifier final
{
public:
    bool verify(const std::filesystem::path& path);
    const CoffObjectInfo& info() const noexcept;
    const std::string& error() const noexcept;

private:
    CoffObjectInfo info_;
    std::string error_;
};
}
