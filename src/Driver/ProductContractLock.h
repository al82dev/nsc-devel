#pragma once
#define NOMINMAX
#include <Windows.h>
#include <filesystem>
#include <string>
#include <vector>

// Cooperative lock only. It does not authenticate a producer, protect
// against nonparticipants, or implement crash recovery.
namespace contract_lock_prototype {
namespace fs = std::filesystem;

inline bool finalPath(HANDLE handle, fs::path& result) {
    const DWORD needed = GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED);
    if (!needed) return false;
    std::vector<wchar_t> buffer(static_cast<std::size_t>(needed) + 1);
    const DWORD written = GetFinalPathNameByHandleW(handle, buffer.data(),
        static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED);
    if (!written || written >= buffer.size()) return false;
    result = fs::path(std::wstring(buffer.data(), written)); return true;
}

inline bool canonicalProduct(const fs::path& product, fs::path& canonical) {
    std::error_code error;
    fs::path absolute = fs::absolute(product, error).lexically_normal();
    if (error || absolute.filename().empty() || absolute.filename().native().find(L':') != std::wstring::npos)
        return false;
    HANDLE parent = CreateFileW(absolute.parent_path().c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (parent == INVALID_HANDLE_VALUE) return false;
    fs::path realParent; bool ok = finalPath(parent, realParent); CloseHandle(parent);
    if (!ok) return false;
    const DWORD attributes = GetFileAttributesW(absolute.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const DWORD code = GetLastError();
        if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND) return false;
        canonical = realParent / absolute.filename(); return true;
    }
    if ((attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) return false;
    HANDLE file = CreateFileW(absolute.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    ok = finalPath(file, canonical); CloseHandle(file); return ok;
}

class ProductLock final {
public:
    ProductLock() = default;
    ProductLock(const ProductLock&) = delete;
    ProductLock& operator=(const ProductLock&) = delete;
    ~ProductLock() { release(); }

    bool acquire(const fs::path& product, bool wait) {
        if (handle_ != INVALID_HANDLE_VALUE) return false;
        fs::path before;
        if (!canonicalProduct(product, before)) return false;
        fs::path lockPath = before; lockPath += L".nsreceiver.lock";
        HANDLE h = CreateFileW(lockPath.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        OVERLAPPED position{};
        DWORD flags = LOCKFILE_EXCLUSIVE_LOCK | (wait ? 0 : LOCKFILE_FAIL_IMMEDIATELY);
        if (!LockFileEx(h, flags, 0, 1, 0, &position)) { CloseHandle(h); return false; }
        fs::path after;
        if (!canonicalProduct(product, after) ||
            CompareStringOrdinal(before.c_str(), -1, after.c_str(), -1, TRUE) != CSTR_EQUAL) {
            UnlockFileEx(h, 0, 1, 0, &position); CloseHandle(h); return false;
        }
        handle_ = h; return true;
    }
    // Readers never create lock files (notably in installed SDK directories).
    // They do not wait: failure leaves the contract Unknown and avoids cycles
    // with a producer that already holds its output lock.
    bool acquireReader(const fs::path& product) {
        if (handle_ != INVALID_HANDLE_VALUE) return false;
        fs::path before;
        if (!canonicalProduct(product, before)) return false;
        fs::path lockPath = before; lockPath += L".nsreceiver.lock";
        HANDLE h = CreateFileW(lockPath.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;
        OVERLAPPED position{};
        if (!LockFileEx(h, LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &position)) { CloseHandle(h); return false; }
        fs::path after;
        if (!canonicalProduct(product, after) ||
            CompareStringOrdinal(before.c_str(), -1, after.c_str(), -1, TRUE) != CSTR_EQUAL) {
            UnlockFileEx(h, 0, 1, 0, &position); CloseHandle(h); return false;
        }
        handle_ = h; return true;
    }
    void release() {
        if (handle_ == INVALID_HANDLE_VALUE) return;
        OVERLAPPED position{}; UnlockFileEx(handle_, 0, 1, 0, &position);
        CloseHandle(handle_); handle_ = INVALID_HANDLE_VALUE;
    }
private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

// Pin the selected namespace and bytes against writers which do not use the
// cooperative product lock. Existing writers cause acquisition to fail; no
// files or directories are created. Every ancestor below the volume root is
// kept open without delete sharing so its path cannot be renamed underneath a
// later hash or linker open. Reparse points are deliberately not trusted.
class ProductArtifactGuard final {
public:
    ProductArtifactGuard() = default;
    ProductArtifactGuard(const ProductArtifactGuard&) = delete;
    ProductArtifactGuard& operator=(const ProductArtifactGuard&) = delete;
    ~ProductArtifactGuard() { release(); }

    bool acquire(const std::vector<fs::path>& paths) {
        if (!handles_.empty() || paths.empty()) return false;
        for (const fs::path& path : paths) {
            std::error_code error;
            const fs::path absolute = fs::absolute(path, error).lexically_normal();
            if (error || absolute.empty() || absolute.filename().empty()) { release(); return false; }
            std::vector<fs::path> ancestors;
            for (fs::path p = absolute.parent_path(); !p.empty() && p != p.root_path(); p = p.parent_path())
                ancestors.push_back(p);
            for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
                HANDLE h = CreateFileW(it->c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
                if (h == INVALID_HANDLE_VALUE) { release(); return false; }
                BY_HANDLE_FILE_INFORMATION info{};
                if (!GetFileInformationByHandle(h, &info) || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                    (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                    CloseHandle(h); release(); return false;
                }
                handles_.push_back(h);
            }
            HANDLE h = CreateFileW(absolute.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (h == INVALID_HANDLE_VALUE) { release(); return false; }
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(h, &info) ||
                (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
                CloseHandle(h); release(); return false;
            }
            handles_.push_back(h);
        }
        return true;
    }
    void release() {
        for (auto it = handles_.rbegin(); it != handles_.rend(); ++it) CloseHandle(*it);
        handles_.clear();
    }
private:
    std::vector<HANDLE> handles_;
};
}
