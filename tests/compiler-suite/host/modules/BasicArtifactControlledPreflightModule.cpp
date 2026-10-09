#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/ArtifactTransaction.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winioctl.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
// Registers one controlled Windows preflight case. The consolidated host owns the callback and
// executes it only after selection by stable case identifier. Time and space are O(1).
void addCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CB-BAS-012",
        "Host.Basic",
        "component",
        std::move(profile),
        {"basic", "artifact-transaction", "preflight", "windows"},
        std::move(function),
    });
}

// Reads one process-environment value while preserving absent versus present values.
// The optional owns the UTF-16 buffer and requires O(n) time and memory.
std::optional<std::wstring> environmentValue(const wchar_t* name)
{
    const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0)
    {
        return std::nullopt;
    }
    std::wstring value(required, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), required);
    if (written == 0 || written >= required)
    {
        throw std::runtime_error("cannot read temporary-directory environment");
    }
    value.resize(written);
    return value;
}

// Converts an absolute or relative path into the Win32 extended spelling used by the product.
// No filesystem object is created; time and memory are O(path length).
std::filesystem::path extendedPath(const std::filesystem::path& input)
{
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(input, error);
    if (error)
    {
        absolute = input;
    }
    const std::wstring value = absolute.native();
    if (value.starts_with(L"\\\\?\\"))
    {
        return absolute;
    }
    if (value.starts_with(L"\\\\"))
    {
        return std::filesystem::path(L"\\\\?\\UNC\\" + value.substr(2));
    }
    return std::filesystem::path(L"\\\\?\\" + value);
}

class ADriveTemporaryEnvironment
{
public:
    // Redirects TEMP and TMP to the current authorized A: workspace for one test case.
    ADriveTemporaryEnvironment()
        : oldTemp_(environmentValue(L"TEMP")),
          oldTmp_(environmentValue(L"TMP")),
          root_(std::filesystem::current_path())
    {
        if (root_.root_name() != L"A:")
        {
            throw std::runtime_error("controlled preflight tests require an A:-drive workspace");
        }
        if (!SetEnvironmentVariableW(L"TEMP", root_.c_str()) ||
            !SetEnvironmentVariableW(L"TMP", root_.c_str()))
        {
            throw std::runtime_error("cannot redirect controlled preflight temporaries to A:");
        }
    }

    ADriveTemporaryEnvironment(const ADriveTemporaryEnvironment&) = delete;
    ADriveTemporaryEnvironment& operator=(const ADriveTemporaryEnvironment&) = delete;

    // Restores the exact prior environment state, including variables that were absent.
    ~ADriveTemporaryEnvironment()
    {
        SetEnvironmentVariableW(L"TEMP", oldTemp_ ? oldTemp_->c_str() : nullptr);
        SetEnvironmentVariableW(L"TMP", oldTmp_ ? oldTmp_->c_str() : nullptr);
    }

private:
    std::optional<std::wstring> oldTemp_;
    std::optional<std::wstring> oldTmp_;
    std::filesystem::path root_;
};

class CaseTree
{
public:
    // Creates a process-unique fixture root and records it for explicit iterative cleanup.
    explicit CaseTree(std::string_view caseId)
        : root_(std::filesystem::current_path() /
                (".nsc-suite-" + std::string(caseId) + "-" +
                 std::to_string(GetCurrentProcessId())))
    {
        if (!std::filesystem::create_directory(root_))
        {
            throw std::runtime_error("cannot create controlled preflight fixture root");
        }
        tracked_.push_back(root_);
    }

    CaseTree(const CaseTree&) = delete;
    CaseTree& operator=(const CaseTree&) = delete;

