#pragma once

#include "nativesharp/LLVM/CoffObjectInfo.h"

#include <filesystem>
#include <string>

namespace nativesharp::llvm_backend
{
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
