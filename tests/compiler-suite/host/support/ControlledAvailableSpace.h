#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace nsc::suite::preflight
{
class ScopedAvailableSpaceLimit
{
public:
    // Replaces this executable's disk-space query only for one exact fixture directory.
    ScopedAvailableSpaceLimit(std::wstring directory, unsigned long long availableBytes)
        : slot_(findImportSlot())
    {
        if (slot_ == nullptr || active_)
        {
            throw std::runtime_error("cannot locate unique preflight disk-space import");
        }
        directory_ = std::move(directory);
        availableBytes_ = availableBytes;
        DWORD previousProtection = 0;
        if (!VirtualProtect(
                static_cast<PVOID>(slot_), sizeof(*slot_), PAGE_READWRITE, &previousProtection))
        {
            throw std::runtime_error("cannot unlock preflight disk-space import");
        }
        original_ = reinterpret_cast<GetDiskFreeSpaceFunction>(*slot_);
        InterlockedExchange(&observed_, 0);
        active_ = true;
        *slot_ = reinterpret_cast<PVOID>(&reportSelectedAvailableSpace);
        DWORD ignored = 0;
        if (!VirtualProtect(
                static_cast<PVOID>(slot_), sizeof(*slot_), previousProtection, &ignored))
        {
            *slot_ = reinterpret_cast<PVOID>(original_);
            active_ = false;
            original_ = nullptr;
            slot_ = nullptr;
            throw std::runtime_error("cannot protect preflight disk-space import");
        }
    }

    ScopedAvailableSpaceLimit(const ScopedAvailableSpaceLimit&) = delete;
    ScopedAvailableSpaceLimit& operator=(const ScopedAvailableSpaceLimit&) = delete;

    ~ScopedAvailableSpaceLimit()
    {
        static_cast<void>(restore());
    }

    // Restores the original import before assertions or result output can execute.
    [[nodiscard]] bool restore() noexcept
    {
        if (slot_ == nullptr)
        {
            return true;
        }
        DWORD previousProtection = 0;
        if (!VirtualProtect(
                static_cast<PVOID>(slot_), sizeof(*slot_), PAGE_READWRITE, &previousProtection))
        {
            return false;
        }
        *slot_ = reinterpret_cast<PVOID>(original_);
        DWORD ignored = 0;
        const bool protectedAgain = VirtualProtect(
            static_cast<PVOID>(slot_), sizeof(*slot_), previousProtection, &ignored) != 0;
        active_ = false;
        original_ = nullptr;
        slot_ = nullptr;
        directory_.clear();
        return protectedAgain;
    }

    [[nodiscard]] bool observed() const noexcept
    {
        return InterlockedCompareExchange(&observed_, 0, 0) != 0;
    }

private:
    using GetDiskFreeSpaceFunction = BOOL(WINAPI*)(
        LPCWSTR,
        PULARGE_INTEGER,
        PULARGE_INTEGER,
        PULARGE_INTEGER);

    template<typename T>
    static T* imagePointer(std::byte* image, DWORD relativeAddress) noexcept
    {
        return reinterpret_cast<T*>(image + relativeAddress);
    }

    // Finds GetDiskFreeSpaceExW in O(imported symbols) time and O(1) memory.
    static PVOID* findImportSlot()
    {
        auto* image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if (dos == nullptr || dos->e_magic != IMAGE_DOS_SIGNATURE)
        {
            return nullptr;
        }
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
        {
            return nullptr;
        }
        const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        auto* descriptor = imagePointer<IMAGE_IMPORT_DESCRIPTOR>(image, directory.VirtualAddress);
        for (; directory.VirtualAddress != 0 && descriptor->Name != 0; ++descriptor)
        {
            if (descriptor->OriginalFirstThunk == 0)
            {
                continue;
            }
            auto* names = imagePointer<IMAGE_THUNK_DATA>(image, descriptor->OriginalFirstThunk);
            auto* addresses = imagePointer<IMAGE_THUNK_DATA>(image, descriptor->FirstThunk);
            for (; names->u1.AddressOfData != 0; ++names, ++addresses)
            {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                {
                    continue;
                }
                const auto* import = imagePointer<IMAGE_IMPORT_BY_NAME>(
                    image, static_cast<DWORD>(names->u1.AddressOfData));
                if (std::strcmp(
                        reinterpret_cast<const char*>(import->Name),
                        "GetDiskFreeSpaceExW") == 0)
                {
                    return reinterpret_cast<PVOID*>(&addresses->u1.Function);
                }
            }
        }
        return nullptr;
    }

    static BOOL WINAPI reportSelectedAvailableSpace(
        LPCWSTR directory,
        PULARGE_INTEGER available,
        PULARGE_INTEGER total,
        PULARGE_INTEGER totalFree)
    {
        if (directory != nullptr && _wcsicmp(directory, directory_.c_str()) == 0)
        {
            InterlockedExchange(&observed_, 1);
            if (available != nullptr)
            {
                available->QuadPart = availableBytes_;
            }
            if (total != nullptr)
            {
                total->QuadPart = availableBytes_;
            }
            if (totalFree != nullptr)
            {
                totalFree->QuadPart = availableBytes_;
            }
            SetLastError(ERROR_SUCCESS);
            return TRUE;
        }
        return original_(directory, available, total, totalFree);
    }

    static inline GetDiskFreeSpaceFunction original_ = nullptr;
    static inline std::wstring directory_;
    static inline unsigned long long availableBytes_ = 0;
    static inline volatile LONG observed_ = 0;
    static inline bool active_ = false;
    PVOID* slot_ = nullptr;
};
}
