#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace nsc::suite::writer
{
// Produces the Win32 extended spelling needed to inspect fixture paths.
// It performs no filesystem mutation and requires O(path length) time and memory.
inline std::filesystem::path extendedPath(const std::filesystem::path& input)
{
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(input, error);
    if (error)
    {
        absolute = input;
    }
    const std::wstring value = absolute.native();
    if (value.starts_with(L"\\\\?\\"))
    {
        return absolute;
    }
    if (value.starts_with(L"\\\\"))
    {
        return std::filesystem::path(L"\\\\?\\UNC\\" + value.substr(2));
    }
    return std::filesystem::path(L"\\\\?\\" + value);
}

// Orders cleanup paths from deepest to shallowest without capturing mutable fixture state.
inline bool longerPathFirst(
    const std::filesystem::path& left,
    const std::filesystem::path& right) noexcept
{
    return left.native().size() > right.native().size();
}
}
