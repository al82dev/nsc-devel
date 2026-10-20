#pragma once

#include <filesystem>
#include <mutex>
#include <string>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace nativesharp::llvm_backend
{
class CacheEntryLock final
{
public:
    CacheEntryLock() = default;
    CacheEntryLock(const CacheEntryLock&) = delete;
    CacheEntryLock& operator=(const CacheEntryLock&) = delete;

    ~CacheEntryLock();

    bool acquire(const std::filesystem::path& path, std::string& error);

private:
#if defined(_WIN32)
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::filesystem::path path_;
#else
    inline static std::mutex globalMutex_;
    std::unique_lock<std::mutex> lock_;
#endif
};
}
