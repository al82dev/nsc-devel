#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Source/SourceManager.h"
#include "../support/SourceReadMutation.h"
#include "../support/WriterCaseTree.h"
#include "../support/WriterWin32Handle.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

// Instantiate the canonical product body under a deterministic, thread-local stream facade.
// This keeps the production SourceManager binary and API untouched while the race fixtures gain
// exact coordination points around the real file_size/read/peek algorithm.
#define SourceManager CoordinatedSourceManager
#define std nsc_source_test_std
#include "../../../../src/Source/SourceManager.cpp"
#undef std
#undef SourceManager

namespace nsc::suite
{
namespace
{
constexpr std::uintmax_t kMaximumSourceBytes = std::uintmax_t{64} * 1024U * 1024U;

// Writes one exact binary fixture and rejects partial or deferred stream failures.
void writeBytes(const std::filesystem::path& path, std::string_view bytes)
{
    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream)
    {
        throw std::runtime_error("cannot write source-read fixture");
    }
}

// Creates one bounded zero-filled fixture without allocating a second in-memory copy.
void resizeFixture(const std::filesystem::path& path, std::uintmax_t size)
{
    writeBytes(path, {});
    std::error_code error;
    std::filesystem::resize_file(path, size, error);
    if (error)
    {
        throw std::runtime_error("cannot resize source-read fixture");
    }
}

#if !defined(NSC_SOURCE_EXTENDED_TESTS_ENABLED)
// Verifies the exact content of a sparse boundary fixture in one iterative pass.
bool containsOnlyZeroBytes(std::string_view bytes) noexcept
{
    for (const char byte : bytes)
    {
        if (byte != '\0')
        {
            return false;
        }
    }
    return true;
}

// Builds a process-unique named-pipe path whose stream opens but has no regular-file size.
std::filesystem::path namedPipePath(std::string_view caseId)
{
    return std::filesystem::path(
        L"\\\\.\\pipe\\nsc-suite-" + std::wstring(caseId.begin(), caseId.end()) + L"-" +
        std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
}

// Creates one outbound byte pipe for the size-query failure fixture.
writer::Win32Handle createNamedPipe(const std::filesystem::path& path)
{
    return writer::Win32Handle(CreateNamedPipeW(
        path.c_str(),
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
        1,
        1,
        1,
        0,
        nullptr));
}

// Registers the empty-file success case and its engaged empty optional contract.
void registerEmptyReadCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-034", "CS-SRC-006", "Host.Source", "component", "PR", {"source", "read", "empty"},
        [](TestContext& context) {
            writer::CaseTree tree("TS-SRC-034");
            const auto path = tree.path("empty.ns");
            writeBytes(path, {});
            std::string error;
            const auto buffer = nativesharp::source::SourceManager::readFileBuffer(path, error);
            context.expect(buffer.has_value(), "TS-SRC-034 must return an engaged buffer");
            context.expect(buffer && buffer->empty(), "TS-SRC-034 must preserve the empty byte sequence");
            context.expect(error.empty(), "TS-SRC-034 must not report an error");
        },
    });
}

// Registers the binary-file success case with embedded NUL and high-bit bytes.
void registerBinaryReadCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-035", "CS-SRC-006", "Host.Source", "component", "PR", {"source", "read", "binary"},
        [](TestContext& context) {
            writer::CaseTree tree("TS-SRC-035");
            const auto path = tree.path("binary.ns");
            const std::string bytes({'a', '\0', 'b', static_cast<char>(0xff), '\0'});
            writeBytes(path, bytes);
            std::string error;
            const auto buffer = nativesharp::source::SourceManager::readFileBuffer(path, error);
            context.expect(buffer.has_value(), "TS-SRC-035 must return an engaged buffer");
            context.expect(buffer && *buffer == bytes, "TS-SRC-035 must preserve the exact binary bytes");
            context.expect(error.empty(), "TS-SRC-035 must not report an error");
        },
    });
}

// Registers the inclusive 64 MiB boundary case without a second 64 MiB expectation buffer.
void registerInclusiveLimitCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-036", "CS-SRC-006", "Host.Source", "component", "WEEKLY", {"source", "read", "limit"},
        [](TestContext& context) {
            writer::CaseTree tree("TS-SRC-036");
            const auto path = tree.path("limit.ns");
            resizeFixture(path, kMaximumSourceBytes);
            std::string error;
            const auto buffer = nativesharp::source::SourceManager::readFileBuffer(path, error);
            context.expect(buffer.has_value(), "TS-SRC-036 must accept the inclusive 64 MiB limit");
            if (buffer)
            {
                context.expect(buffer->size() == kMaximumSourceBytes, "TS-SRC-036 must preserve the exact size");
                context.expect(containsOnlyZeroBytes(*buffer), "TS-SRC-036 must preserve every sparse zero byte");
            }
            context.expect(error.empty(), "TS-SRC-036 must not report an error");
        },
    });
}

