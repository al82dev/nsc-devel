#pragma once

#include "WriterWin32Handle.h"

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite::writer
{
class ScopedWriteFailure
{
public:
    // Replaces only this executable's WriteFile IAT slot and records the selected stage basename.
    explicit ScopedWriteFailure(std::wstring basename)
        : slot_(findWriteFileImportSlot())
    {
        if (slot_ == nullptr || originalWriteFile_ != nullptr)
        {
            throw std::runtime_error("cannot locate unique transactional-writer WriteFile import");
        }
        DWORD previousProtection = 0;
        if (!VirtualProtect(
                static_cast<PVOID>(slot_), sizeof(*slot_), PAGE_READWRITE, &previousProtection))
        {
            throw std::runtime_error("cannot unlock transactional-writer WriteFile import");
        }
        rejectedBasename_ = std::move(basename);
        InterlockedExchange(&observed_, 0);
        originalWriteFile_ = reinterpret_cast<WriteFileFunction>(*slot_);
        *slot_ = reinterpret_cast<PVOID>(&rejectSelectedWrite);
        DWORD ignored = 0;
        VirtualProtect(static_cast<PVOID>(slot_), sizeof(*slot_), previousProtection, &ignored);
    }

    ScopedWriteFailure(const ScopedWriteFailure&) = delete;
    ScopedWriteFailure& operator=(const ScopedWriteFailure&) = delete;

    // Restores the original import before any later assertion or result output can execute.
    ~ScopedWriteFailure()
    {
        static_cast<void>(restore());
    }

    // Restores the import explicitly and reports protection failures without unsafe global state.
    [[nodiscard]] bool restore() noexcept
    {
        rejectedBasename_.clear();
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
        *slot_ = reinterpret_cast<PVOID>(originalWriteFile_);
        DWORD ignored = 0;
        const bool protectedAgain = VirtualProtect(
            static_cast<PVOID>(slot_), sizeof(*slot_), previousProtection, &ignored) != 0;
        slot_ = nullptr;
        originalWriteFile_ = nullptr;
        return protectedAgain;
    }

    // Reports whether the selected handle reached the process-local interception point.
    [[nodiscard]] bool observed() const noexcept
    {
        return InterlockedCompareExchange(&observed_, 0, 0) != 0;
    }

private:
    using WriteFileFunction = BOOL(WINAPI*)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);

    // Returns a typed pointer at one trusted PE relative virtual address without allocation or mutation.
    template<typename T>
    static T* imagePointer(std::byte* image, DWORD relativeAddress) noexcept
    {
        return reinterpret_cast<T*>(image + relativeAddress);
    }

    // Locates this executable's WriteFile import slot in O(imported symbols) time and O(1) memory.
    static PVOID* findWriteFileImportSlot()
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
            PVOID* slot = findWriteFileInDescriptor(image, *descriptor);
            if (slot != nullptr)
            {
                return slot;
            }
        }
        return nullptr;
    }

    // Searches one PE import descriptor without altering loader-owned entries.
    static PVOID* findWriteFileInDescriptor(
        std::byte* image,
        const IMAGE_IMPORT_DESCRIPTOR& descriptor)
    {
        if (descriptor.OriginalFirstThunk == 0)
        {
            return nullptr;
        }
        auto* names = imagePointer<IMAGE_THUNK_DATA>(image, descriptor.OriginalFirstThunk);
        auto* addresses = imagePointer<IMAGE_THUNK_DATA>(image, descriptor.FirstThunk);
        for (; names->u1.AddressOfData != 0; ++names, ++addresses)
        {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
            {
                continue;
            }
            const auto* import = imagePointer<IMAGE_IMPORT_BY_NAME>(
                image, static_cast<DWORD>(names->u1.AddressOfData));
            if (std::strcmp(reinterpret_cast<const char*>(import->Name), "WriteFile") == 0)
            {
                return reinterpret_cast<PVOID*>(&addresses->u1.Function);
            }
        }
        return nullptr;
    }

    // Rejects only the selected stage basename and delegates every other process write unchanged.
    static BOOL WINAPI rejectSelectedWrite(
        HANDLE file,
        LPCVOID buffer,
        DWORD byteCount,
        LPDWORD written,
        LPOVERLAPPED overlapped)
    {
        std::vector<wchar_t> path(32768U);
        const DWORD length = GetFinalPathNameByHandleW(
            file, path.data(), static_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED);
        if (length != 0 && length < path.size() &&
            std::filesystem::path(path.data()).filename().native() == rejectedBasename_)
        {
            InterlockedExchange(&observed_, 1);
            if (written != nullptr)
            {
                *written = 0;
            }
            SetLastError(ERROR_WRITE_FAULT);
            return FALSE;
        }
        return originalWriteFile_(file, buffer, byteCount, written, overlapped);
    }

    static inline WriteFileFunction originalWriteFile_ = nullptr;
    static inline std::wstring rejectedBasename_;
    static inline volatile LONG observed_ = 0;
    PVOID* slot_ = nullptr;
};
}
