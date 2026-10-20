#pragma once

#include "nativesharp/Linker/PdbInfo.h"

#include <filesystem>
#include <string>

namespace nativesharp::linker
{
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
