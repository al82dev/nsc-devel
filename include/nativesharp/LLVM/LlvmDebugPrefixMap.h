#pragma once

#include "nativesharp/LLVM/DebugPrefixMap.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

// NSC305-L03. Debug source-path remapping moved out of ProjectOopLlvmLowerer (pure move).
namespace nativesharp::llvm_backend
{
std::string portablePath(const std::filesystem::path& path);

std::string foldedWindowsPath(std::string path);

bool pathHasPrefix(std::string_view path, std::string_view prefix);

std::string remapDebugSourcePath(
    const std::filesystem::path& sourcePath,
    std::span<const DebugPrefixMap> mappings);
} // namespace nativesharp::llvm_backend
