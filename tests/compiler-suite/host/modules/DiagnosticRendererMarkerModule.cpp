#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"

#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::diagnostics::Diagnostic;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::diagnostics::DiagnosticRenderer;
using nativesharp::diagnostics::DiagnosticSeverity;
using nativesharp::source::SourceManager;

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-010", "Host.Diagnostics", "component",
        std::move(profile), {"diagnostics", "renderer", "marker"}, std::move(function)});
}

std::string render(const SourceManager& sources, const Diagnostic& diagnostic)
{
    std::ostringstream output;
    DiagnosticRenderer(sources).render(diagnostic, output);
    return output.str();
}

Diagnostic makeDiagnostic(SourceLocation location, std::optional<SourceRange> range = std::nullopt)
{
    return Diagnostic(DiagnosticId::InvalidType, DiagnosticSeverity::Error,
        location, range, "marker");
}

void registerColumnOneCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-053", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("ascii.ns", "abcde");
        const std::string actual = render(sources, makeDiagnostic(SourceLocation(file, 0)));
        context.expect(actual == "ascii.ns:1:1: error: marker\n    abcde\n    ^\n",
            "TD-DIA-053 column one must render the indented line and one caret");
    });
}

void registerSpacePaddingCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-054", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("spaces.ns", "abcde");
        const std::string actual = render(sources, makeDiagnostic(SourceLocation(file, 3)));
        context.expect(actual.ends_with("    abcde\n       ^\n"),
            "TD-DIA-054 bytes before the caret must become spaces after the four-space indent");
    });
}

void registerTabPaddingCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-055", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("tab.ns", "\tvalue");
        const std::string actual = render(sources, makeDiagnostic(SourceLocation(file, 1)));
        context.expect(actual.ends_with("    \tvalue\n    \t^\n"),
            "TD-DIA-055 a source tab before the caret must be replicated without expansion");
    });
}

void registerUnitRangeCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-056", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("unit.ns", "abc");
        const SourceRange range(SourceLocation(file, 1), SourceLocation(file, 2));
        const std::string actual = render(sources, makeDiagnostic(range.begin(), range));
        context.expect(actual.ends_with("    abc\n     ^\n"),
            "TD-DIA-056 a one-byte same-line range must render only a caret");
    });
}

void registerWideRangeCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-057", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("wide.ns", "abcdef");
        const SourceRange range(SourceLocation(file, 1), SourceLocation(file, 5));
        const std::string actual = render(sources, makeDiagnostic(range.begin(), range));
        context.expect(actual.ends_with("    abcdef\n     ^~~~\n"),
            "TD-DIA-057 a four-byte same-line range must render caret plus three tildes");
    });
}

void registerEmptyRangeCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-058", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("empty-range.ns", "abcde");
        const SourceRange range(SourceLocation(file, 2), SourceLocation(file, 2));
        const std::string actual = render(sources, makeDiagnostic(range.begin(), range));
        context.expect(actual.ends_with("    abcde\n      ^\n"),
            "TD-DIA-058 an empty range must retain the minimum marker length of one");
    });
}

void registerMultilineRangeCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-059", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("multi.ns", "abc\ndef");
        const SourceRange range(SourceLocation(file, 1), SourceLocation(file, 5));
        const std::string actual = render(sources, makeDiagnostic(range.begin(), range));
        context.expect(actual.ends_with("    abc\n     ^\n"),
            "TD-DIA-059 a multiline range must retain a one-character marker");
    });
}

void registerPastVisibleTextCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-060", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("crlf.ns", "abc\r\n");
        const SourceRange range(SourceLocation(file, 0), SourceLocation(file, 4));
        const std::string actual = render(sources, makeDiagnostic(range.begin(), range));
        context.expect(actual.ends_with("    abc\n    ^~~~\n"),
            "TD-DIA-060 marker width must not clamp to the visible line length");
    });
}

void registerEmptyLineColumnOneCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-061", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("empty-line.ns", "\n");
        const std::string actual = render(sources, makeDiagnostic(SourceLocation(file, 0)));
        context.expect(actual.ends_with("    \n    ^\n"),
            "TD-DIA-061 an empty line at column one must still render line indent and caret");
    });
}

void registerEmptyLineLaterColumnCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-062", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("crlf-middle.ns", "\r\n");
        const SourceLocation location(file, 1);
        const std::string actual = render(sources, makeDiagnostic(location));
        context.expect(sources.getLineText(location).empty() && sources.getPosition(location).column == 2,
            "TD-DIA-062 the CRLF interior must reproduce an empty visible line at column two");
        context.expect(actual == "crlf-middle.ns:1:2: error: marker\n",
            "TD-DIA-062 the empty later-column path must return after the header");
    });
}

void registerIncoherentModelRiskCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-063", "NIGHTLY", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("incoherent.ns", "abc");
        const SourceLocation location(file, 2);
        const SourceRange range(SourceLocation(file, 0), SourceLocation(file, 1));
        const auto position = sources.getPosition(location);
        const auto end = sources.getPosition(range.end());
        context.expect(range.end().offset() > range.begin().offset() && end.line == position.line,
            "TD-DIA-063 the direct model must enter the same-line nonempty-range branch");
        context.expect(end.column < position.column,
            "TD-DIA-063 the direct model must characterize unsigned marker-width underflow without rendering");
    });
}
}

void registerDiagnosticRendererMarkerModule(TestRegistry& registry)
{
    registerColumnOneCase(registry);
    registerSpacePaddingCase(registry);
    registerTabPaddingCase(registry);
    registerUnitRangeCase(registry);
    registerWideRangeCase(registry);
    registerEmptyRangeCase(registry);
    registerMultilineRangeCase(registry);
    registerPastVisibleTextCase(registry);
    registerEmptyLineColumnOneCase(registry);
    registerEmptyLineLaterColumnCase(registry);
    registerIncoherentModelRiskCase(registry);
}
}
