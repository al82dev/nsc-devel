#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/ArtifactTransaction.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

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
void addCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CB-BAS-011",
        "Host.Basic",
        "component",
        std::move(profile),
        {"basic", "artifact-transaction", "staging"},
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
        throw std::runtime_error("cannot read temporary-directory environment");
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
            throw std::runtime_error("artifact staging tests require an A:-drive working directory");
        }
        if (!SetEnvironmentVariableW(L"TEMP", root_.c_str()) ||
            !SetEnvironmentVariableW(L"TMP", root_.c_str()))
        {
            throw std::runtime_error("cannot redirect temporary artifact staging to A:");
        }
    }

    ADriveTemporaryEnvironment(const ADriveTemporaryEnvironment&) = delete;
    ADriveTemporaryEnvironment& operator=(const ADriveTemporaryEnvironment&) = delete;

    ~ADriveTemporaryEnvironment()
    {
        SetEnvironmentVariableW(L"TEMP", oldTemp_ ? oldTemp_->c_str() : nullptr);
        SetEnvironmentVariableW(L"TMP", oldTmp_ ? oldTmp_->c_str() : nullptr);
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept
    {
        return root_;
    }

private:
    std::optional<std::wstring> oldTemp_;
    std::optional<std::wstring> oldTmp_;
    std::filesystem::path root_;
};

class ExplicitPaths
{
public:
    ExplicitPaths() = default;
    ExplicitPaths(const ExplicitPaths&) = delete;
    ExplicitPaths& operator=(const ExplicitPaths&) = delete;

    ~ExplicitPaths()
    {
        std::error_code ignored;
        for (auto iterator = paths_.rbegin(); iterator != paths_.rend(); ++iterator)
        {
            std::filesystem::remove(*iterator, ignored);
            ignored.clear();
        }
    }

    void createFile(const std::filesystem::path& path)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.put('x');
        stream.close();
        if (!stream)
        {
            throw std::runtime_error("cannot create staging collision file: " + path.string());
        }
        paths_.push_back(path);
    }

private:
    std::vector<std::filesystem::path> paths_;
};

unsigned long long stagingOrdinal(const std::filesystem::path& stagingRoot)
{
    const std::string name = stagingRoot.filename().string();
    const std::size_t attemptSeparator = name.rfind('-');
    if (attemptSeparator == std::string::npos)
    {
        throw std::runtime_error("staging root lacks attempt suffix: " + name);
    }
    const std::size_t ordinalSeparator = name.rfind('-', attemptSeparator - 1);
    if (ordinalSeparator == std::string::npos)
    {
        throw std::runtime_error("staging root lacks ordinal suffix: " + name);
    }
    return std::stoull(name.substr(ordinalSeparator + 1, attemptSeparator - ordinalSeparator - 1));
}

std::filesystem::path stagingRootFrom(const std::filesystem::path& stage)
{
    return stage.parent_path().parent_path();
}
}

