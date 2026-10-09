#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Basic/SourceLocation.h"
#include "nativesharp/Basic/SourceRange.h"
#include "nativesharp/Source/SourceManager.h"
#include "nativesharp/Source/SourcePosition.h"

#include <filesystem>
#include <string>

namespace nsc::suite
{
namespace
{
// Registers the complete sentinel contract for a location whose file identity is absent.
void registerInvalidLocationCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-050", "CS-SRC-008", "Host.Source", "component", "PR", {"source", "query", "invalid-file"},
        [](TestContext& context) {
            const nativesharp::source::SourceManager manager;
            const nativesharp::SourceLocation location;
            const nativesharp::source::SourcePosition position = manager.getPosition(location);
            context.expect(
                position.line == 1 && position.column == 1,
                "TS-SRC-050 must return the default 1:1 position");
            context.expect(manager.getLineText(location).empty(), "TS-SRC-050 must return an empty line view");
            context.expect(manager.getPath(location).empty(), "TS-SRC-050 must return an empty path view");
        },
    });
}

// Registers a valid file with an oversized offset and verifies clamping before line extraction.
void registerClampedLocationCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-051", "CS-SRC-008", "Host.Source", "component", "PR", {"source", "query", "clamp"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            const auto id = manager.addBuffer("clamp.ns", "a\nb");
            const nativesharp::SourceLocation location(id, 1000);
            const nativesharp::source::SourcePosition position = manager.getPosition(location);
            context.expect(
                position.line == 2 && position.column == 2,
                "TS-SRC-051 must clamp position to EOF");
            context.expect(manager.getLineText(location) == "b", "TS-SRC-051 must return the EOF line");
        },
    });
}

// Registers invalid default, cross-file, and reversed ranges against the empty-view sentinel.
void registerInvalidRangeCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-052", "CS-SRC-008", "Host.Source", "component", "PR", {"source", "query", "invalid-range"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            const auto first = manager.addBuffer("first.ns", "abc");
            const auto second = manager.addBuffer("second.ns", "xyz");
            const nativesharp::SourceRange crossFile(
                nativesharp::SourceLocation(first, 0), nativesharp::SourceLocation(second, 1));
            const nativesharp::SourceRange reversed(
                nativesharp::SourceLocation(first, 2), nativesharp::SourceLocation(first, 1));
            context.expect(manager.getText({}).empty(), "TS-SRC-052 must reject the default range");
            context.expect(manager.getText(crossFile).empty(), "TS-SRC-052 must reject a cross-file range");
            context.expect(manager.getText(reversed).empty(), "TS-SRC-052 must reject a reversed range");
        },
    });
}

// Registers a valid zero-width range and verifies an empty non-owning slice without out-of-bounds access.
void registerEmptyRangeCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-053", "CS-SRC-008", "Host.Source", "component", "PR", {"source", "query", "empty-range"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            const auto id = manager.addBuffer("empty-range.ns", "abc");
            const nativesharp::SourceRange range(
                nativesharp::SourceLocation(id, 1), nativesharp::SourceLocation(id, 1));
            context.expect(range.isValid(), "TS-SRC-053 fixture must be a valid range");
            context.expect(manager.getText(range).empty(), "TS-SRC-053 must return an empty slice");
        },
    });
}

// Registers the representative half-open range [1,4) and verifies exactly its three bytes.
void registerOrdinaryRangeCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-054", "CS-SRC-008", "Host.Source", "component", "PR", {"source", "query", "slice"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            const auto id = manager.addBuffer("slice.ns", "abcde");
            const nativesharp::SourceRange range(
                nativesharp::SourceLocation(id, 1), nativesharp::SourceLocation(id, 4));
            context.expect(manager.getText(range) == "bcd", "TS-SRC-054 must return bytes one through three");
        },
    });
}

// Registers a range ending exactly at file size and verifies that the boundary remains inclusive for end.
void registerExactEndRangeCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-055", "CS-SRC-008", "Host.Source", "component", "PR", {"source", "query", "exact-end"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            const auto id = manager.addBuffer("exact-end.ns", "abcde");
            const nativesharp::SourceRange range(
                nativesharp::SourceLocation(id, 2), nativesharp::SourceLocation(id, 5));
            context.expect(manager.getText(range) == "cde", "TS-SRC-055 must accept end at exact file size");
        },
    });
}

// Registers the first end offset beyond file size and verifies rejection without a partial slice.
void registerPastEndRangeCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-056", "CS-SRC-008", "Host.Source", "component", "PR", {"source", "query", "past-end"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            const auto id = manager.addBuffer("past-end.ns", "abcde");
            const nativesharp::SourceRange range(
                nativesharp::SourceLocation(id, 2), nativesharp::SourceLocation(id, 6));
            context.expect(manager.getText(range).empty(), "TS-SRC-056 must reject end beyond file size");
        },
    });
}

// Registers a second-line location after CRLF and verifies logical terminators are excluded.
void registerCrLfLineCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-057", "CS-SRC-008", "Host.Source", "component", "PR", {"source", "query", "crlf-line"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            const auto id = manager.addBuffer("lines.ns", "first\r\nsecond\r\nthird");
            context.expect(
                manager.getLineText(nativesharp::SourceLocation(id, 7)) == "second",
                "TS-SRC-057 must return the second line without CRLF");
        },
    });
}

// Registers a valid path query and proves its display spelling is owned independently of the caller path.
void registerPathSnapshotCase(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-058", "CS-SRC-008", "Host.Source", "component", "PR", {"source", "query", "path"},
        [](TestContext& context) {
            nativesharp::source::SourceManager manager;
            std::filesystem::path path = R"(A:\source\frozen-path.ns)";
            const std::string expected = path.string();
            const auto id = manager.addBuffer(path, "value");
            path = R"(A:\source\changed.ns)";
            context.expect(
                manager.getPath(nativesharp::SourceLocation(id, 999)) == expected,
                "TS-SRC-058 must return the frozen display path independently of offset");
        },
    });
}
}

// Registers the complete CS-SRC-008 location, range, line, and path query matrix.
void registerSourceManagerQueryModule(TestRegistry& registry)
{
    registerInvalidLocationCase(registry);
    registerClampedLocationCase(registry);
    registerInvalidRangeCase(registry);
    registerEmptyRangeCase(registry);
    registerOrdinaryRangeCase(registry);
    registerExactEndRangeCase(registry);
    registerPastEndRangeCase(registry);
    registerCrLfLineCase(registry);
    registerPathSnapshotCase(registry);
}
}
