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
// Registers one canonical CB-BAS-012 case. The callback is stored by the consolidated host and
// executed only after the controller selects its stable case identifier. Time and space are O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CB-BAS-012",
        "Host.Basic",
        "component",
        "PR",
        {"basic", "artifact-transaction", "preflight"},
        std::move(function),
    });
}

// Reads one process-environment value while preserving the distinction between absent and empty.
// The returned optional owns its buffer; the operation is O(n) in the UTF-16 value length.
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
    // Redirects TEMP and TMP to the authorized A: workspace for the lifetime of one test case.
    // Construction fails before touching fixtures when the host is not running from A:.
    ADriveTemporaryEnvironment()
        : oldTemp_(environmentValue(L"TEMP")),
          oldTmp_(environmentValue(L"TMP")),
          root_(std::filesystem::current_path())
    {
        if (root_.root_name() != L"A:")
        {
            throw std::runtime_error("artifact preflight tests require an A:-drive working directory");
        }
        if (!SetEnvironmentVariableW(L"TEMP", root_.c_str()) ||
            !SetEnvironmentVariableW(L"TMP", root_.c_str()))
        {
            throw std::runtime_error("cannot redirect artifact preflight temporaries to A:");
        }
    }

    ADriveTemporaryEnvironment(const ADriveTemporaryEnvironment&) = delete;
    ADriveTemporaryEnvironment& operator=(const ADriveTemporaryEnvironment&) = delete;

    // Restores the exact environment state observed at construction, including absent values.
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
            throw std::runtime_error("cannot create artifact preflight fixture root: " + root_.string());
        }
        tracked_.push_back(root_);
    }

    CaseTree(const CaseTree&) = delete;
    CaseTree& operator=(const CaseTree&) = delete;

    // Removes tracked leaves before their parents without recursive filesystem traversal.
    ~CaseTree()
    {
        std::sort(tracked_.begin(), tracked_.end(), [](const auto& left, const auto& right) {
            return left.native().size() > right.native().size();
        });
        std::error_code ignored;
        for (const auto& path : tracked_)
        {
            std::filesystem::remove(path, ignored);
            ignored.clear();
        }
    }

    // Returns and tracks a path below the case root without creating the filesystem object.
    [[nodiscard]] std::filesystem::path path(std::string_view relative)
    {
        const std::filesystem::path value = root_ / std::string(relative);
        tracked_.push_back(value);
        return value;
    }

    // Creates and tracks one directory below the case root; nested parents are intentionally absent.
    [[nodiscard]] std::filesystem::path directory(std::string_view relative)
    {
        const std::filesystem::path value = path(relative);
        if (!std::filesystem::create_directory(value))
        {
            throw std::runtime_error("cannot create artifact preflight directory: " + value.string());
        }
        return value;
    }

private:
    std::filesystem::path root_;
    std::vector<std::filesystem::path> tracked_;
};

// Writes an exact binary fixture and reports incomplete writes as infrastructure failures.
void writeText(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream)
    {
        throw std::runtime_error("cannot write artifact preflight fixture: " + path.string());
    }
}

// Reads all bytes from a small fixture. The test inputs are bounded, so memory is O(file size).
std::string readText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

// Converts a filesystem path to the UTF-8 spelling used by ArtifactTransaction diagnostics.
std::string utf8Path(const std::filesystem::path& path)
{
    const std::u8string encoded = path.u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}
}

// Registers the four ordinary CB-BAS-012 preflight contracts. Each callback owns an isolated
// fixture tree and verifies both the reported outcome and the filesystem state left behind.
void registerBasicArtifactPreflightModule(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-060", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        CaseTree tree("TB-BAS-060");
        const auto destination = tree.path("destination.lib");
        const auto missingStage = tree.path("missing-stage.lib");
        writeText(destination, "original");
        nativesharp::basic::ArtifactTransaction transaction;
        std::string error;
        context.expect(
            transaction.addStaged(destination, missingStage, error),
            "missing stage must register before preflight");
        const bool committed = transaction.commit(error);
        context.expect(!committed, "missing stage must fail preflight");
        context.expect(
            error == "artifact transaction stage is missing: '" + utf8Path(missingStage) + "'",
            "missing stage error must be exact");
        context.expect(readText(destination) == "original", "missing stage must not alter the destination");
    });
    addCase(registry, "TB-BAS-061", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        CaseTree tree("TB-BAS-061");
        const auto destination = tree.path("destination.lib");
        const auto stageDirectory = tree.directory("stage-directory");
        writeText(destination, "original");
        nativesharp::basic::ArtifactTransaction transaction;
        std::string error;
        context.expect(
            transaction.addStaged(destination, stageDirectory, error),
            "directory stage must register before preflight");
        const bool committed = transaction.commit(error);
        context.expect(!committed, "directory stage must fail preflight");
        context.expect(
            error == "artifact transaction stage is missing: '" + utf8Path(stageDirectory) + "'",
            "directory stage error must match a non-regular stage");
        context.expect(readText(destination) == "original", "directory stage must not alter the destination");
    });
    addCase(registry, "TB-BAS-062", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        CaseTree tree("TB-BAS-062");
        const auto stage = tree.path("stage.lib");
        const auto parent = tree.path("new-parent");
        const auto destination = tree.path("new-parent/published.lib");
        writeText(stage, "new bytes");
        nativesharp::basic::ArtifactTransaction transaction;
        std::string error;
        context.expect(transaction.addStaged(destination, stage, error), "valid stage must register");
        const bool committed = transaction.commit(error);
        context.expect(committed, "preflight must create a missing destination parent");
        context.expect(error.empty(), "successful parent creation and commit must leave no error");
        context.expect(std::filesystem::is_directory(parent), "missing destination parent must be created");
        context.expect(readText(destination) == "new bytes", "published destination must preserve stage bytes");
    });
    addCase(registry, "TB-BAS-063", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        CaseTree tree("TB-BAS-063");
        const auto stage = tree.path("stage.lib");
        const auto destination = tree.directory("destination-directory");
        writeText(stage, "new bytes");
        nativesharp::basic::ArtifactTransaction transaction;
        std::string error;
        context.expect(
            transaction.addStaged(destination, stage, error),
            "directory destination must register before preflight");
        const bool committed = transaction.commit(error);
        context.expect(!committed, "directory destination must fail preflight");
        context.expect(
            error.find("destination-not-regular") != std::string::npos,
            "directory destination must report destination-not-regular");
        context.expect(std::filesystem::is_directory(destination), "rejected destination directory must remain intact");
        context.expect(std::filesystem::is_regular_file(stage), "rejected preflight must preserve the stage");
    });
}
}