void registerBasicArtifactStagingModule(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-049", "PR", [](TestContext& context) {
        nativesharp::basic::ArtifactTransaction transaction;
        std::filesystem::path stage = L"unchanged";
        std::string error;
        const bool added = transaction.add({}, stage, error);
        context.expect(!added, "empty artifact destination must be rejected");
        context.expect(error == "artifact transaction destination is empty", "empty destination error must be exact");
        context.expect(stage == L"unchanged", "rejected add must not publish a stage path");
    });
    addCase(registry, "TB-BAS-050", "PR", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        nativesharp::basic::ArtifactTransaction transaction;
        const std::filesystem::path destination = temporary.root() / "published" / "product.dll";
        std::filesystem::path stage;
        std::string error;
        const bool added = transaction.add(destination, stage, error);
        context.expect(added, "first valid artifact must be staged");
        context.expect(error.empty(), "successful add must leave an empty error");
        context.expect(stage.filename() == destination.filename(), "stage must preserve the destination basename");
        context.expect(stage.parent_path().filename() == L"0", "first stage must use entry directory zero");
        context.expect(stagingRootFrom(stage).root_name() == L"A:", "staging root must remain on A:");
        context.expect(std::filesystem::is_directory(stage.parent_path()), "stage parent must exist");
    });
    addCase(registry, "TB-BAS-051", "PR", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        nativesharp::basic::ArtifactTransaction transaction;
        std::filesystem::path firstStage;
        std::filesystem::path secondStage;
        std::string error;
        const bool firstAdded = transaction.add(temporary.root() / "one" / "shared.lib", firstStage, error);
        const bool secondAdded = transaction.add(temporary.root() / "two" / "shared.lib", secondStage, error);
        context.expect(firstAdded && secondAdded, "same-basename destinations must both be staged");
        context.expect(firstStage != secondStage, "same-basename destinations must receive distinct stage paths");
        context.expect(firstStage.filename() == L"shared.lib", "first stage must preserve its basename");
        context.expect(secondStage.filename() == L"shared.lib", "second stage must preserve its basename");
        context.expect(firstStage.parent_path().filename() == L"0", "first stage must use entry directory zero");
        context.expect(secondStage.parent_path().filename() == L"1", "second stage must use entry directory one");
    });
    addCase(registry, "TB-BAS-052", "NIGHTLY", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        nativesharp::basic::ArtifactTransaction transaction;
        const std::filesystem::path destination = temporary.root() / L"publicación" / L"módulo-ñ.dll";
        std::filesystem::path stage;
        std::string error;
        const bool added = transaction.add(destination, stage, error);
        context.expect(added, "Unicode destination must be staged");
        context.expect(stage.filename() == destination.filename(), "Unicode stage must preserve the destination basename");
        context.expect(std::filesystem::is_directory(stage.parent_path()), "Unicode stage parent must exist");
    });
    addCase(registry, "TB-BAS-053", "NIGHTLY", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        nativesharp::basic::ArtifactTransaction probe;
        std::filesystem::path probeStage;
        std::string error;
        if (!probe.add(temporary.root() / "probe.lib", probeStage, error))
        {
            context.expect(false, "probe transaction must reveal the current staging ordinal");
            return;
        }
        const unsigned long long nextOrdinal = stagingOrdinal(stagingRootFrom(probeStage)) + 1ULL;
        const std::filesystem::path collision = temporary.root() /
            ("nsc-artifacts-" + std::to_string(GetCurrentProcessId()) + "-" +
             std::to_string(nextOrdinal) + "-0");
        ExplicitPaths fixtures;
        fixtures.createFile(collision);

        nativesharp::basic::ArtifactTransaction transaction;
        std::filesystem::path stage;
        error.clear();
        const bool added = transaction.add(temporary.root() / "after-collision.lib", stage, error);
        context.expect(added, "staging must retry after a root-name collision");
        context.expect(stagingRootFrom(stage) != collision, "retried staging must not reuse the occupied root");
        context.expect(stagingRootFrom(stage).filename().string().ends_with("-1"), "retry must advance the attempt suffix");
    });
    addCase(registry, "TB-BAS-054", "WEEKLY", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        nativesharp::basic::ArtifactTransaction probe;
        std::filesystem::path probeStage;
        std::string error;
        if (!probe.add(temporary.root() / "probe-exhaustion.lib", probeStage, error))
        {
            context.expect(false, "probe transaction must reveal the ordinal before exhaustion");
            return;
        }
        const unsigned long long firstBlockedOrdinal = stagingOrdinal(stagingRootFrom(probeStage)) + 1ULL;
        ExplicitPaths fixtures;
        for (unsigned attempt = 0; attempt < 100; ++attempt)
        {
            const std::filesystem::path collision = temporary.root() /
                ("nsc-artifacts-" + std::to_string(GetCurrentProcessId()) + "-" +
                 std::to_string(firstBlockedOrdinal + attempt) + "-" + std::to_string(attempt));
            fixtures.createFile(collision);
        }

        nativesharp::basic::ArtifactTransaction transaction;
        std::filesystem::path stage;
        error.clear();
        const bool added = transaction.add(temporary.root() / "exhausted.lib", stage, error);
        context.expect(!added, "staging must fail after all one hundred candidates are occupied");
        context.expect(stage.empty(), "exhausted staging must not publish a stage path");
        context.expect(
            error.starts_with("cannot create NativeSharp artifact staging directory:"),
            "exhausted staging must report directory creation failure");
    });
    addCase(registry, "TB-BAS-055", "PR", [](TestContext& context) {
        nativesharp::basic::ArtifactTransaction transaction;
        std::string error;
        const bool emptyDestination = transaction.addStaged({}, L"stage.lib", error);
        context.expect(!emptyDestination, "addStaged must reject an empty destination");
        context.expect(
            error == "artifact transaction destination or stage is empty",
            "empty destination error must be exact");
        error.clear();
        const bool emptyStage = transaction.addStaged(L"destination.lib", {}, error);
        context.expect(!emptyStage, "addStaged must reject an empty stage");
        context.expect(
            error == "artifact transaction destination or stage is empty",
            "empty stage error must be exact");
    });
    addCase(registry, "TB-BAS-056", "PR", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        const std::filesystem::path destination = temporary.root() / "deferred-destination.lib";
        const std::filesystem::path stage = temporary.root() / "deferred-stage.lib";
        nativesharp::basic::ArtifactTransaction transaction;
        std::string error = "stale error";
        const bool added = transaction.addStaged(destination, stage, error);
        context.expect(added, "addStaged must register non-empty paths without immediate existence checks");
        context.expect(error.empty(), "successful addStaged must clear stale errors");
        context.expect(!std::filesystem::exists(destination), "addStaged must not create the destination");
        context.expect(!std::filesystem::exists(stage), "addStaged must not create the stage");
    });
    addCase(registry, "TB-BAS-057", "NIGHTLY", [](TestContext& context) {
        const ADriveTemporaryEnvironment temporary;
        std::filesystem::path stagingRoot;
        {
            nativesharp::basic::ArtifactTransaction transaction;
            std::filesystem::path stage;
            std::string error;
            if (!transaction.add(temporary.root() / "abandoned.lib", stage, error))
            {
                context.expect(false, "abandoned transaction must create its stage");
                return;
            }
            stagingRoot = stagingRootFrom(stage);
            std::ofstream stream(stage, std::ios::binary | std::ios::trunc);
            stream.write("partial", 7);
            stream.close();
            context.expect(static_cast<bool>(stream), "abandoned stage fixture must be written");
            context.expect(std::filesystem::is_regular_file(stage), "abandoned stage must exist before destruction");
        }
        context.expect(!std::filesystem::exists(stagingRoot), "transaction destructor must remove uncommitted staging");
    });
}
}
