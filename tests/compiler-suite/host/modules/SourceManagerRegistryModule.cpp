#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Source/SourceManager.h"
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

}

// Registers the bounded executable CS-SRC-007 cases.
void registerSourceManagerRegistryModule(TestRegistry& registry)
{
    registerConsecutiveIdentityCase(registry);
    registerDuplicatePathCase(registry);
    registerInvalidIdentityCase(registry);
    registerValidLookupCase(registry);
}
}
