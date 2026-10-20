#pragma once

#include "nativesharp/Linker/CoffArchiveInfo.h"

#include <filesystem>
#include <string>

namespace nativesharp::linker
{
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
