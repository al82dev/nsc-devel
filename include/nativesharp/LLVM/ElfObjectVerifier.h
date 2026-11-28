#pragma once

#include "nativesharp/LLVM/ElfObjectInfo.h"

#include <filesystem>
#include <string>

namespace nativesharp::llvm_backend
{
// Checks that a file is a 64-bit little-endian x86-64 ELF relocatable object whose section header table
// lies inside the file.
class ElfObjectVerifier final
{
public:
    bool verify(const std::filesystem::path& path);
    const ElfObjectInfo& info() const noexcept;
    const std::string& error() const noexcept;

private:
    ElfObjectInfo info_;
    std::string error_;
};
}
