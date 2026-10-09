#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/ArtifactTransaction.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
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
// Registers one ordered-commit case in the consolidated Host.Basic executable.
void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CB-BAS-013",
        "Host.Basic",
        "component",
        std::move(profile),
        {"basic", "artifact-transaction", "commit"},
        std::move(function),
    });
}

// Converts a path to the extended Win32 spelling used by ArtifactTransaction.
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

// Reads an environment value while preserving absent versus present state.
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

class ADriveTemporaryEnvironment
{
public:
    // Keeps all product-created stages on the authorized A: workspace.
    ADriveTemporaryEnvironment()
        : oldTemp_(environmentValue(L"TEMP")),
          oldTmp_(environmentValue(L"TMP")),
          root_(std::filesystem::current_path())
    {
        if (root_.root_name() != L"A:")
        {
            throw std::runtime_error("artifact commit tests require an A:-drive working directory");
        }
        if (!SetEnvironmentVariableW(L"TEMP", root_.c_str()) ||
            !SetEnvironmentVariableW(L"TMP", root_.c_str()))
        {
            throw std::runtime_error("cannot redirect artifact commit temporaries to A:");
        }
    }

    ADriveTemporaryEnvironment(const ADriveTemporaryEnvironment&) = delete;
    ADriveTemporaryEnvironment& operator=(const ADriveTemporaryEnvironment&) = delete;

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
    // Creates a unique root and records every fixture for iterative cleanup.
    explicit CaseTree(std::string_view caseId)
        : root_(std::filesystem::current_path() /
                (".nsc-suite-" + std::string(caseId) + "-" +
                 std::to_string(GetCurrentProcessId()) + "-" +
                 std::to_string(GetTickCount64())))
    {
        if (!std::filesystem::create_directory(root_))
        {
            throw std::runtime_error("cannot create artifact commit fixture root");
        }
        tracked_.push_back(root_);
    }

    CaseTree(const CaseTree&) = delete;
    CaseTree& operator=(const CaseTree&) = delete;

    ~CaseTree() noexcept
    {
        try
        {
            std::sort(tracked_.begin(), tracked_.end(), [](const auto& left, const auto& right) {
                return left.native().size() > right.native().size();
            });
            for (const auto& path : tracked_)
            {
                const auto native = extendedPath(path);
                const DWORD attributes = GetFileAttributesW(native.c_str());
                if (attributes == INVALID_FILE_ATTRIBUTES)
                {
                    continue;
                }
                if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                {
                    RemoveDirectoryW(native.c_str());
                }
                else
                {
                    DeleteFileW(native.c_str());
                }
            }
        }
        catch (...)
        {
            OutputDebugStringA("NativeSharp test fixture cleanup failed\n");
        }
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept
    {
        return root_;
    }

    // Returns one tracked path below the fixture root without creating it.
    [[nodiscard]] std::filesystem::path path(std::string_view relative)
    {
        const std::filesystem::path value = root_ / std::string(relative);
        tracked_.push_back(value);
        return value;
    }

    // Creates an extended Unicode directory chain without recursive traversal.
    [[nodiscard]] std::filesystem::path deepUnicodeDirectory(std::size_t targetLength)
    {
        std::filesystem::path current = root_;
        std::size_t ordinal = 0;
        while (current.native().size() < targetLength)
        {
            current /= L"directorio-áéíóú-" + std::to_wstring(ordinal++);
            if (!CreateDirectoryW(extendedPath(current).c_str(), nullptr))
            {
                throw std::runtime_error(
                    "cannot create Unicode extended fixture directory: " +
                    std::to_string(GetLastError()));
            }
            tracked_.push_back(current);
        }
        return current;
    }

    void track(const std::filesystem::path& path)
    {
        tracked_.push_back(path);
    }

private:
    std::filesystem::path root_;
    std::vector<std::filesystem::path> tracked_;
};

// Writes bounded stage or destination bytes through the ordinary short fixture path.
void writeText(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream(path, std::ios::binary);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream)
    {
        throw std::runtime_error("cannot write artifact commit fixture");
    }
}

// Reads bounded result bytes through Win32 so paths above MAX_PATH remain observable.
std::string readText(const std::filesystem::path& path)
{
    HANDLE handle = CreateFileW(
        extendedPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("cannot open artifact commit result");
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size) || size.QuadPart < 0 || size.QuadPart > 4096)
    {
        CloseHandle(handle);
        throw std::runtime_error("artifact commit result has invalid fixture size");
    }
    std::string value(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = value.empty() || ReadFile(
        handle, value.data(), static_cast<DWORD>(value.size()), &read, nullptr);
    CloseHandle(handle);
    if (!ok || read != value.size())
    {
        throw std::runtime_error("cannot read complete artifact commit result");
    }
    return value;
}

// Detects transaction-owned sibling or backup files without recursive enumeration.
bool hasTransactionResidue(const std::filesystem::path& directory)
{
    const std::filesystem::path pattern = extendedPath(directory) / L"*";
    WIN32_FIND_DATAW data{};
    HANDLE search = FindFirstFileW(pattern.c_str(), &data);
    if (search == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("cannot enumerate artifact commit destination directory");
    }
    bool residue = false;
    do
    {
        const std::wstring_view name(data.cFileName);
        if (name.find(L".nsc-transaction-") != std::wstring_view::npos)
        {
            residue = true;
            break;
        }
    } while (FindNextFileW(search, &data));
    FindClose(search);
    return residue;
}

