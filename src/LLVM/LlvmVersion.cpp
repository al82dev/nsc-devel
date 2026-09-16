#include "nativesharp/LLVM/LlvmVersion.h"

#include "LlvmCApi.h"

#include <string>

namespace nativesharp::llvm_backend
{
LlvmVersion loadedLlvmVersion() noexcept
{
    LlvmVersion version;
    llvm_api::LLVMGetVersion(&version.major, &version.minor, &version.patch);
    return version;
}

bool isSupportedLlvmVersion(const LlvmVersion& version) noexcept
{
    return version.major == kSupportedLlvmMajor;
}

std::string llvmVersionString(const LlvmVersion& version)
{
    return std::to_string(version.major) + '.' + std::to_string(version.minor) + '.' + std::to_string(version.patch);
}

bool verifyLoadedLlvmVersion(std::string& error)
{
    error.clear();
    const LlvmVersion version = loadedLlvmVersion();
    if (isSupportedLlvmVersion(version))
    {
        return true;
    }

    error = "unsupported loaded LLVM " + llvmVersionString(version) + "; NativeSharp 2.0 supports LLVM 23.x";
    return false;
}
} // namespace nativesharp::llvm_backend
