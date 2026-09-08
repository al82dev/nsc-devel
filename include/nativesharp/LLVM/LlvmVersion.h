#pragma once

#include <string>

namespace nativesharp::llvm_backend
{
inline constexpr unsigned kSupportedLlvmMajor = 23;

struct LlvmVersion final
{
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
};

LlvmVersion loadedLlvmVersion() noexcept;
bool isSupportedLlvmVersion(const LlvmVersion& version) noexcept;
std::string llvmVersionString(const LlvmVersion& version);
bool verifyLoadedLlvmVersion(std::string& error);
} // namespace nativesharp::llvm_backend