// Adds one product-owned stage and fills it with exact fixture bytes.
std::filesystem::path prepare(
    nativesharp::basic::ArtifactTransaction& transaction,
    const std::filesystem::path& destination,
    std::string_view bytes,
    std::string& error)
{
    std::filesystem::path stage;
    if (!transaction.add(destination, stage, error))
    {
        throw std::runtime_error("cannot prepare artifact commit stage: " + error);
    }
    writeText(stage, bytes);
    return stage;
}

void testEmptyCommit(TestContext& context)
{
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error = "stale";
    context.expect(transaction.commit(error), "empty commit must succeed");
    context.expect(error.empty(), "empty commit must clear the incoming error");
    context.expect(!transaction.commit(error), "second empty commit must fail");
    context.expect(error == "artifact transaction was already committed", "repeat error must be exact");
}

void testSecondCommit(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-059");
    const auto destination = tree.path("published.lib");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    prepare(transaction, destination, "published once", error);
    context.expect(transaction.commit(error), "first populated commit must succeed");
    context.expect(!transaction.commit(error), "second populated commit must fail");
    context.expect(error == "artifact transaction was already committed", "repeat error must be exact");
    context.expect(readText(destination) == "published once", "second commit must preserve published bytes");
}

void testNewDestination(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-068");
    const auto destination = tree.path("Product.lib");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    const auto stage = prepare(transaction, destination, "new product", error);
    context.expect(stage.filename() == destination.filename(), "stage must preserve the output basename");
    context.expect(stage != destination, "stage must remain isolated from the destination");
    context.expect(transaction.commit(error), "new destination commit must succeed");
    context.expect(error.empty(), "successful commit must leave no error");
    context.expect(readText(destination) == "new product", "new destination bytes must be exact");
    context.expect(!hasTransactionResidue(tree.root()), "new destination commit must remove siblings");
}

void testExistingDestination(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-069");
    const auto destination = tree.path("existing.lib");
    writeText(destination, "old product");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    prepare(transaction, destination, "new product", error);
    context.expect(transaction.commit(error), "existing destination commit must succeed");
    context.expect(readText(destination) == "new product", "existing destination must be replaced");
    context.expect(!hasTransactionResidue(tree.root()), "successful replacement must remove backup and sibling");
}

void testThreeNewDestinations(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-070");
    const std::vector<std::pair<std::filesystem::path, std::string>> entries{
        {tree.path("first.lib"), "first"},
        {tree.path("second.lib"), "second"},
        {tree.path("third.lib"), "third"},
    };
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    for (const auto& [destination, bytes] : entries)
    {
        prepare(transaction, destination, bytes, error);
    }
    context.expect(transaction.commit(error), "three-destination commit must succeed");
    for (const auto& [destination, bytes] : entries)
    {
        context.expect(readText(destination) == bytes, "each new destination must preserve exact bytes");
    }
    context.expect(!hasTransactionResidue(tree.root()), "multi-file commit must remove all siblings");
}

void testMixedDestinations(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-071");
    const auto existing = tree.path("existing.lib");
    const auto added = tree.path("added.lib");
    const auto secondExisting = tree.path("second-existing.lib");
    writeText(existing, "old one");
    writeText(secondExisting, "old two");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    prepare(transaction, existing, "new one", error);
    prepare(transaction, added, "new middle", error);
    prepare(transaction, secondExisting, "new two", error);
    context.expect(transaction.commit(error), "mixed commit must succeed");
    context.expect(readText(existing) == "new one", "first existing destination must be replaced");
    context.expect(readText(added) == "new middle", "new destination must be installed");
    context.expect(readText(secondExisting) == "new two", "second existing destination must be replaced");
    context.expect(!hasTransactionResidue(tree.root()), "mixed commit must remove all backups and siblings");
}

void testUnicodeExtendedDestination(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-072");
    const auto parent = tree.deepUnicodeDirectory(320U);
    const std::filesystem::path destination = parent / L"salida-ñ.lib";
    tree.track(destination);
    context.expect(destination.native().size() > MAX_PATH, "fixture must exceed the legacy MAX_PATH limit");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    prepare(transaction, destination, "unicode extended path", error);
    context.expect(transaction.commit(error), "Unicode extended destination commit must succeed");
    context.expect(error.empty(), "Unicode extended commit must leave no error");
    context.expect(readText(destination) == "unicode extended path", "extended destination bytes must be exact");
    context.expect(!hasTransactionResidue(parent), "extended destination must not retain transaction residue");
}
}

// Registers every executable CB-BAS-013 case without modifying product behavior.
void registerBasicArtifactCommitModule(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-058", "PR", testEmptyCommit);
    addCase(registry, "TB-BAS-059", "PR", testSecondCommit);
    addCase(registry, "TB-BAS-068", "PR", testNewDestination);
    addCase(registry, "TB-BAS-069", "PR", testExistingDestination);
    addCase(registry, "TB-BAS-070", "PR", testThreeNewDestinations);
    addCase(registry, "TB-BAS-071", "PR", testMixedDestinations);
    addCase(registry, "TB-BAS-072", "NIGHTLY", testUnicodeExtendedDestination);
}
}
