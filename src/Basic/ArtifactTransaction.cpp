#include "nativesharp/Basic/ArtifactTransaction.h"

#include <atomic>
#include <fstream>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <process.h>
#endif

namespace nativesharp::basic
{
namespace
{
std::atomic<unsigned long long> transactionOrdinal = 0;

#if defined(_WIN32)
std::filesystem::path extendedPath(const std::filesystem::path& input)
{
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(input, error);
    if (error) absolute = input;
    const std::wstring value = absolute.native();
    if (value.starts_with(L"\\\\?\\")) return absolute;
    if (value.starts_with(L"\\\\")) return std::filesystem::path(L"\\\\?\\UNC\\" + value.substr(2));
    return std::filesystem::path(L"\\\\?\\" + value);
}

bool failWin32(std::string_view operation, const std::filesystem::path& path,
    unsigned long code, std::string& error)
{
    const OutputFailureKind kind = classifyWindowsOutputError(code);
    error = std::string(operation) + " failed for '" + path.string() + "': " +
        std::string(outputFailureKindName(kind)) + " (Win32 error " + std::to_string(code) + ")";
    return false;
}

bool ensureDirectory(const std::filesystem::path& directory, std::string& error)
{
    if (directory.empty()) return true;
    const std::filesystem::path extended = extendedPath(directory);
    std::error_code filesystemError;
    std::filesystem::create_directories(extended, filesystemError);
    if (!filesystemError) return true;
    filesystemError.clear();
    if (std::filesystem::is_directory(extended, filesystemError) && !filesystemError) return true;
    const unsigned long code = filesystemError ? static_cast<unsigned long>(filesystemError.value()) : ::GetLastError();
    return failWin32("output directory creation", directory, code, error);
}

bool copyToSibling(const std::filesystem::path& source, const std::filesystem::path& destination,
    std::string& error)
{
    if (::CopyFileW(extendedPath(source).c_str(), extendedPath(destination).c_str(), FALSE) == 0)
        return failWin32("artifact staging copy", destination, ::GetLastError(), error);
    HANDLE handle = ::CreateFileW(extendedPath(destination).c_str(), GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return failWin32("artifact staging flush-open", destination, ::GetLastError(), error);
    const BOOL flushed = ::FlushFileBuffers(handle);
    const unsigned long flushError = flushed != 0 ? ERROR_SUCCESS : ::GetLastError();
    ::CloseHandle(handle);
    if (flushed == 0) return failWin32("artifact staging flush", destination, flushError, error);
    return true;
}

bool moveReplace(const std::filesystem::path& source, const std::filesystem::path& destination,
    std::string& error)
{
    if (::MoveFileExW(extendedPath(source).c_str(), extendedPath(destination).c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
        return true;
    return failWin32("artifact commit", destination, ::GetLastError(), error);
}

bool exists(const std::filesystem::path& path, bool& value, std::string& error)
{
    const DWORD attributes = ::GetFileAttributesW(extendedPath(path).c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES)
    {
        value = true;
        return true;
    }
    const DWORD code = ::GetLastError();
    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
    {
        value = false;
        return true;
    }
    return failWin32("artifact inspection", path, code, error);
}

void removeNoThrow(const std::filesystem::path& path) noexcept
{
    if (path.empty()) return;
    ::DeleteFileW(extendedPath(path).c_str());
}
#else
bool ensureDirectory(const std::filesystem::path& directory, std::string& error)
{
    std::error_code filesystemError;
    std::filesystem::create_directories(directory, filesystemError);
    if (!filesystemError) return true;
    error = "output directory creation failed for '" + directory.string() + "': " + filesystemError.message();
    return false;
}
void removeNoThrow(const std::filesystem::path& path) noexcept
{
    if (path.empty()) return;
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
#endif
}

std::string_view outputFailureKindName(OutputFailureKind kind) noexcept
{
    switch (kind)
    {
    case OutputFailureKind::None: return "none";
    case OutputFailureKind::PathLimit: return "path-limit";
    case OutputFailureKind::MissingComponent: return "missing-component";
    case OutputFailureKind::PermissionDenied: return "permission-denied";
    case OutputFailureKind::FileLocked: return "file-locked";
    case OutputFailureKind::InsufficientSpace: return "insufficient-space";
    case OutputFailureKind::IoError: return "io-error";
    }
    return "io-error";
}

#if defined(_WIN32)
OutputFailureKind classifyWindowsOutputError(unsigned long code) noexcept
{
    switch (code)
    {
    case ERROR_SUCCESS: return OutputFailureKind::None;
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_BUFFER_OVERFLOW: return OutputFailureKind::PathLimit;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_DIRECTORY: return OutputFailureKind::MissingComponent;
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
    case ERROR_WRITE_PROTECT: return OutputFailureKind::PermissionDenied;
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    case ERROR_USER_MAPPED_FILE: return OutputFailureKind::FileLocked;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL: return OutputFailureKind::InsufficientSpace;
    default: return OutputFailureKind::IoError;
    }
}
#endif

ArtifactTransaction::ArtifactTransaction() = default;

ArtifactTransaction::~ArtifactTransaction()
{
    std::error_code ignored;
    if (!stagingRoot_.empty()) std::filesystem::remove_all(stagingRoot_, ignored);
}

bool ArtifactTransaction::ensureStaging(std::string& error)
{
    if (!stagingRoot_.empty()) return true;
    std::error_code filesystemError;
    const std::filesystem::path root = std::filesystem::temp_directory_path(filesystemError);
    if (filesystemError)
    {
        error = "cannot resolve NativeSharp artifact staging directory: " + filesystemError.message();
        return false;
    }
#if defined(_WIN32)
    const unsigned long process = static_cast<unsigned long>(_getpid());
#else
    const unsigned long process = 0;
#endif
    for (unsigned attempt = 0; attempt != 100; ++attempt)
    {
        stagingRoot_ = root / ("nsc-artifacts-" + std::to_string(process) + "-" +
            std::to_string(transactionOrdinal.fetch_add(1, std::memory_order_relaxed)) + "-" +
            std::to_string(attempt));
        filesystemError.clear();
        if (std::filesystem::create_directory(stagingRoot_, filesystemError)) return true;
        if (filesystemError != std::errc::file_exists) break;
    }
    error = "cannot create NativeSharp artifact staging directory: " + filesystemError.message();
    stagingRoot_.clear();
    return false;
}

bool ArtifactTransaction::add(
    const std::filesystem::path& destination, std::filesystem::path& stage, std::string& error)
{
    error.clear();
    if (destination.empty())
    {
        error = "artifact transaction destination is empty";
        return false;
    }
    if (!ensureStaging(error)) return false;
    // Linkers persist the output basename inside PE artifacts: DLL import libraries name their
    // runtime and CodeView RSDS records name their PDB. Keep isolation in a per-entry directory
    // instead of prefixing the filename, otherwise a later rename publishes internally stale
    // names such as `0-Product.dll` and `0-Product.pdb`.
    stage = stagingRoot_ / std::to_string(entries_.size()) / destination.filename();
    if (!ensureDirectory(stage.parent_path(), error)) return false;
    entries_.push_back(Entry{destination, stage});
    return true;
}

bool ArtifactTransaction::addStaged(const std::filesystem::path& destination,
    const std::filesystem::path& stage, std::string& error)
{
    error.clear();
    if (destination.empty() || stage.empty())
    {
        error = "artifact transaction destination or stage is empty";
        return false;
    }
    entries_.push_back(Entry{destination, stage});
    return true;
}

bool ArtifactTransaction::commit(std::string& error)
{
    error.clear();
    if (committed_)
    {
        error = "artifact transaction was already committed";
        return false;
    }
    if (entries_.empty())
    {
        committed_ = true;
        return true;
    }

    struct State final
    {
        std::filesystem::path sibling;
        std::filesystem::path backup;
        bool hadDestination = false;
        bool backedUp = false;
        bool installed = false;
    };
    std::vector<State> states(entries_.size());
    const std::string suffix = ".nsc-transaction-" +
        std::to_string(transactionOrdinal.fetch_add(1, std::memory_order_relaxed));
    const auto cleanupPrepared = [&states] {
        for (const State& state : states)
        {
            removeNoThrow(state.sibling);
            removeNoThrow(state.backup);
        }
    };

    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        const Entry& entry = entries_[index];
        std::error_code filesystemError;
        if (!std::filesystem::is_regular_file(entry.stage, filesystemError) || filesystemError)
        {
            error = "artifact transaction stage is missing: '" + entry.stage.string() + "'";
            cleanupPrepared();
            return false;
        }
        if (!ensureDirectory(entry.destination.parent_path(), error))
        {
            cleanupPrepared();
            return false;
        }
        states[index].sibling = entry.destination.string() + suffix + ".new";
        states[index].backup = entry.destination.string() + suffix + ".old";
#if defined(_WIN32)
        if (entry.destination.native().size() >= 32767U)
        {
            error = "artifact preflight failed for '" + entry.destination.string() +
                "': path-limit (Windows extended path exceeds 32767 UTF-16 code units)";
            cleanupPrepared();
            return false;
        }
        ULARGE_INTEGER available{};
        if (::GetDiskFreeSpaceExW(extendedPath(entry.destination.parent_path()).c_str(),
                &available, nullptr, nullptr) == 0)
        {
            failWin32("artifact free-space preflight", entry.destination, ::GetLastError(), error);
            cleanupPrepared();
            return false;
        }
        const auto required = std::filesystem::file_size(entry.stage, filesystemError);
        if (filesystemError)
        {
            error = "cannot measure staged artifact '" + entry.stage.string() + "': " + filesystemError.message();
            cleanupPrepared();
            return false;
        }
        if (available.QuadPart < required)
        {
            error = "artifact preflight failed for '" + entry.destination.string() + "': insufficient-space";
            cleanupPrepared();
            return false;
        }
        removeNoThrow(states[index].sibling);
        removeNoThrow(states[index].backup);
        if (!copyToSibling(entry.stage, states[index].sibling, error))
        {
            cleanupPrepared();
            return false;
        }
#else
        std::filesystem::copy_file(entry.stage, states[index].sibling,
            std::filesystem::copy_options::overwrite_existing, filesystemError);
        if (filesystemError)
        {
            error = "artifact staging copy failed for '" + entry.destination.string() + "': " + filesystemError.message();
            cleanupPrepared();
            return false;
        }
#endif
    }

    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        const Entry& entry = entries_[index];
        State& state = states[index];
#if defined(_WIN32)
        if (!exists(entry.destination, state.hadDestination, error)) goto rollback;
        if (state.hadDestination)
        {
            if (!moveReplace(entry.destination, state.backup, error)) goto rollback;
            state.backedUp = true;
        }
        if (!moveReplace(state.sibling, entry.destination, error)) goto rollback;
#else
        std::error_code filesystemError;
        state.hadDestination = std::filesystem::exists(entry.destination, filesystemError);
        if (state.hadDestination)
        {
            std::filesystem::rename(entry.destination, state.backup, filesystemError);
            state.backedUp = !filesystemError;
        }
        if (filesystemError) { error = filesystemError.message(); goto rollback; }
        std::filesystem::rename(state.sibling, entry.destination, filesystemError);
        if (filesystemError) { error = filesystemError.message(); goto rollback; }
#endif
        state.installed = true;
    }

    for (const State& state : states) removeNoThrow(state.backup);
    committed_ = true;
    return true;

rollback:
    {
        const std::string commitError = error;
        bool rollbackFailed = false;
        for (std::size_t reverse = entries_.size(); reverse != 0; --reverse)
        {
            const std::size_t index = reverse - 1U;
            const Entry& entry = entries_[index];
            State& state = states[index];
            if (state.installed) removeNoThrow(entry.destination);
            if (state.backedUp)
            {
#if defined(_WIN32)
                std::string ignored;
                if (!moveReplace(state.backup, entry.destination, ignored)) rollbackFailed = true;
#else
                std::error_code filesystemError;
                std::filesystem::rename(state.backup, entry.destination, filesystemError);
                rollbackFailed = rollbackFailed || static_cast<bool>(filesystemError);
#endif
            }
            removeNoThrow(state.sibling);
        }
        error = commitError + (rollbackFailed ? "; rollback-incomplete" : "; rollback-complete");
        return false;
    }
}

bool writeFileTransactionally(const std::filesystem::path& destination,
    std::span<const std::uint8_t> bytes, std::string& error)
{
    ArtifactTransaction transaction;
    std::filesystem::path stage;
    if (!transaction.add(destination, stage, error)) return false;
    std::ofstream stream(stage, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = "cannot open staged artifact '" + stage.string() + "'";
        return false;
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream)
    {
        error = "cannot finish staged artifact '" + stage.string() + "'";
        return false;
    }
    return transaction.commit(error);
}

bool writeTextTransactionally(
    const std::filesystem::path& destination, std::string_view text, std::string& error)
{
    return writeFileTransactionally(destination,
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()), error);
}
}
