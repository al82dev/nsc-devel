#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/ArtifactTransaction.h"

#include "../support/WriterCaseTree.h"
#include "../support/WriterSeedFailure.h"
#include "../support/WriterTemporaryEnvironment.h"
#include "../support/WriterWin32Handle.h"
#include "../support/WriterWin32Path.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
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
        "CB-BAS-016",
        "Host.Basic",
        "component",
        std::move(profile),
        {"basic", "artifact-transaction", "seed", "windows"},
        std::move(function),
    });
}

void writeBytes(const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    writer::Win32Handle file(CreateFileW(
        writer::extendedPath(path).c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (file.get() == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("cannot create artifact-seed fixture");
    }
    DWORD written = 0;
    const BOOL wrote = bytes.empty() || WriteFile(
        file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    if (!wrote || written != bytes.size() || !FlushFileBuffers(file.get()))
    {
        throw std::runtime_error("cannot finish artifact-seed fixture");
    }
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    writer::Win32Handle file(CreateFileW(
        writer::extendedPath(path).c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (file.get() == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("cannot open artifact-seed fixture");
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart < 0 || size.QuadPart > 1024LL * 1024LL)
    {
        throw std::runtime_error("invalid artifact-seed fixture size");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL readOk = bytes.empty() || ReadFile(
        file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    if (!readOk || read != bytes.size())
    {
        throw std::runtime_error("cannot read artifact-seed fixture");
    }
    return bytes;
}

std::string utf8Path(const std::filesystem::path& path)
{
    const std::u8string encoded = path.u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

void testMissingSource(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-083");
    const auto source = tree.path("missing.bin");
    const auto stage = tree.path("cold-stage.bin");
    std::string error = "stale";
    const bool seeded = nativesharp::basic::seedArtifactStage(source, stage, error);
    context.expect(seeded, "missing source must be a successful cold path");
    context.expect(error.empty(), "missing source must clear stale errors");
    context.expect(!std::filesystem::exists(stage), "missing source must not fabricate a stage");
}

void testRegularSource(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-084");
    const auto source = tree.path("source.bin");
    const auto stage = tree.path("stage.bin");
    const std::array<std::uint8_t, 7> expected{0x00, 0x23, 0x7f, 0x80, 0xff, 0x0a, 0x00};
    writeBytes(source, expected);
    std::string error;
    const bool seeded = nativesharp::basic::seedArtifactStage(source, stage, error);
    context.expect(seeded, "regular source must seed a stage");
    context.expect(error.empty(), "successful stage seeding must leave no error");
    context.expect(
        readBytes(stage) == std::vector(expected.begin(), expected.end()),
        "seeded stage must preserve every source byte");
}

void testEmptyPaths(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-085");
    const auto source = tree.path("source.bin");
    const auto stage = tree.path("stage.bin");
    const std::array<std::uint8_t, 1> bytes{0x42};
    writeBytes(source, bytes);
    std::string error;
    const bool emptySource = nativesharp::basic::seedArtifactStage({}, stage, error);
    context.expect(!emptySource, "empty source path must be rejected");
    context.expect(
        error == "artifact seed source or stage is empty",
        "empty source path must use the exact stable diagnostic");
    error = "stale";
    const bool emptyStage = nativesharp::basic::seedArtifactStage(source, {}, error);
    context.expect(!emptyStage, "empty stage path must be rejected");
    context.expect(
        error == "artifact seed source or stage is empty",
        "empty stage path must use the exact stable diagnostic");
    context.expect(!std::filesystem::exists(stage), "rejected empty paths must not publish a stage");
}

void testDirectorySource(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-086");
    const auto source = tree.path("source-directory");
    const auto stage = tree.path("stage.bin");
    if (!std::filesystem::create_directory(source))
    {
        throw std::runtime_error("cannot create artifact-seed source directory");
    }
    std::string error;
    const bool seeded = nativesharp::basic::seedArtifactStage(source, stage, error);
    context.expect(!seeded, "directory source must fail the Windows copy branch");
    context.expect(
        error.starts_with("artifact staging copy failed for '" + utf8Path(stage) + "': "),
        "directory source must report a classified stage-copy failure");
    context.expect(!std::filesystem::is_regular_file(stage), "directory source must not publish a stage file");
}

void testMissingStageDirectory(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-087");
    const auto source = tree.path("source.bin");
    const auto parent = tree.path("nested");
    const auto stage = tree.path("nested/stage.bin");
    const std::array<std::uint8_t, 5> expected{'s', 'e', 'e', 'd', 0x00};
    writeBytes(source, expected);
    std::string error;
    const bool seeded = nativesharp::basic::seedArtifactStage(source, stage, error);
    context.expect(seeded, "missing stage directory must be created before copying");
    context.expect(error.empty(), "successful nested stage seeding must leave no error");
    context.expect(std::filesystem::is_directory(parent), "missing stage parent must be created");
    context.expect(
        readBytes(stage) == std::vector(expected.begin(), expected.end()),
        "nested stage must preserve source bytes exactly");
}

void testClassifiedFailures(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-088");
    const std::array<std::uint8_t, 6> expected{'s', 'o', 'u', 'r', 'c', 'e'};

    const auto inspectionSource = tree.path("inspection-failure.bin");
    const auto inspectionStage = tree.path("inspection-stage.bin");
    writeBytes(inspectionSource, expected);
    std::string inspectionError;
    bool inspectionSeeded = false;
    bool inspectionObserved = false;
    bool inspectionRestored = false;
    {
        writer::ScopedSeedFailure failure(
            "GetFileAttributesW", L"inspection-failure.bin", ERROR_ACCESS_DENIED);
        inspectionSeeded = nativesharp::basic::seedArtifactStage(
            inspectionSource, inspectionStage, inspectionError);
        inspectionObserved = failure.observed();
        inspectionRestored = failure.restore();
    }
    context.expect(inspectionObserved, "source inspection must reach the process-local fault");
    context.expect(inspectionRestored, "source inspection import must be restored before assertions");
    context.expect(!inspectionSeeded, "source inspection failure must reject stage seeding");
    context.expect(
        inspectionError == "artifact inspection failed for '" + utf8Path(inspectionSource) +
            "': permission-denied (Win32 error 5)",
        "source inspection failure must report its path and classified Win32 error");
    context.expect(!std::filesystem::exists(inspectionStage), "inspection failure must not create a stage");

    const auto copySource = tree.path("copy-source.bin");
    const auto copyStage = tree.path("copy-stage.bin");
    writeBytes(copySource, expected);
    if (!std::filesystem::create_directory(copyStage))
    {
        throw std::runtime_error("cannot create artifact-seed copy-failure directory");
    }
    std::string copyError;
    const bool copySeeded = nativesharp::basic::seedArtifactStage(copySource, copyStage, copyError);
    context.expect(!copySeeded, "stage-directory collision must reject the copy");
    context.expect(
        copyError.starts_with("artifact staging copy failed for '" + utf8Path(copyStage) + "': "),
        "copy failure must report the stage path and a classified error");
    context.expect(std::filesystem::is_directory(copyStage), "copy failure must preserve the blocking directory");

    const auto flushSource = tree.path("flush-source.bin");
    const auto flushStage = tree.path("flush-failure.bin");
    writeBytes(flushSource, expected);
    std::string flushError;
    bool flushSeeded = false;
    bool flushObserved = false;
    bool flushRestored = false;
    {
        writer::ScopedSeedFailure failure(
            "FlushFileBuffers", L"flush-failure.bin", ERROR_WRITE_FAULT);
        flushSeeded = nativesharp::basic::seedArtifactStage(flushSource, flushStage, flushError);
        flushObserved = failure.observed();
        flushRestored = failure.restore();
    }
    context.expect(flushObserved, "stage flush must reach the process-local fault");
    context.expect(flushRestored, "stage flush import must be restored before assertions");
    context.expect(!flushSeeded, "stage flush failure must reject seeding");
    context.expect(
        flushError == "artifact staging flush failed for '" + utf8Path(flushStage) +
            "': io-error (Win32 error 29)",
        "stage flush failure must report its path and classified Win32 error");
    context.expect(
        readBytes(flushStage) == std::vector(expected.begin(), expected.end()),
        "flush failure must expose the copied stage bytes without claiming durable success");
}
}

void registerBasicArtifactSeedModule(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-083", "PR", testMissingSource);
    addCase(registry, "TB-BAS-084", "PR", testRegularSource);
    addCase(registry, "TB-BAS-085", "PR", testEmptyPaths);
    addCase(registry, "TB-BAS-086", "NIGHTLY", testDirectorySource);
    addCase(registry, "TB-BAS-087", "PR", testMissingStageDirectory);
    addCase(registry, "TB-BAS-088", "NIGHTLY", testClassifiedFailures);
}
}
