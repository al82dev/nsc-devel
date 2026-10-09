#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/ArtifactTransaction.h"

#include "../support/WriterCaseTree.h"
#include "../support/WriterTemporaryEnvironment.h"
#include "../support/WriterWin32Handle.h"
#include "../support/WriterWin32Path.h"
#include "../support/WriterWriteFailure.h"

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
// Registers one CB-BAS-015 case in the consolidated host in O(1) time and storage.
void addCase(
    TestRegistry& registry,
    std::string id,
    std::string profile,
    TestFunction function)
{
    registry.add(TestCase{
        std::move(id),
        "CB-BAS-015",
        "Host.Basic",
        "component",
        std::move(profile),
        {"basic", "artifact-transaction", "writer", "windows"},
        std::move(function),
    });
}

// Writes a bounded fixture through an extended path and flushes it before returning.
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
        throw std::runtime_error("cannot create transactional-writer fixture");
    }
    DWORD written = 0;
    const BOOL wrote = bytes.empty() || WriteFile(
        file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    if (!wrote || written != bytes.size() || !FlushFileBuffers(file.get()))
    {
        throw std::runtime_error("cannot finish transactional-writer fixture");
    }
}

// Reads one bounded fixture through an extended path in O(file size) time and memory.
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
        throw std::runtime_error("cannot open transactional-writer fixture");
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) ||
        size.QuadPart < 0 ||
        size.QuadPart > 1024LL * 1024LL)
    {
        throw std::runtime_error("invalid transactional-writer fixture size");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size.QuadPart));
    DWORD read = 0;
    const BOOL readOk = bytes.empty() || ReadFile(
        file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
    if (!readOk || read != bytes.size())
    {
        throw std::runtime_error("cannot read transactional-writer fixture");
    }
    return bytes;
}

// Verifies publication of an empty byte span to a previously absent destination.
void testEmptyBytes(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-078");
    const auto destination = tree.path("empty.bin");
    std::string error;
    const bool written = nativesharp::basic::writeFileTransactionally(destination, {}, error);
    context.expect(written, "empty byte span must publish successfully");
    context.expect(error.empty(), "successful empty publication must leave no error");
    context.expect(readBytes(destination).empty(), "published empty destination must contain zero bytes");
}

// Verifies binary transparency, including embedded NUL and bytes outside ASCII.
void testBinaryBytes(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-079");
    const auto destination = tree.path("binary.bin");
    const std::array<std::uint8_t, 7> expected{0x00, 0x01, 0x7f, 0x80, 0xff, 0x00, 0x42};
    std::string error;
    const bool written = nativesharp::basic::writeFileTransactionally(destination, expected, error);
    context.expect(written, "binary byte span must publish successfully");
    context.expect(error.empty(), "successful binary publication must leave no error");
    context.expect(
        readBytes(destination) == std::vector(expected.begin(), expected.end()),
        "binary publication must not translate embedded bytes");
}

// Verifies that the text helper preserves the exact CR, LF, and UTF-8 byte sequence supplied by its view.
void testTextBytes(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-080");
    const auto destination = tree.path("text.txt");
    const std::string expected = "first\r\nl\xc3\xadnea\nlast\r";
    std::string error;
    const bool written = nativesharp::basic::writeTextTransactionally(destination, expected, error);
    context.expect(written, "text view must publish successfully");
    context.expect(error.empty(), "successful text publication must leave no error");
    context.expect(
        readBytes(destination) == std::vector<std::uint8_t>(expected.begin(), expected.end()),
        "text publication must preserve CR, LF, and UTF-8 bytes exactly");
}

// Verifies the stable open-stage diagnostic through a filename rejected by Win32 creation rules.
void testStageOpenFailure(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-081");
    const auto destination = tree.path("invalid?.bin");
    const std::array<std::uint8_t, 1> bytes{0x42};
    std::string error;
    const bool written = nativesharp::basic::writeFileTransactionally(destination, bytes, error);
    context.expect(!written, "invalid stage filename must fail before publication");
    context.expect(
        error.starts_with("cannot open staged artifact '"),
        "stage-open failure must use the stable diagnostic prefix");
}

// Verifies post-open write failure with a process-local import hook restored before assertions.
void testStageFinishFailure(TestContext& context)
{
    const writer::ADriveTemporaryEnvironment temporary;
    writer::CaseTree tree("TB-BAS-082");
    const auto destination = tree.path("finish-failure.bin");
    const std::array<std::uint8_t, 8> original{'o', 'r', 'i', 'g', 'i', 'n', 'a', 'l'};
    writeBytes(destination, original);
    const std::vector<std::uint8_t> replacement(std::size_t{64} * 1024U, 0x5a);
    std::string error;
    bool written = false;
    bool observed = false;
    bool restored = false;
    {
        writer::ScopedWriteFailure failure(L"finish-failure.bin");
        written = nativesharp::basic::writeFileTransactionally(destination, replacement, error);
        observed = failure.observed();
        restored = failure.restore();
    }
    context.expect(observed, "controlled stage write must reach the process-local fault");
    context.expect(restored, "process-local WriteFile import must be restored before assertions");
    context.expect(!written, "rejected stage write must fail before publication");
    context.expect(
        error.starts_with("cannot finish staged artifact '"),
        "stage write/close failure must use the stable diagnostic prefix");
    context.expect(
        readBytes(destination) == std::vector(original.begin(), original.end()),
        "failed staged write must preserve the prior destination bytes");
}
}

// Registers the complete single-file transactional-writer contract without product hooks.
void registerBasicTransactionalWriterModule(TestRegistry& registry)
{
    addCase(registry, "TB-BAS-078", "PR", testEmptyBytes);
    addCase(registry, "TB-BAS-079", "PR", testBinaryBytes);
    addCase(registry, "TB-BAS-080", "PR", testTextBytes);
    addCase(registry, "TB-BAS-081", "NIGHTLY", testStageOpenFailure);
    addCase(registry, "TB-BAS-082", "WEEKLY", testStageFinishFailure);
}
}