// Registers the first byte beyond the 64 MiB safety boundary and its exact diagnostic.
void registerExceededLimitCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-037", "CS-SRC-006", "Host.Source", "component", "WEEKLY", {"source", "read", "limit"},
        [](TestContext& context) {
            writer::CaseTree tree("TS-SRC-037");
            const auto path = tree.path("over-limit.ns");
            resizeFixture(path, kMaximumSourceBytes + 1U);
            std::string error;
            const auto buffer = nativesharp::source::SourceManager::readFileBuffer(path, error);
            context.expect(!buffer.has_value(), "TS-SRC-037 must reject 64 MiB plus one byte");
            context.expect(
                error == "source file exceeds the 64 MiB safety limit",
                "TS-SRC-037 must report the exact safety-limit diagnostic");
        },
    });
}

// Registers the missing-file path and verifies that the emitted diagnostic includes its exact spelling.
void registerMissingFileCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-038", "CS-SRC-006", "Host.Source", "component", "PR", {"source", "read", "missing"},
        [](TestContext& context) {
            writer::CaseTree tree("TS-SRC-038");
            const auto path = tree.path("missing.ns");
            std::string error;
            const auto buffer = nativesharp::source::SourceManager::readFileBuffer(path, error);
            context.expect(!buffer.has_value(), "TS-SRC-038 must reject a missing path");
            context.expect(
                error == "cannot open source file '" + path.string() + "'",
                "TS-SRC-038 must report the exact missing path");
        },
    });
}

// Registers a named-pipe fixture that opens as a stream but cannot report regular-file size.
void registerUnknownSizeCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-039", "CS-SRC-006", "Host.Source", "component", "NIGHTLY", {"source", "read", "size"},
        [](TestContext& context) {
            const auto path = namedPipePath("TS-SRC-039");
            writer::Win32Handle pipe = createNamedPipe(path);
            context.expect(pipe.get() != INVALID_HANDLE_VALUE, "TS-SRC-039 must create the named-pipe fixture");
            if (pipe.get() == INVALID_HANDLE_VALUE)
            {
                return;
            }
            std::string error;
            const auto buffer = nativesharp::source::SourceManager::readFileBuffer(path, error);
            context.expect(!buffer.has_value(), "TS-SRC-039 must reject a path without a regular-file size");
            context.expect(
                error == "cannot determine source file size for '" + path.string() + "'",
                "TS-SRC-039 must report the exact size-query diagnostic");
        },
    });
}

// Registers the successful-read state case in which an existing diagnostic remains unchanged.
void registerPreservedErrorCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-042", "CS-SRC-006", "Host.Source", "component", "PR", {"source", "read", "error-state"},
        [](TestContext& context) {
            writer::CaseTree tree("TS-SRC-042");
            const auto path = tree.path("valid.ns");
            writeBytes(path, "value");
            std::string error = "prior diagnostic";
            const auto buffer = nativesharp::source::SourceManager::readFileBuffer(path, error);
            context.expect(buffer.has_value(), "TS-SRC-042 must return an engaged buffer");
            context.expect(buffer && *buffer == "value", "TS-SRC-042 must preserve the exact bytes");
            context.expect(error == "prior diagnostic", "TS-SRC-042 must preserve the prior error text");
        },
    });
}

// Registers the failed-load transaction and proves it consumes no manager identity.
void registerFailedLoadCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-043", "CS-SRC-006", "Host.Source", "component", "PR", {"source", "load", "rollback"},
        [](TestContext& context) {
            writer::CaseTree tree("TS-SRC-043");
            const auto path = tree.path("missing.ns");
            nativesharp::source::SourceManager manager;
            std::string error;
            const auto loaded = manager.loadFile(path, error);
            context.expect(!loaded.has_value(), "TS-SRC-043 must return nullopt after a read failure");
            const auto firstId = manager.addBuffer("after-failure.ns", "ok");
            context.expect(firstId == nativesharp::FileId{1}, "TS-SRC-043 must not consume an ID on failure");
        },
    });
}

