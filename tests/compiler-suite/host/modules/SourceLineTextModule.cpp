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
// Creates a small immutable source fixture. The fixed path and identity keep every case focused on line slicing.
nativesharp::source::SourceFile makeFile(std::string buffer)
{
    return nativesharp::source::SourceFile(
        nativesharp::FileId{79}, std::filesystem::path(R"(A:\source\line-text.ns)"), std::move(buffer));
}

// Compares a one-based line query with its exact byte slice without decoding or normalizing its content.
void expectLineText(
    TestContext& context,
    const nativesharp::source::SourceFile& file,
    std::string_view caseId,
    std::uint32_t oneBasedLine,
    std::string_view expected)
{
    context.expect(
        file.lineText(oneBasedLine) == expected,
        std::string(caseId) + " must return the expected line bytes without terminators");
}
}

// Registers the complete CS-SRC-005 matrix for invalid, populated, and empty logical line slices.
void registerSourceLineTextModule(TestRegistry& registry)
{
    registry.add(TestCase{
        "TS-SRC-027", "CS-SRC-005", "Host.Source", "unit", "PR", {"source", "line-text", "zero"},
        [](TestContext& context) {
            const auto file = makeFile("abc");
            expectLineText(context, file, "TS-SRC-027", 0, "");
        },
    });
    registry.add(TestCase{
        "TS-SRC-028", "CS-SRC-005", "Host.Source", "unit", "PR", {"source", "line-text", "past-end"},
        [](TestContext& context) {
            const auto file = makeFile("abc");
            expectLineText(context, file, "TS-SRC-028", 2, "");
        },
    });
    registry.add(TestCase{
        "TS-SRC-029", "CS-SRC-005", "Host.Source", "unit", "PR", {"source", "line-text", "plain"},
        [](TestContext& context) {
            const auto file = makeFile("abc");
            expectLineText(context, file, "TS-SRC-029", 1, "abc");
        },
    });
    registry.add(TestCase{
        "TS-SRC-030", "CS-SRC-005", "Host.Source", "unit", "PR", {"source", "line-text", "terminators"},
        [](TestContext& context) {
            const auto file = makeFile("lf\ncr\rcrlf\r\nlast");
            expectLineText(context, file, "TS-SRC-030", 1, "lf");
            expectLineText(context, file, "TS-SRC-030", 2, "cr");
            expectLineText(context, file, "TS-SRC-030", 3, "crlf");
            expectLineText(context, file, "TS-SRC-030", 4, "last");
        },
    });
    registry.add(TestCase{
        "TS-SRC-031", "CS-SRC-005", "Host.Source", "unit", "PR", {"source", "line-text", "empty-middle"},
        [](TestContext& context) {
            const auto file = makeFile("a\n\nb");
            context.expect(file.lineCount() == 3, "TS-SRC-031 must retain the empty intermediate line");
            expectLineText(context, file, "TS-SRC-031", 2, "");
        },
    });
    registry.add(TestCase{
        "TS-SRC-032", "CS-SRC-005", "Host.Source", "unit", "PR", {"source", "line-text", "empty-final"},
        [](TestContext& context) {
            const auto file = makeFile("a\n");
            context.expect(file.lineCount() == 2, "TS-SRC-032 must retain the empty final line");
            expectLineText(context, file, "TS-SRC-032", 2, "");
        },
    });
    registry.add(TestCase{
        "TS-SRC-033", "CS-SRC-005", "Host.Source", "unit", "PR", {"source", "line-text", "contiguous"},
        [](TestContext& context) {
            const auto file = makeFile("x\r\n\r\ny");
            context.expect(file.lineCount() == 3, "TS-SRC-033 must retain all consecutive logical boundaries");
            expectLineText(context, file, "TS-SRC-033", 1, "x");
            expectLineText(context, file, "TS-SRC-033", 2, "");
            expectLineText(context, file, "TS-SRC-033", 3, "y");
        },
    });
}
}
