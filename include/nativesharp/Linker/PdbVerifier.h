#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace nativesharp::linker
{
struct PdbInfo final
{
    std::uint32_t version = 0;
    std::uint32_t signature = 0;
    std::uint32_t age = 0;
    std::array<std::uint8_t, 16> guid{};
    std::uint32_t streamCount = 0;
    bool hasModuleRecords = false;
    bool hasPublicSymbols = false;
    bool hasSourceFiles = false;
    bool hasSourceChecksums = false;
};

class PdbVerifier final
{
public:
    bool verify(const std::filesystem::path& path);
    const PdbInfo& info() const noexcept;
    const std::string& error() const noexcept;

private:
    PdbInfo info_;
    std::string error_;
};
}
