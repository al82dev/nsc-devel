#include "nativesharp/Basic/ArtifactTransaction.h"
#include "Internal/ArtifactCommitState.h"

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

std::string pathText(const std::filesystem::path& path)
{
    const std::u8string encoded = path.u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

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
    error = std::string(operation) + " failed for '" + pathText(path) + "': " +
        std::string(outputFailureKindName(kind)) + " (Win32 error " + std::to_string(code) + ")";
    return false;
}

bool ensureDirectory(const std::filesystem::path& directory, std::string& error)
{
    if (directory.empty()) return true;
    std::error_code filesystemError;
    std::filesystem::create_directories(directory, filesystemError);
    if (!filesystemError) return true;

    // The MSVC filesystem implementation does not reliably enumerate a new directory tree
    // when its input already carries the Win32 extended-path prefix. Use the native UTF-16
    // path first, then retain the extended form as the long-path fallback.
    const std::filesystem::path extended = extendedPath(directory);
    filesystemError.clear();
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

bool destinationCanBeReplaced(const std::filesystem::path& path, std::string& error)
{
    const DWORD attributes = ::GetFileAttributesW(extendedPath(path).c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        const DWORD code = ::GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) return true;
        return failWin32("artifact destination preflight", path, code, error);
    }
    if ((attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0) return true;
    error = "artifact destination preflight failed for '" + pathText(path) +
        "': destination-not-regular";
    return false;
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
    error = "output directory creation failed for '" + pathText(directory) + "': " + filesystemError.message();
    return false;
}
bool destinationCanBeReplaced(const std::filesystem::path& path, std::string& error)
{
    std::error_code filesystemError;
    const std::filesystem::file_status status = std::filesystem::symlink_status(path, filesystemError);
    if (filesystemError == std::errc::no_such_file_or_directory) return true;
    if (filesystemError)
    {
        error = "artifact destination preflight failed for '" + pathText(path) + "': " +
            filesystemError.message();
        return false;
    }
    if (!std::filesystem::exists(status) || std::filesystem::is_regular_file(status)) return true;
    error = "artifact destination preflight failed for '" + pathText(path) +
        "': destination-not-regular";
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

bool ArtifactTransaction::prepareCommitEntry(
    std::size_t index,
    std::vector<detail::ArtifactCommitState>& states,
    const std::string& suffix,
    std::string& error)
{
    const Entry& entry = entries_[index];
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(entry.stage, filesystemError) || filesystemError)
    {
        error = "artifact transaction stage is missing: '" + pathText(entry.stage) + "'";
        return false;
    }
    if (!ensureDirectory(entry.destination.parent_path(), error)) return false;
    if (!destinationCanBeReplaced(entry.destination, error)) return false;
    states[index].sibling = entry.destination;
    states[index].sibling += suffix + ".new";
    states[index].backup = entry.destination;
    states[index].backup += suffix + ".old";
#if defined(_WIN32)
    if (entry.destination.native().size() >= 32767U)
    {
        error = "artifact preflight failed for '" + pathText(entry.destination) +
            "': path-limit (Windows extended path exceeds 32767 UTF-16 code units)";
        return false;
    }
    ULARGE_INTEGER available{};
    // EN: The directory comes from the absolute extended destination: a destination given without a
    //     directory (`-o name.exe`) has an empty parent_path(), whose extended spelling `\\?\` is an
    //     invalid name (Win32 error 123). The parent of the absolute form is the current directory.
    // ES: El directorio sale del destino extendido absoluto: un destino sin directorio (`-o name.exe`)
    //     tiene parent_path() vacio, cuya forma extendida `\\?\` es un nombre invalido (error Win32
    //     123). El padre de la forma absoluta es el directorio actual.
    if (::GetDiskFreeSpaceExW(extendedPath(entry.destination).parent_path().c_str(),
            &available, nullptr, nullptr) == 0)
    {
        return failWin32(
            "artifact free-space preflight", entry.destination, ::GetLastError(), error);
    }
    const auto required = std::filesystem::file_size(entry.stage, filesystemError);
    if (filesystemError)
    {
        error = "cannot measure staged artifact '" + pathText(entry.stage) + "': " +
            filesystemError.message();
        return false;
    }
    if (available.QuadPart < required)
    {
        error = "artifact preflight failed for '" + pathText(entry.destination) +
            "': insufficient-space";
        return false;
    }
    removeNoThrow(states[index].sibling);
    removeNoThrow(states[index].backup);
    return copyToSibling(entry.stage, states[index].sibling, error);
#else
    std::filesystem::copy_file(
        entry.stage,
        states[index].sibling,
        std::filesystem::copy_options::overwrite_existing,
        filesystemError);
    if (!filesystemError) return true;
    error = "artifact staging copy failed for '" + pathText(entry.destination) + "': " +
        filesystemError.message();
    return false;
#endif
}

bool ArtifactTransaction::installCommitEntry(
    std::size_t index,
    std::vector<detail::ArtifactCommitState>& states,
    std::string& error)
{
    const Entry& entry = entries_[index];
    detail::ArtifactCommitState& state = states[index];
#if defined(_WIN32)
    if (!exists(entry.destination, state.hadDestination, error)) return false;
    if (state.hadDestination)
    {
        if (!moveReplace(entry.destination, state.backup, error)) return false;
        state.backedUp = true;
    }
    if (!moveReplace(state.sibling, entry.destination, error)) return false;
#else
    std::error_code filesystemError;
    state.hadDestination = std::filesystem::exists(entry.destination, filesystemError);
    if (state.hadDestination)
    {
        std::filesystem::rename(entry.destination, state.backup, filesystemError);
        state.backedUp = !filesystemError;
    }
    if (filesystemError)
    {
        error = filesystemError.message();
        return false;
    }
    std::filesystem::rename(state.sibling, entry.destination, filesystemError);
    if (filesystemError)
    {
        error = filesystemError.message();
        return false;
    }
#endif
    state.installed = true;
    return true;
}

void ArtifactTransaction::rollbackCommit(
    std::vector<detail::ArtifactCommitState>& states,
    std::string& error)
{
    const std::string commitError = error;
    bool rollbackFailed = false;
    for (std::size_t reverse = entries_.size(); reverse != 0; --reverse)
    {
        const std::size_t index = reverse - 1U;
        const Entry& entry = entries_[index];
        detail::ArtifactCommitState& state = states[index];
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

    std::vector<detail::ArtifactCommitState> states(entries_.size());
    const std::string suffix = ".nsc-transaction-" +
        std::to_string(transactionOrdinal.fetch_add(1, std::memory_order_relaxed));
    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        if (prepareCommitEntry(index, states, suffix, error)) continue;
        for (const detail::ArtifactCommitState& state : states)
        {
            removeNoThrow(state.sibling);
            removeNoThrow(state.backup);
        }
        return false;
    }

    for (std::size_t index = 0; index < entries_.size(); ++index)
    {
        if (installCommitEntry(index, states, error)) continue;
        rollbackCommit(states, error);
        return false;
    }

    for (const detail::ArtifactCommitState& state : states) removeNoThrow(state.backup);
    committed_ = true;
    return true;
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
        error = "cannot open staged artifact '" + pathText(stage) + "'";
        return false;
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream)
    {
        error = "cannot finish staged artifact '" + pathText(stage) + "'";
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

bool seedArtifactStage(const std::filesystem::path& source,
    const std::filesystem::path& stage, std::string& error)
{
    error.clear();
    if (source.empty() || stage.empty())
    {
        error = "artifact seed source or stage is empty";
        return false;
    }
#if defined(_WIN32)
    bool present = false;
    if (!exists(source, present, error)) return false;
    if (!present) return true;
    if (!ensureDirectory(stage.parent_path(), error)) return false;
    return copyToSibling(source, stage, error);
#else
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(source, filesystemError))
        return !filesystemError;
    if (!ensureDirectory(stage.parent_path(), error)) return false;
    std::filesystem::copy_file(source, stage,
        std::filesystem::copy_options::overwrite_existing, filesystemError);
    if (!filesystemError) return true;
    error = "artifact seed copy failed for '" + pathText(stage) + "': " + filesystemError.message();
    return false;
#endif
}
}
