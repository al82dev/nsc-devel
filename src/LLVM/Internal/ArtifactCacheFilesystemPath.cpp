#include "ArtifactCacheFilesystemPath.h"

#include <system_error>

namespace nativesharp::llvm_backend
{
#if defined(_WIN32)
std::filesystem::path extendedWindowsPath(const std::filesystem::path& input)
{
    std::error_code filesystemError;
    std::filesystem::path absolute = std::filesystem::absolute(input, filesystemError);
    if (filesystemError) absolute = input;
    const std::wstring value = absolute.native();
    if (value.starts_with(L"\\\\?\\")) return absolute;
    if (value.starts_with(L"\\\\"))
        return std::filesystem::path(L"\\\\?\\UNC\\" + value.substr(2));
    return std::filesystem::path(L"\\\\?\\" + value);
}
#endif

std::filesystem::path cacheFilesystemPath(const std::filesystem::path& input)
{
#if defined(_WIN32)
    return extendedWindowsPath(input);
#else
    return input;
#endif
}
}
