#include "CacheEntryLock.h"

#include "ArtifactCacheFilesystemPath.h"

#include <chrono>
#include <system_error>
#include <thread>

namespace nativesharp::llvm_backend
{
CacheEntryLock::~CacheEntryLock()
{
#if defined(_WIN32)
    if (handle_ != INVALID_HANDLE_VALUE) ::CloseHandle(handle_);
    std::error_code ignored;
    if (!path_.empty()) std::filesystem::remove(path_, ignored);
#endif
}

bool CacheEntryLock::acquire(const std::filesystem::path& path, std::string& error)
{
#if defined(_WIN32)
    path_ = path;
    constexpr unsigned attempts = 3000;
    for (unsigned attempt = 0; attempt < attempts; ++attempt)
    {
        handle_ = ::CreateFileW(extendedWindowsPath(path).c_str(),
            GENERIC_READ | GENERIC_WRITE, 0,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (handle_ != INVALID_HANDLE_VALUE) return true;
        const unsigned long failure = ::GetLastError();
        if (failure != ERROR_SHARING_VIOLATION && failure != ERROR_LOCK_VIOLATION)
        {
            error = "cannot lock artifact-cache entry '" + path.string() +
                "' (Win32 error " + std::to_string(failure) + ")";
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    error = "timed out waiting for artifact-cache entry '" + path.string() + "'";
    return false;
#else
    lock_ = std::unique_lock(globalMutex_);
    (void)path;
    (void)error;
    return true;
#endif
}
}
