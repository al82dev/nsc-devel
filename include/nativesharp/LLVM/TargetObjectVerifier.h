#pragma once

#include "nativesharp/LLVM/TargetObjectInfo.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace nativesharp::llvm_backend
{
// Verifies an emitted object in the object format of TargetInfo::DefaultX64(): COFF or ELF.
class TargetObjectVerifier final
{
public:
    bool verify(const std::filesystem::path& path);
    const TargetObjectInfo& info() const noexcept;
    const std::string& error() const noexcept;
    std::string_view formatName() const noexcept;
    std::string_view machineName() const noexcept;

private:
    TargetObjectInfo info_;
    std::string error_;
};
}
