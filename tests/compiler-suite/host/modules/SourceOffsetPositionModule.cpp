#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Source/SourceFile.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
// Compares one byte offset against its one-based line and byte-column contract. The helper owns no state and
// deliberately delegates clamping and line lookup to the product method under test.
void expectPosition(
    TestContext& context,
    const nativesharp::source::SourceFile& file,
    std::string_view caseId,
    std::uint32_t offset,
    nativesharp::source::SourcePosition expected)
{
    const nativesharp::source::SourcePosition actual = file.positionForOffset(offset);
    context.expect(
        actual.line == expected.line,
        std::string(caseId) + " must resolve the expected one-based line");
    context.expect(
        actual.column == expected.column,
        std::string(caseId) + " must resolve the expected byte column");
}

// Creates a SourceFile with a stable identity and path so every case varies only the bytes and queried offset.
nativesharp::source::SourceFile makeFile(std::string buffer)
{
    return nativesharp::source::SourceFile(
        nativesharp::FileId{73}, std::filesystem::path(R"(A:\source\offset-position.ns)"), std::move(buffer));
}
}

// Registers the complete CS-SRC-004 matrix for byte offsets, terminators, EOF clamping, and UTF-8 byte columns.
void registerSourceOffsetPositionModule(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-019", "CS-SRC-004", "Host.Source", "unit", "PR", {"source", "position", "offset-zero"},
        [](TestContext& context) {
            const auto file = makeFile("abc");
            expectPosition(context, file, "TS-SRC-019", 0, {1, 1});
        },
    });
    registry.add(TestCase{
        "TS-SRC-020", "CS-SRC-004", "Host.Source", "unit", "PR", {"source", "position", "plain"},
        [](TestContext& context) {
            const auto file = makeFile("abc");
            expectPosition(context, file, "TS-SRC-020", 1, {1, 2});
            expectPosition(context, file, "TS-SRC-020", 2, {1, 3});
            expectPosition(context, file, "TS-SRC-020", 3, {1, 4});
        },
    });
    registry.add(TestCase{
        "TS-SRC-021", "CS-SRC-004", "Host.Source", "unit", "PR", {"source", "position", "lf-byte"},
        [](TestContext& context) {
            const auto file = makeFile("a\nb");
            expectPosition(context, file, "TS-SRC-021", 1, {1, 2});
        },
    });
    registry.add(TestCase{
        "TS-SRC-022", "CS-SRC-004", "Host.Source", "unit", "PR", {"source", "position", "after-lf"},
        [](TestContext& context) {
            const auto file = makeFile("a\nb");
            expectPosition(context, file, "TS-SRC-022", 2, {2, 1});
        },
    });
    registry.add(TestCase{
        "TS-SRC-023", "CS-SRC-004", "Host.Source", "unit", "PR", {"source", "position", "crlf"},
        [](TestContext& context) {
            const auto file = makeFile("a\r\nb");
            expectPosition(context, file, "TS-SRC-023", 1, {1, 2});
            expectPosition(context, file, "TS-SRC-023", 2, {1, 3});
            expectPosition(context, file, "TS-SRC-023", 3, {2, 1});
        },
    });
    registry.add(TestCase{
        "TS-SRC-024", "CS-SRC-004", "Host.Source", "unit", "PR", {"source", "position", "eof"},
        [](TestContext& context) {
            const auto file = makeFile("a\nb");
            expectPosition(context, file, "TS-SRC-024", 3, {2, 2});
        },
    });
    registry.add(TestCase{
        "TS-SRC-025", "CS-SRC-004", "Host.Source", "unit", "PR", {"source", "position", "clamped"},
        [](TestContext& context) {
            const auto file = makeFile("a\nb");
            expectPosition(context, file, "TS-SRC-025", 1000, {2, 2});
        },
    });
    registry.add(TestCase{
        "TS-SRC-026", "CS-SRC-004", "Host.Source", "unit", "PR", {"source", "position", "utf8-bytes"},
        [](TestContext& context) {
            const std::string bytes({static_cast<char>(0xc3), static_cast<char>(0xa9), 'x'});
            const auto file = makeFile(bytes);
            expectPosition(context, file, "TS-SRC-026", 2, {1, 3});
            expectPosition(context, file, "TS-SRC-026", 3, {1, 4});
        },
    });
}
}
