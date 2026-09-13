#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"
#include "nativesharp/Source/SourceManager.h"

#include "../support/TestContext.h"

#include <sstream>
#include <string>

int main()
{
    using namespace nativesharp;
    using namespace nativesharp::diagnostics;
    using nativesharp::tests::TestContext;

    TestContext tests;
    source::SourceManager sources;
    const FileId id = sources.addBuffer("diag.ns", "int Main()\n{\n    return @;\n}\n");

    DiagnosticEngine engine;
    engine.report(
        DiagnosticId::UnexpectedCharacter,
        DiagnosticSeverity::Error,
        SourceRange(SourceLocation(id, 24), SourceLocation(id, 25)),
        "unexpected character '@'");
    engine.report(
        DiagnosticId::UnexpectedToken,
        DiagnosticSeverity::Warning,
        SourceLocation(id, 4),
        "example warning");
    engine.report(
        DiagnosticId::ExpectedToken,
        DiagnosticSeverity::Note,
        SourceLocation(id, 0),
        "example note");

    tests.expect(engine.errorCount() == 1, "error count is tracked");
    tests.expect(engine.warningCount() == 1, "warning count is tracked");
    tests.expect(engine.hasErrors(), "hasErrors reflects error diagnostics");
    tests.expect(engine.diagnostics().size() == 3, "all diagnostics are retained");

    DiagnosticEngine merged;
    for (const Diagnostic& diagnostic : engine.diagnostics()) merged.append(diagnostic);
    tests.expect(merged.errorCount() == 1 && merged.warningCount() == 1,
        "appended task diagnostics preserve severity counts");
    tests.expect(merged.diagnostics().size() == 3 &&
            merged.diagnostics()[0].message() == "unexpected character '@'" &&
            merged.diagnostics()[2].message() == "example note",
        "appended task diagnostics preserve stable local order");

    std::ostringstream output;
    DiagnosticRenderer renderer(sources);
    renderer.renderAll(engine, output);
    const std::string text = output.str();
    tests.expect(text.find("diag.ns:3:12: error: unexpected character '@'") != std::string::npos,
        "renderer emits file, line, column, severity, and message");
    tests.expect(text.find("return @;") != std::string::npos, "renderer emits source line");
    tests.expect(text.find('^') != std::string::npos, "renderer emits caret marker");
    tests.expect(text.find("warning: example warning") != std::string::npos, "renderer supports warnings");
    tests.expect(text.find("note: example note") != std::string::npos, "renderer supports notes");

    engine.clear();
    tests.expect(engine.diagnostics().empty(), "clear removes retained diagnostics");
    tests.expect(engine.errorCount() == 0 && engine.warningCount() == 0, "clear resets counts");

    const FileId tabId = sources.addBuffer("tabs.ns", "\tvalue;\nnext;\n");
    Diagnostic tabDiagnostic(
        DiagnosticId::UnexpectedToken,
        DiagnosticSeverity::Error,
        SourceLocation(tabId, 1),
        SourceRange(SourceLocation(tabId, 1), SourceLocation(tabId, 6)),
        "range after tab");
    std::ostringstream tabOutput;
    renderer.render(tabDiagnostic, tabOutput);
    tests.expect(tabOutput.str().find("\t^~~~~") != std::string::npos,
        "renderer preserves tabs while positioning range marker");

    Diagnostic multiline(
        DiagnosticId::UnexpectedToken,
        DiagnosticSeverity::Error,
        SourceLocation(tabId, 1),
        SourceRange(SourceLocation(tabId, 1), SourceLocation(tabId, 10)),
        "multiline range");
    std::ostringstream multilineOutput;
    renderer.render(multiline, multilineOutput);
    tests.expect(multilineOutput.str().find('^') != std::string::npos,
        "multiline ranges render a stable marker at their starting location");

    return tests.result();
}
