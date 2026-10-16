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
inline std::string portablePath(const std::filesystem::path& path)
{
    const std::u8string encoded = path.lexically_normal().generic_u8string();
    std::string result(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    std::replace(result.begin(), result.end(), '\\', '/');
    if (result.size() >= 2 && result[1] == ':' && result[0] >= 'a' && result[0] <= 'z')
        result[0] = static_cast<char>(result[0] - 'a' + 'A');
    while (result.size() > 1 && result.back() == '/' &&
           !(result.size() == 3 && result[1] == ':'))
        result.pop_back();
    return result;
}

inline std::string foldedWindowsPath(std::string path)
{
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char value) {
        return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a')
                                            : static_cast<char>(value);
    });
    return path;
}

inline bool pathHasPrefix(std::string_view path, std::string_view prefix)
{
    if (!path.starts_with(prefix)) return false;
    if (path.size() == prefix.size() || prefix.ends_with('/')) return true;
    return path[prefix.size()] == '/';
}

inline std::string remapDebugSourcePath(
    const std::filesystem::path& sourcePath,
    std::span<const DebugPrefixMap> mappings)
{
    std::error_code filesystemError;
    std::filesystem::path absolute = std::filesystem::absolute(sourcePath, filesystemError);
    if (filesystemError) absolute = sourcePath;
    const std::string source = portablePath(absolute);
    const std::string foldedSource = foldedWindowsPath(source);

    std::size_t selectedLength = 0;
    std::string selectedReplacement;
    for (const DebugPrefixMap& mapping : mappings)
    {
        const std::string prefix = portablePath(mapping.sourcePrefix);
        const std::string foldedPrefix = foldedWindowsPath(prefix);
        if (!pathHasPrefix(foldedSource, foldedPrefix) || prefix.size() < selectedLength) continue;
        selectedLength = prefix.size();
        selectedReplacement = portablePath(mapping.replacementPrefix);
    }
    if (selectedLength == 0) return source;

    const std::string_view suffix(source.data() + selectedLength, source.size() - selectedLength);
    if (selectedReplacement.empty())
        return !suffix.empty() && suffix.front() == '/' ? std::string(suffix.substr(1))
                                                        : std::string(suffix);
    if (!suffix.empty() && selectedReplacement.back() == '/' && suffix.front() == '/')
        selectedReplacement.pop_back();
    else if (!suffix.empty() && selectedReplacement.back() != '/' && suffix.front() != '/')
        selectedReplacement.push_back('/');
    selectedReplacement.append(suffix);
    return selectedReplacement;
}
} // namespace nativesharp::llvm_backend
