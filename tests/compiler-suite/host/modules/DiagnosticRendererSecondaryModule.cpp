#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::SourceRange;
using nativesharp::diagnostics::Diagnostic;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::diagnostics::DiagnosticLabel;
using nativesharp::diagnostics::DiagnosticRenderer;
using nativesharp::diagnostics::DiagnosticSeverity;
using nativesharp::source::SourceManager;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-011", "Host.Diagnostics", "component",
        "PR", {"diagnostics", "renderer", "secondary"}, std::move(function)});
}

std::string render(const SourceManager& sources, const Diagnostic& diagnostic)
{
    std::ostringstream output;
    DiagnosticRenderer(sources).render(diagnostic, output);
    return output.str();
}

Diagnostic makeDiagnostic(
    SourceLocation location,
    std::vector<DiagnosticLabel> labels = {},
    std::vector<std::string> notes = {})
{
    return Diagnostic(DiagnosticId::InvalidType, DiagnosticSeverity::Error,
        location, std::nullopt, "primary", std::move(labels), std::move(notes));
}

void registerSameSourceLabelCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-064", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("same.ns", "abc\ndef");
        std::vector<DiagnosticLabel> labels{{
            SourceRange(SourceLocation(file, 4), SourceLocation(file, 7)), "secondary"}};
        const std::string actual = render(sources, makeDiagnostic(SourceLocation(file, 0), std::move(labels)));
        context.expect(actual ==
                "same.ns:1:1: error: primary\n    abc\n    ^\nsame.ns:2:1: note: secondary\n",
            "TD-DIA-064 a same-source label must use its own range begin and note severity");
    });
}

void registerOtherSourceLabelCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-065", [](TestContext& context) {
        SourceManager sources;
        const FileId primary = sources.addBuffer("primary.ns", "x");
        const FileId other = sources.addBuffer("other.ns", "  y");
        std::vector<DiagnosticLabel> labels{{
            SourceRange(SourceLocation(other, 2), SourceLocation(other, 3)), "other label"}};
        const std::string actual = render(sources, makeDiagnostic(SourceLocation(primary, 0), std::move(labels)));
        context.expect(actual.ends_with("other.ns:1:3: note: other label\n"),
            "TD-DIA-065 a label must resolve the path and position of its own source");
    });
}

void registerInvalidSourceLabelCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-066", [](TestContext& context) {
        SourceManager sources;
        const FileId primary = sources.addBuffer("primary.ns", "x");
        const SourceLocation invalid;
        std::vector<DiagnosticLabel> labels{{SourceRange(invalid, invalid), "missing source"}};
        const std::string actual = render(sources, makeDiagnostic(SourceLocation(primary, 0), std::move(labels)));
        context.expect(actual.ends_with("<source>:1:1: note: missing source\n"),
            "TD-DIA-066 an invalid label source must use the canonical source fallback");
    });
}

void registerStoredOrderCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-067", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("ordered.ns", "abcd");
        std::vector<DiagnosticLabel> labels{
            {SourceRange(SourceLocation(file, 2), SourceLocation(file, 3)), "label two"},
            {SourceRange(SourceLocation(file, 1), SourceLocation(file, 2)), "label one"}};
        std::vector<std::string> notes{"note two", "note one"};
        const std::string actual = render(sources,
            makeDiagnostic(SourceLocation(file, 0), std::move(labels), std::move(notes)));
        context.expect(actual.ends_with(
                "ordered.ns:1:3: note: label two\n"
                "ordered.ns:1:2: note: label one\n"
                "note: note two\n"
                "note: note one\n"),
            "TD-DIA-067 labels must precede notes while each vector retains insertion order");
    });
}

void registerNotesOnlyCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-068", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("notes.ns", "x");
        std::vector<std::string> notes{"first", "second: []!?"};
        const std::string actual = render(sources,
            makeDiagnostic(SourceLocation(file, 0), {}, std::move(notes)));
        context.expect(actual.ends_with("note: first\nnote: second: []!?\n"),
            "TD-DIA-068 notes without labels must retain the exact prefix, bytes, and order");
    });
}

void registerEarlyReturnCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-069", [](TestContext& context) {
        SourceManager sources;
        const FileId primary = sources.addBuffer("early.ns", "\r\n");
        const FileId secondary = sources.addBuffer("secondary.ns", "x");
        std::vector<DiagnosticLabel> labels{{
            SourceRange(SourceLocation(secondary, 0), SourceLocation(secondary, 1)), "omitted label"}};
        std::vector<std::string> notes{"omitted note"};
        const SourceLocation location(primary, 1);
        const std::string actual = render(sources,
            makeDiagnostic(location, std::move(labels), std::move(notes)));
        context.expect(sources.getLineText(location).empty() && sources.getPosition(location).column == 2,
            "TD-DIA-069 must reproduce the primary early-return condition");
        context.expect(actual == "early.ns:1:2: error: primary\n",
            "TD-DIA-069 the primary early return must omit all secondary labels and notes");
    });
}
}

void registerDiagnosticRendererSecondaryModule(TestRegistry& registry)
{
    registerSameSourceLabelCase(registry);
    registerOtherSourceLabelCase(registry);
    registerInvalidSourceLabelCase(registry);
    registerStoredOrderCase(registry);
    registerNotesOnlyCase(registry);
    registerEarlyReturnCase(registry);
}
}
