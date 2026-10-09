#pragma once

#include "WriterWin32Handle.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite::writer
{
class ScopedSeedFailure
{
public:
    // Replaces one selected import in this executable and rejects only the named fixture path.
    ScopedSeedFailure(
        std::string_view importName,
        std::wstring basename,
        DWORD errorCode)
    {
        if (active_)
        {
            throw std::runtime_error("nested artifact-seed fault injection is not supported");
        }
        rejectedBasename_ = std::move(basename);
        errorCode_ = errorCode;
        PVOID replacement = nullptr;
        if (importName == "GetFileAttributesW")
        {
            replacement = reinterpret_cast<PVOID>(&rejectSelectedInspection);
        }
        else if (importName == "FlushFileBuffers")
        {
            replacement = reinterpret_cast<PVOID>(&rejectSelectedFlush);
        }
        else
        {
            throw std::runtime_error("unsupported artifact-seed import fault");
        }
        slot_ = findImportSlot(importName);
        if (slot_ == nullptr)
        {
            throw std::runtime_error("cannot locate artifact-seed import");
        }
        DWORD previousProtection = 0;
        if (!VirtualProtect(
                static_cast<PVOID>(slot_), sizeof(*slot_), PAGE_READWRITE, &previousProtection))
        {
            throw std::runtime_error("cannot unlock artifact-seed import");
        }
        original_ = *slot_;
        InterlockedExchange(&observed_, 0);
        active_ = true;
        *slot_ = replacement;
        DWORD ignored = 0;
        if (!VirtualProtect(
                static_cast<PVOID>(slot_), sizeof(*slot_), previousProtection, &ignored))
        {
            *slot_ = original_;
            active_ = false;
            original_ = nullptr;
            slot_ = nullptr;
            throw std::runtime_error("cannot protect artifact-seed import");
        }
    }

    ScopedSeedFailure(const ScopedSeedFailure&) = delete;
    ScopedSeedFailure& operator=(const ScopedSeedFailure&) = delete;

    // Restores the original import before assertions or result output can execute.
    ~ScopedSeedFailure()
    {
        static_cast<void>(restore());
    }

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
        *slot_ = original_;
        DWORD ignored = 0;
        const bool protectedAgain = VirtualProtect(
            static_cast<PVOID>(slot_), sizeof(*slot_), previousProtection, &ignored) != 0;
        active_ = false;
        original_ = nullptr;
        slot_ = nullptr;
        rejectedBasename_.clear();
        return protectedAgain;
    }

    [[nodiscard]] bool observed() const noexcept
    {
        return InterlockedCompareExchange(&observed_, 0, 0) != 0;
    }

private:
    using GetFileAttributesFunction = DWORD(WINAPI*)(LPCWSTR);
    using FlushFileBuffersFunction = BOOL(WINAPI*)(HANDLE);

    template<typename T>
    static T* imagePointer(std::byte* image, DWORD relativeAddress) noexcept
    {
        return reinterpret_cast<T*>(image + relativeAddress);
    }

    // Finds one named IAT slot in O(imported symbols) time and O(1) memory.
    static PVOID* findImportSlot(std::string_view importName)
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
                const std::string_view name(reinterpret_cast<const char*>(import->Name));
                if (name == importName)
                {
                    return reinterpret_cast<PVOID*>(&addresses->u1.Function);
                }
            }
        }
        return nullptr;
    }

    // Compares only the final path component without allocating inside a Win32 callback.
    static bool rejectsPath(LPCWSTR path) noexcept
    {
        if (path == nullptr)
        {
            return false;
        }
        const wchar_t* basename = path;
        for (const wchar_t* cursor = path; *cursor != L'\0'; ++cursor)
        {
            if (*cursor == L'\\' || *cursor == L'/')
            {
                basename = cursor + 1;
            }
        }
        return rejectedBasename_ == basename;
    }

    static DWORD WINAPI rejectSelectedInspection(LPCWSTR path)
    {
        if (rejectsPath(path))
        {
            InterlockedExchange(&observed_, 1);
            SetLastError(errorCode_);
            return INVALID_FILE_ATTRIBUTES;
        }
        const auto original = reinterpret_cast<GetFileAttributesFunction>(original_);
        return original(path);
    }

    static BOOL WINAPI rejectSelectedFlush(HANDLE file)
    {
        thread_local std::array<wchar_t, 32768> path{};
        const DWORD length = GetFinalPathNameByHandleW(
            file, path.data(), static_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED);
        if (length != 0 && length < path.size() && rejectsPath(path.data()))
        {
            InterlockedExchange(&observed_, 1);
            SetLastError(errorCode_);
            return FALSE;
        }
        const auto original = reinterpret_cast<FlushFileBuffersFunction>(original_);
        return original(file);
    }

    static inline PVOID original_ = nullptr;
    static inline std::wstring rejectedBasename_;
    static inline DWORD errorCode_ = ERROR_SUCCESS;
    static inline volatile LONG observed_ = 0;
    static inline bool active_ = false;
    PVOID* slot_ = nullptr;
};
}
