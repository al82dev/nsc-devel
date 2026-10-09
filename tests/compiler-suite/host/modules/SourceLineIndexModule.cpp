#include "nsc_suite/TestRegistry.h"

#include "nativesharp/Basic/FileId.h"
#include "nativesharp/Source/SourceFile.h"

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
// Verifies private line-start state through its two public observables: the line count and offset lookup. Each
// expected start must resolve to column one, and the preceding byte must still belong to the previous line.
void expectLineStarts(
    TestContext& context,
    std::string_view caseId,
    std::string buffer,
    std::initializer_list<std::uint32_t> expectedStarts)
{
    const nativesharp::source::SourceFile file(
        nativesharp::FileId{71}, std::filesystem::path(R"(A:\source\line-index.ns)"), std::move(buffer));
    context.expect(
        file.lineCount() == static_cast<std::uint32_t>(expectedStarts.size()),
        std::string(caseId) + " must expose the expected line count");

    std::uint32_t oneBasedLine = 1;
    for (const std::uint32_t start : expectedStarts)
    {
        const nativesharp::source::SourcePosition atStart = file.positionForOffset(start);
        context.expect(
            atStart.line == oneBasedLine && atStart.column == 1,
            std::string(caseId) + " must expose each expected line start as column one");
        if (start != 0)
        {
            const nativesharp::source::SourcePosition beforeStart = file.positionForOffset(start - 1);
            context.expect(
                beforeStart.line == oneBasedLine - 1,
                std::string(caseId) + " must keep the preceding byte on the previous line");
        }
        ++oneBasedLine;
    }
}
}

// Registers the complete bounded matrix for iterative CR, LF, and CRLF line indexing.
void registerSourceLineIndexModule(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-009", "CS-SRC-003", "Host.Source", "unit", "PR", {"source", "line-index", "empty"},
        [](TestContext& context) { expectLineStarts(context, "TS-SRC-009", "", {0}); },
    });
    registry.add(TestCase{
        "TS-SRC-010", "CS-SRC-003", "Host.Source", "unit", "PR", {"source", "line-index", "plain"},
        [](TestContext& context) { expectLineStarts(context, "TS-SRC-010", "abc", {0}); },
    });
    registry.add(TestCase{
        "TS-SRC-011", "CS-SRC-003", "Host.Source", "unit", "PR", {"source", "line-index", "lf"},
        [](TestContext& context) { expectLineStarts(context, "TS-SRC-011", "a\nb", {0, 2}); },
    });
    registry.add(TestCase{
        "TS-SRC-012", "CS-SRC-003", "Host.Source", "unit", "PR", {"source", "line-index", "cr"},
        [](TestContext& context) { expectLineStarts(context, "TS-SRC-012", "a\rb", {0, 2}); },
    });
    registry.add(TestCase{
        "TS-SRC-013", "CS-SRC-003", "Host.Source", "unit", "PR", {"source", "line-index", "crlf"},
        [](TestContext& context) { expectLineStarts(context, "TS-SRC-013", "a\r\nb", {0, 3}); },
    });
    registry.add(TestCase{
        "TS-SRC-014", "CS-SRC-003", "Host.Source", "unit", "PR", {"source", "line-index", "terminal-lf"},
        [](TestContext& context) { expectLineStarts(context, "TS-SRC-014", "\n", {0, 1}); },
    });
    registry.add(TestCase{
        "TS-SRC-015", "CS-SRC-003", "Host.Source", "unit", "PR", {"source", "line-index", "terminal-cr"},
        [](TestContext& context) { expectLineStarts(context, "TS-SRC-015", "\r", {0, 1}); },
    });
    registry.add(TestCase{
        "TS-SRC-016", "CS-SRC-003", "Host.Source", "unit", "PR", {"source", "line-index", "terminal-crlf"},
        [](TestContext& context) { expectLineStarts(context, "TS-SRC-016", "\r\n", {0, 2}); },
    });
    registry.add(TestCase{
        "TS-SRC-017", "CS-SRC-003", "Host.Source", "unit", "PR", {"source", "line-index", "mixed"},
        [](TestContext& context) { expectLineStarts(context, "TS-SRC-017", "\n\r\r\n", {0, 1, 2, 4}); },
    });
    registry.add(TestCase{
        "TS-SRC-018", "CS-SRC-003", "Host.Source", "unit", "PR", {"source", "line-index", "binary"},
        [](TestContext& context) {
            const std::string bytes({'\0', static_cast<char>(0xff), 'x'});
            expectLineStarts(context, "TS-SRC-018", bytes, {0});
        },
    });
}
}