    // Removes tracked leaves before parents through Win32 calls that support extended paths.
    ~CaseTree()
    {
        std::sort(tracked_.begin(), tracked_.end(), [](const auto& left, const auto& right) {
            return left.native().size() > right.native().size();
        });
        for (const auto& path : tracked_)
        {
            const auto extended = extendedPath(path);
            const DWORD attributes = GetFileAttributesW(extended.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES)
            {
                continue;
            }
            if ((attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
            {
                RemoveDirectoryW(extended.c_str());
            }
            else
            {
                DeleteFileW(extended.c_str());
            }
        }
    }

    // Returns and tracks a path below the case root without creating it.
    [[nodiscard]] std::filesystem::path path(std::string_view relative)
    {
        const std::filesystem::path value = root_ / std::string(relative);
        tracked_.push_back(value);
        return value;
    }

    // Creates and tracks one direct directory below the case root.
    [[nodiscard]] std::filesystem::path directory(std::string_view relative)
    {
        const std::filesystem::path value = path(relative);
        if (!std::filesystem::create_directory(value))
        {
            throw std::runtime_error("cannot create controlled preflight directory");
        }
        return value;
    }

    // Builds an extended directory iteratively with components bounded below the NTFS limit.
    // Time, filesystem objects, and retained cleanup paths are O(targetLength / 200).
    [[nodiscard]] std::filesystem::path deepDirectory(std::size_t targetLength)
    {
        std::filesystem::path current = root_;
        while (current.native().size() < targetLength)
        {
            const std::size_t remaining = targetLength - current.native().size();
            const std::size_t componentLength = std::min<std::size_t>(200, remaining > 1 ? remaining - 1 : 1);
            current /= std::wstring(componentLength, L'd');
            if (!CreateDirectoryW(extendedPath(current).c_str(), nullptr))
            {
                throw std::runtime_error(
                    "cannot create extended controlled preflight directory: " +
                    std::to_string(GetLastError()));
            }
            tracked_.push_back(current);
        }
        return current;
    }

    // Adds a computed sibling or backup path to the iterative cleanup inventory.
    void track(const std::filesystem::path& value)
    {
        tracked_.push_back(value);
    }

private:
    std::filesystem::path root_;
    std::vector<std::filesystem::path> tracked_;
};

class Win32Handle
{
public:
    // Takes exclusive ownership of one Win32 handle; INVALID_HANDLE_VALUE means empty.
    explicit Win32Handle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept : handle_(handle) {}
    Win32Handle(const Win32Handle&) = delete;
    Win32Handle& operator=(const Win32Handle&) = delete;

    // Transfers ownership without duplicating or closing the native handle.
    Win32Handle(Win32Handle&& other) noexcept : handle_(other.handle_)
    {
        other.handle_ = INVALID_HANDLE_VALUE;
    }

    // Replaces the current resource and transfers ownership from another wrapper.
    Win32Handle& operator=(Win32Handle&& other) noexcept
    {
        if (this != &other)
        {
            close();
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    // Closes the owned handle exactly once.
    ~Win32Handle()
    {
        close();
    }

    // Returns the borrowed native handle without transferring ownership.
    [[nodiscard]] HANDLE get() const noexcept
    {
        return handle_;
    }

    // Releases the native resource and makes later calls idempotent.
    void close() noexcept
    {
        if (handle_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE handle_;
};

struct MountPointReparseData
{
    ULONG tag;
    USHORT dataLength;
    USHORT reserved;
    USHORT substituteOffset;
    USHORT substituteLength;
    USHORT printOffset;
    USHORT printLength;
    WCHAR pathBuffer[1];
};

// Writes an exact binary fixture and reports incomplete writes as infrastructure failures.
void writeText(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream)
    {
        throw std::runtime_error("cannot write controlled preflight fixture");
    }
}

// Reads all bytes from a bounded fixture. Time and memory are O(file size).
std::string readText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

// Creates a directory junction without requiring symbolic-link privilege.
// The reparse buffer contains one substitute name and one printable name.
void createJunction(const std::filesystem::path& link, const std::filesystem::path& target)
{
    const std::wstring printName = std::filesystem::absolute(target).native();
    const std::wstring substituteName = L"\\??\\" + printName;
    const std::size_t pathCharacters = substituteName.size() + 1 + printName.size() + 1;
    const std::size_t bufferSize = offsetof(MountPointReparseData, pathBuffer) +
        pathCharacters * sizeof(wchar_t);
    std::vector<std::byte> storage(bufferSize);
    auto* data = reinterpret_cast<MountPointReparseData*>(storage.data());
    data->tag = IO_REPARSE_TAG_MOUNT_POINT;
    data->dataLength = static_cast<USHORT>(bufferSize - 8U);
    data->substituteOffset = 0;
    data->substituteLength = static_cast<USHORT>(substituteName.size() * sizeof(wchar_t));
    data->printOffset = static_cast<USHORT>((substituteName.size() + 1) * sizeof(wchar_t));
    data->printLength = static_cast<USHORT>(printName.size() * sizeof(wchar_t));
    std::memcpy(data->pathBuffer, substituteName.c_str(), data->substituteLength + sizeof(wchar_t));
    std::memcpy(
        reinterpret_cast<std::byte*>(data->pathBuffer) + data->printOffset,
        printName.c_str(),
        data->printLength + sizeof(wchar_t));
    Win32Handle handle(CreateFileW(
        extendedPath(link).c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    DWORD returned = 0;
    if (handle.get() == INVALID_HANDLE_VALUE ||
        !DeviceIoControl(handle.get(), FSCTL_SET_REPARSE_POINT, data,
            static_cast<DWORD>(bufferSize), nullptr, 0, &returned, nullptr))
    {
        throw std::runtime_error("cannot create controlled reparse point: " +
            std::to_string(GetLastError()));
    }
}

// Creates a sparse file with the requested logical size without allocating its data range.
void createSparseFile(const std::filesystem::path& path, unsigned long long logicalSize)
{
    Win32Handle handle(CreateFileW(
        extendedPath(path).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr));
    DWORD returned = 0;
    if (handle.get() == INVALID_HANDLE_VALUE ||
        !DeviceIoControl(handle.get(), FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr))
    {
        throw std::runtime_error("cannot create sparse preflight stage: " +
            std::to_string(GetLastError()));
    }
    LARGE_INTEGER end{};
    end.QuadPart = static_cast<LONGLONG>(logicalSize);
    if (!SetFilePointerEx(handle.get(), end, nullptr, FILE_BEGIN) || !SetEndOfFile(handle.get()))
    {
        throw std::runtime_error("cannot size sparse preflight stage: " +
            std::to_string(GetLastError()));
    }
}

// Extracts the monotonically increasing transaction ordinal from a generated staging root.
unsigned long long stagingOrdinal(const std::filesystem::path& stagingRoot)
{
    const std::string name = stagingRoot.filename().string();
    const std::size_t attemptSeparator = name.rfind('-');
    const std::size_t ordinalSeparator = name.rfind('-', attemptSeparator - 1);
    if (attemptSeparator == std::string::npos || ordinalSeparator == std::string::npos)
    {
        throw std::runtime_error("staging root lacks ordinal fields");
    }
    return std::stoull(name.substr(ordinalSeparator + 1, attemptSeparator - ordinalSeparator - 1));
}

// Returns the transaction-owned root above the per-entry directory and output basename.
std::filesystem::path stagingRootFrom(const std::filesystem::path& stage)
{
    return stage.parent_path().parent_path();
}

// Constructs the exact sibling name used by one commit ordinal.
std::filesystem::path siblingPath(
    const std::filesystem::path& destination,
    unsigned long long ordinal,
    std::string_view extension)
{
    std::filesystem::path sibling = destination;
    sibling += ".nsc-transaction-" + std::to_string(ordinal) + std::string(extension);
    return sibling;
}

// Exercises rejection of an existing Windows reparse-point destination.
void testReparsePointDestination(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-064");
    const auto target = tree.directory("junction-target");
    const auto destination = tree.directory("destination-junction");
    const auto stage = tree.path("stage.lib");
    writeText(stage, "new bytes");
    createJunction(destination, target);
    const DWORD before = GetFileAttributesW(extendedPath(destination).c_str());
    context.expect((before & FILE_ATTRIBUTE_REPARSE_POINT) != 0, "fixture must be a reparse point");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    context.expect(transaction.addStaged(destination, stage, error), "reparse destination must register");
    context.expect(!transaction.commit(error), "reparse destination must fail preflight");
    context.expect(error.find("destination-not-regular") != std::string::npos, "reparse error category");
    const DWORD after = GetFileAttributesW(extendedPath(destination).c_str());
    context.expect((after & FILE_ATTRIBUTE_REPARSE_POINT) != 0, "reparse destination must remain intact");
    context.expect(std::filesystem::is_regular_file(stage), "reparse rejection must preserve the stage");
}

// Exercises the Windows extended-path boundary without creating an invalid destination object.
void testExtendedPathLimit(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-065");
    const auto stage = tree.path("stage.lib");
    writeText(stage, "new bytes");
    const auto parent = tree.deepDirectory(32600U);
    const std::filesystem::path destination = parent / std::wstring(300U, L'f');
    context.expect(destination.native().size() >= 32767U, "fixture must cross the Windows path boundary");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    context.expect(transaction.addStaged(destination, stage, error), "extended destination must register");
    context.expect(!transaction.commit(error), "extended destination must fail preflight");
    context.expect(error.find("path-limit") != std::string::npos, "extended destination error category");
    context.expect(std::filesystem::is_regular_file(stage), "path rejection must preserve the stage");
}

// Exercises free-space rejection with a sparse stage one byte larger than reported capacity.
void testInsufficientSpace(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-066");
    const auto destination = tree.path("destination.lib");
    const auto stage = tree.path("oversized-stage.lib");
    writeText(destination, "original");
    ULARGE_INTEGER available{};
    if (!GetDiskFreeSpaceExW(extendedPath(destination.parent_path()).c_str(), &available, nullptr, nullptr))
    {
        throw std::runtime_error("cannot query controlled preflight free space");
    }
    createSparseFile(stage, available.QuadPart + 1ULL);
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    context.expect(transaction.addStaged(destination, stage, error), "sparse stage must register");
    context.expect(!transaction.commit(error), "oversized sparse stage must fail preflight");
    context.expect(error.find("insufficient-space") != std::string::npos, "space error category");
    context.expect(readText(destination) == "original", "space rejection must preserve destination bytes");
    context.expect(std::filesystem::is_regular_file(stage), "space rejection must preserve the stage");
}

// Exercises all-or-nothing preflight when the last sibling name is occupied by a directory.
void testLastSiblingCopyFailure(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-067");
    const auto firstDestination = tree.path("first.lib");
    const auto secondDestination = tree.path("second.lib");
    const auto firstStage = tree.path("first-stage.lib");
    const auto secondStage = tree.path("second-stage.lib");
    writeText(firstDestination, "first original");
    writeText(secondDestination, "second original");
    writeText(firstStage, "first new");
    writeText(secondStage, "second new");
    nativesharp::basic::ArtifactTransaction probe;
    std::filesystem::path probeStage;
    std::string error;
    if (!probe.add(tree.path("probe.lib"), probeStage, error))
    {
        throw std::runtime_error("cannot reveal controlled preflight ordinal: " + error);
    }
    const unsigned long long commitOrdinal = stagingOrdinal(stagingRootFrom(probeStage)) + 1ULL;
    const auto firstSibling = siblingPath(firstDestination, commitOrdinal, ".new");
    const auto secondSibling = siblingPath(secondDestination, commitOrdinal, ".new");
    tree.track(firstSibling);
    tree.track(secondSibling);
    if (!std::filesystem::create_directory(secondSibling))
    {
        throw std::runtime_error("cannot create last-sibling directory fixture");
    }
    nativesharp::basic::ArtifactTransaction transaction;
    context.expect(transaction.addStaged(firstDestination, firstStage, error), "first entry must register");
    context.expect(transaction.addStaged(secondDestination, secondStage, error), "second entry must register");
    context.expect(!transaction.commit(error), "last sibling copy must fail preflight");
    context.expect(
        error.find("permission-denied") != std::string::npos,
        "last sibling copy must report the controlled directory collision");
    context.expect(readText(firstDestination) == "first original", "first destination must remain unchanged");
    context.expect(readText(secondDestination) == "second original", "second destination must remain unchanged");
    context.expect(!std::filesystem::exists(firstSibling), "prepared first sibling must be cleaned");
    context.expect(std::filesystem::is_directory(secondSibling), "fault fixture must remain distinguishable");
    context.expect(std::filesystem::remove(secondSibling), "fault fixture must be removable after the assertion");
    context.expect(!std::filesystem::exists(secondSibling), "no sibling may remain after fixture removal");
}
}

// Registers the controlled Windows half of CB-BAS-012 without modifying product behavior.
void registerBasicArtifactControlledPreflightModule(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-064", "NIGHTLY", testReparsePointDestination);
    addCase(registry, "TB-BAS-065", "WEEKLY", testExtendedPathLimit);
    addCase(registry, "TB-BAS-066", "WEEKLY", testInsufficientSpace);
    addCase(registry, "TB-BAS-067", "NIGHTLY", testLastSiblingCopyFailure);
}
}
