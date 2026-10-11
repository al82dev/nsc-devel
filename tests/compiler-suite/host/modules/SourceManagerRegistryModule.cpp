#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Source/SourceManager.h"
#include "../support/AsanSubprocess.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace nsc::suite
{
namespace
{
// Registers consecutive identities and verifies that each lookup recovers its own immutable snapshot.
void registerConsecutiveIdentityCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-045", "CS-SRC-007", "Host.Source", "component", "PR", {"source", "manager", "identity"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            const auto firstId = manager.addBuffer("first.ns", "first");
            const auto secondId = manager.addBuffer("second.ns", "second");
            const auto* first = manager.getFile(firstId);
            const auto* second = manager.getFile(secondId);
            context.expect(firstId == nativesharp::FileId{1}, "TS-SRC-045 must assign ID one first");
            context.expect(secondId == nativesharp::FileId{2}, "TS-SRC-045 must assign ID two second");
            context.expect(first && first->buffer() == "first", "TS-SRC-045 must recover the first snapshot");
            context.expect(second && second->buffer() == "second", "TS-SRC-045 must recover the second snapshot");
        },
    });
}

// Registers duplicate display paths and proves that SourceManager performs no path deduplication.
void registerDuplicatePathCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-046", "CS-SRC-007", "Host.Source", "component", "PR", {"source", "manager", "duplicate"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            const std::filesystem::path path = R"(A:\source\duplicate.ns)";
            const auto firstId = manager.addBuffer(path, "first");
            const auto secondId = manager.addBuffer(path, "second");
            const auto* first = manager.getFile(firstId);
            const auto* second = manager.getFile(secondId);
            context.expect(firstId != secondId, "TS-SRC-046 must assign distinct IDs to duplicate paths");
            context.expect(first && second && first != second, "TS-SRC-046 must publish distinct source objects");
            context.expect(first && first->path() == path, "TS-SRC-046 must preserve the first duplicate path");
            context.expect(second && second->path() == path, "TS-SRC-046 must preserve the second duplicate path");
        },
    });
}

// Registers invalid identity probes at zero and immediately beyond the populated range.
void registerInvalidIdentityCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-047", "CS-SRC-007", "Host.Source", "component", "PR", {"source", "manager", "invalid-id"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            manager.addBuffer("only.ns", "only");
            context.expect(manager.getFile(nativesharp::FileId{}) == nullptr, "TS-SRC-047 must reject ID zero");
            context.expect(
                manager.getFile(nativesharp::FileId{2}) == nullptr,
                "TS-SRC-047 must reject an ID above the current count");
        },
    });
}

// Registers a valid lookup and verifies the returned object's complete identity, path, and byte contract.
void registerValidLookupCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-048", "CS-SRC-007", "Host.Source", "component", "PR", {"source", "manager", "lookup"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            const std::filesystem::path path = R"(A:\source\lookup.ns)";
            const std::string bytes({'x', '\0', 'y'});
            const auto id = manager.addBuffer(path, bytes);
            const auto* file = manager.getFile(id);
            context.expect(file != nullptr, "TS-SRC-048 must return a pointer for a valid ID");
            if (file != nullptr)
            {
                context.expect(file->id() == id, "TS-SRC-048 must preserve the exact identity");
                context.expect(file->path() == path, "TS-SRC-048 must preserve the exact path");
                context.expect(file->buffer() == bytes, "TS-SRC-048 must preserve the exact bytes");
            }
        },
    });
}

// A future versioned ASan profile may opt into the isolated invalidated-reference probe. Merely
// compiling this ordinary module with ASan must not publish TS-SRC-049.
#if defined(NSC_SOURCE_ASAN_TESTS_ENABLED)
void registerInvalidatedReferenceCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-049",
        "CS-SRC-007",
        "Host.Source",
        "component",
        "NIGHTLY",
        {"source", "manager", "lifetime", "asan", "subprocess"},
        [](TestContext& context) {
            if (source_test_support::isAsanProbeProcess())
            {
                nativesharp::source::SourceManager manager;
                const nativesharp::FileId firstId = manager.addBuffer("retained.ns", "retained");
                const nativesharp::source::SourceFile* retained = manager.getFile(firstId);
                bool reallocated = false;
                for (std::uint32_t index = 0; index < 4096 && !reallocated; ++index)
                {
                    manager.addBuffer(
                        "growth-" + std::to_string(index) + ".ns", std::string(64, 'g'));
                    reallocated = manager.getFile(firstId) != retained;
                }
                if (!reallocated)
                {
                    std::fputs("TS-SRC-049: vector did not reallocate\n", stderr);
                    std::fflush(stderr);
                    ExitProcess(96);
                }
                std::fputs("TS-SRC-049: probing invalidated SourceFile pointer\n", stderr);
                std::fflush(stderr);
                volatile std::uint32_t observed = retained->size();
                (void)observed;
                std::fputs("TS-SRC-049: ASan did not terminate the invalid access\n", stderr);
                std::fflush(stderr);
                ExitProcess(97);
            }

            const source_test_support::AsanSubprocessResult probe =
                source_test_support::runAsanProbe("TS-SRC-049");
            context.expect(probe.launched && probe.error.empty(),
                "TS-SRC-049 must launch the isolated sanitizer probe successfully");
            context.expect(!probe.timedOut,
                "TS-SRC-049 sanitizer probe must terminate within the controlled timeout");
            context.expect(probe.exitCode != 0,
                "TS-SRC-049 invalidated-reference access must terminate the child unsuccessfully");
            context.expect(probe.output.find("AddressSanitizer") != std::string::npos &&
                    probe.output.find("heap-use-after-free") != std::string::npos,
                "TS-SRC-049 ASan must diagnose the invalidated SourceFile access as heap-use-after-free");
        },
        TestDisposition::Required,
    });
}
#endif
}

// Registers bounded CS-SRC-007 cases and only the explicitly enabled future sanitizer contract.
void registerSourceManagerRegistryModule(TestRegistry& registry)
{
    registerConsecutiveIdentityCase(registry);
    registerDuplicatePathCase(registry);
    registerInvalidIdentityCase(registry);
    registerValidLookupCase(registry);
#if defined(NSC_SOURCE_ASAN_TESTS_ENABLED)
    registerInvalidatedReferenceCase(registry);
#endif
}
}