// Registers the successful-load transaction and verifies the published path and byte snapshot.
void registerSuccessfulLoadCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-044", "CS-SRC-006", "Host.Source", "component", "PR", {"source", "load", "snapshot"},
        [](TestContext& context) {
            writer::CaseTree tree("TS-SRC-044");
            const auto path = tree.path("loaded.ns");
            writeBytes(path, "first\nsecond");
            nativesharp::source::SourceManager manager;
            std::string error;
            const auto loaded = manager.loadFile(path, error);
            context.expect(loaded == nativesharp::FileId{1}, "TS-SRC-044 must allocate the first source ID");
            const auto* file = loaded ? manager.getFile(*loaded) : nullptr;
            context.expect(file != nullptr, "TS-SRC-044 must publish the loaded source snapshot");
            if (file != nullptr)
            {
                context.expect(file->path() == path, "TS-SRC-044 must preserve the exact filesystem path");
                context.expect(file->buffer() == "first\nsecond", "TS-SRC-044 must preserve the exact bytes");
            }
            context.expect(error.empty(), "TS-SRC-044 must not report an error");
        },
    });
}
#endif

#if defined(NSC_SOURCE_EXTENDED_TESTS_ENABLED)
// Registers deterministic mutation cases against the canonical SourceManager.cpp algorithm.
void registerCoordinatedMutationCases(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-040",
        "CS-SRC-006",
        "Host.Source",
        "component",
        "NIGHTLY",
        {"source", "read", "race", "truncate", "coordinated"},
        [](TestContext& context) {
            writer::CaseTree tree("TS-SRC-040");
            const auto path = tree.path("truncate-after-size.ns");
            constexpr std::uintmax_t originalBytes = 8192;
            constexpr std::uintmax_t truncatedBytes = 1024;
            resizeFixture(path, originalBytes);
            source_test_support::SourceReadMutationPlan plan{
                path,
                source_test_support::SourceReadMutationMode::TruncateBeforeRead,
                originalBytes,
                truncatedBytes};
            std::string error;
            std::optional<std::string> buffer;
            {
                source_test_support::ScopedSourceReadMutation mutation(plan);
                buffer = nativesharp::source::CoordinatedSourceManager::readFileBuffer(path, error);
            }
            context.expect(plan.triggered && plan.error.empty(),
                "TS-SRC-040 must truncate exactly after file_size and before stream.read");
            context.expect(!buffer.has_value(),
                "TS-SRC-040 a short read must fail without publishing a partial buffer");
            context.expect(error == "cannot read source file '" + path.string() + "'",
                "TS-SRC-040 a short read must publish the exact read diagnostic");
        },
    });
    registry.add(TestCase{
        "TS-SRC-041",
        "CS-SRC-006",
        "Host.Source",
        "component",
        "NIGHTLY",
        {"source", "read", "race", "growth", "coordinated"},
        [](TestContext& context) {
            writer::CaseTree tree("TS-SRC-041");
            const auto path = tree.path("grow-before-peek.ns");
            constexpr std::uintmax_t originalBytes = 4096;
            resizeFixture(path, originalBytes);
            source_test_support::SourceReadMutationPlan plan{
                path,
                source_test_support::SourceReadMutationMode::GrowAfterRead,
                originalBytes,
                originalBytes + 1U};
            std::string error;
            std::optional<std::string> buffer;
            {
                source_test_support::ScopedSourceReadMutation mutation(plan);
                buffer = nativesharp::source::CoordinatedSourceManager::readFileBuffer(path, error);
            }
            context.expect(plan.triggered && plan.error.empty(),
                "TS-SRC-041 must append exactly after the sized read and before the final peek");
            context.expect(!buffer.has_value(),
                "TS-SRC-041 the final peek must reject growth without publishing a partial buffer");
            context.expect(error == "cannot read source file '" + path.string() + "'",
                "TS-SRC-041 post-size growth must publish the exact read diagnostic");
        },
    });
}
#endif
}

// Registers the deterministic CS-SRC-006 cases, including two coordinated mutation contracts.
void registerSourceFileReadModule(TestRegistry& registry)
{
#if defined(NSC_SOURCE_EXTENDED_TESTS_ENABLED)
    registerCoordinatedMutationCases(registry);
#else
    registerEmptyReadCase(registry);
    registerBinaryReadCase(registry);
    registerInclusiveLimitCase(registry);
    registerExceededLimitCase(registry);
    registerMissingFileCase(registry);
    registerUnknownSizeCase(registry);
    registerPreservedErrorCase(registry);
    registerFailedLoadCase(registry);
    registerSuccessfulLoadCase(registry);
#endif
}
}
