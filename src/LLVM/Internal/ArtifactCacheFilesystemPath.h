#pragma once

#include <filesystem>

namespace nativesharp::llvm_backend
{
#if defined(_WIN32)
std::filesystem::path extendedWindowsPath(const std::filesystem::path& input);
#endif

std::filesystem::path cacheFilesystemPath(const std::filesystem::path& input);
}
