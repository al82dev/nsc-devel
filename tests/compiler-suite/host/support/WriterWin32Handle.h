#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace nsc::suite::writer
{
class Win32Handle
{
public:
    // Takes exclusive ownership of one Win32 handle; INVALID_HANDLE_VALUE represents no resource.
    explicit Win32Handle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept : handle_(handle) {}
    Win32Handle(const Win32Handle&) = delete;
    Win32Handle& operator=(const Win32Handle&) = delete;

    // Closes the owned handle exactly once. Cleanup deliberately ignores errors during stack unwinding.
    ~Win32Handle()
    {
        close();
    }

    // Returns a borrowed handle without transferring ownership.
    [[nodiscard]] HANDLE get() const noexcept
    {
        return handle_;
    }

    // Releases the owned handle and makes subsequent cleanup idempotent.
    void close() noexcept
    {
        if (handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr)
        {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE handle_;
};
}
