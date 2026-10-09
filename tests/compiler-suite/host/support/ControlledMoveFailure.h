#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string_view>

namespace nsc::suite::artifact
{
class ScopedMoveFailure
{
public:
    // Replaces this executable's MoveFileExW import and rejects selected call ordinals.
    explicit ScopedMoveFailure(std::initializer_list<unsigned> failedCalls)
        : slot_(findImportSlot())
    {
        if (slot_ == nullptr || active_ || failedCalls.size() > failedCalls_.size())
        {
            throw std::runtime_error("cannot initialize artifact move fault");
        }
        failedCallCount_ = failedCalls.size();
        std::size_t index = 0;
        for (const unsigned call : failedCalls)
        {
            failedCalls_[index++] = call;
        }
        InterlockedExchange(&callCount_, 0);
        InterlockedExchange(&observedFailures_, 0);
        for (auto& record : records_)
        {
            record.source.fill(L'\0');
            record.destination.fill(L'\0');
        }
        DWORD previousProtection = 0;
        if (!VirtualProtect(
                static_cast<PVOID>(slot_), sizeof(*slot_), PAGE_READWRITE, &previousProtection))
        {
            throw std::runtime_error("cannot unlock artifact move import");
        }
        original_ = reinterpret_cast<MoveFileExFunction>(*slot_);
        active_ = true;
        *slot_ = reinterpret_cast<PVOID>(&moveWithSelectedFailures);
        DWORD ignored = 0;
        if (!VirtualProtect(
                static_cast<PVOID>(slot_), sizeof(*slot_), previousProtection, &ignored))
        {
            *slot_ = reinterpret_cast<PVOID>(original_);
            active_ = false;
            original_ = nullptr;
            slot_ = nullptr;
            throw std::runtime_error("cannot protect artifact move import");
        }
    }

    ScopedMoveFailure(const ScopedMoveFailure&) = delete;
    ScopedMoveFailure& operator=(const ScopedMoveFailure&) = delete;

    ~ScopedMoveFailure()
    {
        static_cast<void>(restore());
    }

    // Restores the product import before the test reads filesystem state or emits results.
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
        return protectedAgain;
    }

    [[nodiscard]] unsigned callCount() const noexcept
    {
        return static_cast<unsigned>(InterlockedCompareExchange(&callCount_, 0, 0));
    }

    [[nodiscard]] unsigned observedFailureCount() const noexcept
    {
        return static_cast<unsigned>(InterlockedCompareExchange(&observedFailures_, 0, 0));
    }

    // Returns the recorded final source component for one one-based call ordinal.
    [[nodiscard]] std::wstring_view sourceBasename(unsigned call) const noexcept
    {
        if (call == 0 || call > records_.size())
        {
            return {};
        }
        return records_[call - 1U].source.data();
    }

    // Returns the recorded final destination component for one one-based call ordinal.
    [[nodiscard]] std::wstring_view destinationBasename(unsigned call) const noexcept
    {
        if (call == 0 || call > records_.size())
        {
            return {};
        }
        return records_[call - 1U].destination.data();
    }

private:
    using MoveFileExFunction = BOOL(WINAPI*)(LPCWSTR, LPCWSTR, DWORD);

    struct MoveRecord
    {
        std::array<wchar_t, 512> source;
        std::array<wchar_t, 512> destination;
    };

    template<typename T>
    static T* imagePointer(std::byte* image, DWORD relativeAddress) noexcept
    {
        return reinterpret_cast<T*>(image + relativeAddress);
    }

    // Finds MoveFileExW in O(imported symbols) time and O(1) memory.
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
                        "MoveFileExW") == 0)
                {
                    return reinterpret_cast<PVOID*>(&addresses->u1.Function);
                }
            }
        }
        return nullptr;
    }

    static const wchar_t* basename(LPCWSTR path) noexcept
    {
        if (path == nullptr)
        {
            return L"";
        }
        const wchar_t* result = path;
        for (const wchar_t* cursor = path; *cursor != L'\0'; ++cursor)
        {
            if (*cursor == L'\\' || *cursor == L'/')
            {
                result = cursor + 1;
            }
        }
        return result;
    }

    static void record(unsigned call, LPCWSTR source, LPCWSTR destination) noexcept
    {
        if (call == 0 || call > records_.size())
        {
            return;
        }
        MoveRecord& record = records_[call - 1U];
        wcsncpy_s(record.source.data(), record.source.size(), basename(source), _TRUNCATE);
        wcsncpy_s(
            record.destination.data(), record.destination.size(), basename(destination), _TRUNCATE);
    }

    static bool mustFail(unsigned call) noexcept
    {
        for (std::size_t index = 0; index < failedCallCount_; ++index)
        {
            if (failedCalls_[index] == call)
            {
                return true;
            }
        }
        return false;
    }

    static BOOL WINAPI moveWithSelectedFailures(
        LPCWSTR source,
        LPCWSTR destination,
        DWORD flags)
    {
        const unsigned call = static_cast<unsigned>(InterlockedIncrement(&callCount_));
        record(call, source, destination);
        if (mustFail(call))
        {
            InterlockedIncrement(&observedFailures_);
            SetLastError(ERROR_SHARING_VIOLATION);
            return FALSE;
        }
        return original_(source, destination, flags);
    }

    static inline MoveFileExFunction original_ = nullptr;
    static inline std::array<unsigned, 8> failedCalls_{};
    static inline std::size_t failedCallCount_ = 0;
    static inline std::array<MoveRecord, 16> records_{};
    static inline volatile LONG callCount_ = 0;
    static inline volatile LONG observedFailures_ = 0;
    static inline bool active_ = false;
    PVOID* slot_ = nullptr;
};
}
