#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/ArtifactTransaction.h"
#include "../support/ControlledMoveFailure.h"

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
// Registers one reverse-rollback case in the consolidated Host.Basic executable.
void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CB-BAS-014",
        "Host.Basic",
        "component",
        std::move(profile),
        {"basic", "artifact-transaction", "rollback", "windows"},
        std::move(function),
    });
}

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
        throw std::runtime_error("cannot read rollback temporary-directory environment");
    }
    value.resize(written);
    return value;
}

class ADriveTemporaryEnvironment
{
public:
    ADriveTemporaryEnvironment()
        : oldTemp_(environmentValue(L"TEMP")),
          oldTmp_(environmentValue(L"TMP")),
          root_(std::filesystem::current_path())
    {
        if (root_.root_name() != L"A:")
        {
            throw std::runtime_error("artifact rollback tests require an A:-drive working directory");
        }
        if (!SetEnvironmentVariableW(L"TEMP", root_.c_str()) ||
            !SetEnvironmentVariableW(L"TMP", root_.c_str()))
        {
            throw std::runtime_error("cannot redirect artifact rollback temporaries to A:");
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
    explicit CaseTree(std::string_view caseId)
        : root_(std::filesystem::current_path() /
                (".nsc-suite-" + std::string(caseId) + "-" +
                 std::to_string(GetCurrentProcessId()) + "-" +
                 std::to_string(GetTickCount64())))
    {
        if (!std::filesystem::create_directory(root_))
        {
            throw std::runtime_error("cannot create artifact rollback fixture root");
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
            std::error_code ignored;
            for (const auto& path : tracked_)
            {
                std::filesystem::remove(path, ignored);
                ignored.clear();
            }
        }
        catch (...)
        {
            OutputDebugStringA("NativeSharp rollback fixture cleanup failed\n");
        }
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept
    {
        return root_;
    }

    [[nodiscard]] std::filesystem::path path(std::string_view relative)
    {
        const std::filesystem::path value = root_ / std::string(relative);
        tracked_.push_back(value);
        return value;
    }

    void track(const std::filesystem::path& path)
    {
        tracked_.push_back(path);
    }

private:
    std::filesystem::path root_;
    std::vector<std::filesystem::path> tracked_;
};

void writeText(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream(path, std::ios::binary);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream)
    {
        throw std::runtime_error("cannot write artifact rollback fixture");
    }
}

std::string readText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

bool pathExists(const std::filesystem::path& path)
{
    std::error_code error;
    const bool result = std::filesystem::exists(path, error);
    if (error)
    {
        throw std::runtime_error("cannot inspect artifact rollback fixture");
    }
    return result;
}

std::vector<std::filesystem::path> transactionResidues(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> result;
    std::error_code error;
    std::filesystem::directory_iterator cursor(directory, error);
    const std::filesystem::directory_iterator end;
    if (error)
    {
        throw std::runtime_error("cannot enumerate artifact rollback fixture");
    }
    while (cursor != end)
    {
        const std::wstring name = cursor->path().filename().native();
        if (name.find(L".nsc-transaction-") != std::wstring::npos)
        {
            result.push_back(cursor->path());
        }
        cursor.increment(error);
        if (error)
        {
            throw std::runtime_error("cannot continue artifact rollback enumeration");
        }
    }
    return result;
}

struct StageFixture
{
    std::string_view name;
    std::string_view bytes;
};

void prepare(
    nativesharp::basic::ArtifactTransaction& transaction,
    CaseTree& tree,
    const std::filesystem::path& destination,
    const StageFixture& fixture,
    std::string& error)
{
    const auto stage = tree.path(fixture.name);
    writeText(stage, fixture.bytes);
    if (!transaction.addStaged(destination, stage, error))
    {
        throw std::runtime_error("cannot register artifact rollback stage: " + error);
    }
}

bool hasRollbackSuffix(const std::string& error, std::string_view suffix)
{
    return error.ends_with(suffix) && error.find("artifact commit failed for '") == 0 &&
           error.find(": file-locked (Win32 error 32)") != std::string::npos;
}

void testSecondDestinationBlocked(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-073");
    const auto first = tree.path("first.lib");
    const auto second = tree.path("second.lib");
    writeText(first, "first original");
    writeText(second, "second original");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    prepare(transaction, tree, first, {"first-stage.lib", "first new"}, error);
    prepare(transaction, tree, second, {"second-stage.lib", "second new"}, error);
    artifact::ScopedMoveFailure failure({3});
    const bool committed = transaction.commit(error);
    const bool restored = failure.restore();
    context.expect(restored, "MoveFileExW import must be restored");
    context.expect(!committed, "blocked second destination must fail commit");
    context.expect(failure.observedFailureCount() == 1, "the third move must be rejected once");
    context.expect(hasRollbackSuffix(error, "; rollback-complete"), "rollback error must be exact");
    context.expect(readText(first) == "first original", "first original must be restored");
    context.expect(readText(second) == "second original", "second original must remain unchanged");
    context.expect(transactionResidues(tree.root()).empty(), "complete rollback must remove all residues");
}

void testFirstInstallBlocked(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-074");
    const auto first = tree.path("first.lib");
    const auto second = tree.path("second.lib");
    writeText(first, "first original");
    writeText(second, "second original");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    prepare(transaction, tree, first, {"first-stage.lib", "first new"}, error);
    prepare(transaction, tree, second, {"second-stage.lib", "second new"}, error);
    artifact::ScopedMoveFailure failure({1});
    const bool committed = transaction.commit(error);
    const bool restored = failure.restore();
    context.expect(restored, "MoveFileExW import must be restored");
    context.expect(!committed, "blocked first backup must fail commit");
    context.expect(failure.callCount() == 1, "later entries must not enter installation");
    context.expect(hasRollbackSuffix(error, "; rollback-complete"), "empty rollback must be complete");
    context.expect(readText(first) == "first original", "first destination must remain unchanged");
    context.expect(readText(second) == "second original", "later destination must remain unchanged");
    context.expect(transactionResidues(tree.root()).empty(), "failed first install must remove siblings");
}

void testRestoreFailure(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-075");
    const auto first = tree.path("first.lib");
    const auto second = tree.path("second.lib");
    writeText(first, "first original");
    writeText(second, "second original");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    prepare(transaction, tree, first, {"first-stage.lib", "first new"}, error);
    prepare(transaction, tree, second, {"second-stage.lib", "second new"}, error);
    artifact::ScopedMoveFailure failure({3, 4});
    const bool committed = transaction.commit(error);
    const bool restored = failure.restore();
    context.expect(restored, "MoveFileExW import must be restored");
    context.expect(!committed, "restore failure must leave commit failed");
    context.expect(failure.observedFailureCount() == 2, "commit and restoration moves must both fail");
    context.expect(hasRollbackSuffix(error, "; rollback-incomplete"), "restore failure suffix must be exact");
    context.expect(!pathExists(first), "failed restoration must leave the installed destination absent");
    context.expect(readText(second) == "second original", "unprocessed destination must remain unchanged");
    const auto residues = transactionResidues(tree.root());
    context.expect(residues.size() == 1, "failed restoration must preserve exactly one backup");
    if (residues.size() == 1)
    {
        tree.track(residues.front());
        context.expect(readText(residues.front()) == "first original", "preserved backup must keep prior bytes");
    }
}

void testAbsentDestinationRemoved(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-076");
    const auto first = tree.path("new-first.lib");
    const auto second = tree.path("existing-second.lib");
    writeText(second, "second original");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    prepare(transaction, tree, first, {"first-stage.lib", "first new"}, error);
    prepare(transaction, tree, second, {"second-stage.lib", "second new"}, error);
    artifact::ScopedMoveFailure failure({2});
    const bool committed = transaction.commit(error);
    const bool restored = failure.restore();
    context.expect(restored, "MoveFileExW import must be restored");
    context.expect(!committed, "later failure must reject mixed commit");
    context.expect(hasRollbackSuffix(error, "; rollback-complete"), "mixed rollback must be complete");
    context.expect(!pathExists(first), "new destination installed before failure must be removed");
    context.expect(readText(second) == "second original", "blocked existing destination must remain intact");
    context.expect(transactionResidues(tree.root()).empty(), "mixed rollback must remove all residues");
}

void testReverseRollbackOrder(TestContext& context)
{
    const ADriveTemporaryEnvironment temporary;
    CaseTree tree("TB-BAS-077");
    const auto first = tree.path("first.lib");
    const auto second = tree.path("second.lib");
    const auto third = tree.path("third.lib");
    writeText(first, "first original");
    writeText(second, "second original");
    writeText(third, "third original");
    nativesharp::basic::ArtifactTransaction transaction;
    std::string error;
    prepare(transaction, tree, first, {"first-stage.lib", "first new"}, error);
    prepare(transaction, tree, second, {"second-stage.lib", "second new"}, error);
    prepare(transaction, tree, third, {"third-stage.lib", "third new"}, error);
    artifact::ScopedMoveFailure failure({5});
    const bool committed = transaction.commit(error);
    const bool restored = failure.restore();
    context.expect(restored, "MoveFileExW import must be restored");
    context.expect(!committed, "third-entry failure must reject commit");
    context.expect(failure.callCount() == 7, "two installed entries must require two restore moves");
    context.expect(failure.destinationBasename(6) == L"second.lib", "second entry must restore first");
    context.expect(failure.destinationBasename(7) == L"first.lib", "first entry must restore last");
    context.expect(hasRollbackSuffix(error, "; rollback-complete"), "reverse rollback must be complete");
    context.expect(readText(first) == "first original", "first original must be restored");
    context.expect(readText(second) == "second original", "second original must be restored");
    context.expect(readText(third) == "third original", "third original must remain unchanged");
    context.expect(transactionResidues(tree.root()).empty(), "reverse rollback must remove all residues");
}
}

// Registers every CB-BAS-014 rollback case without modifying product behavior.
void registerBasicArtifactRollbackModule(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-073", "NIGHTLY", testSecondDestinationBlocked);
    addCase(registry, "TB-BAS-074", "NIGHTLY", testFirstInstallBlocked);
    addCase(registry, "TB-BAS-075", "WEEKLY", testRestoreFailure);
    addCase(registry, "TB-BAS-076", "NIGHTLY", testAbsentDestinationRemoved);
    addCase(registry, "TB-BAS-077", "NIGHTLY", testReverseRollbackOrder);
}
}
