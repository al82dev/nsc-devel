#include "nsc_test/Modules.h"

#include "nsc_test/TestContext.h"
#include "nsc_test/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticId.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"
#include "nativesharp/Source/SourceManager.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace nsc::test
{
namespace
{
std::string readText(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void verifyDiagnostics(TestContext& context)
{
    using namespace nativesharp;
    using namespace nativesharp::diagnostics;

    source::SourceManager sources;
    const std::filesystem::path fixture =
        std::filesystem::path(NSC_TEST_CASES_DIR) / "unit-data/diagnostics/diag.ns";
    const FileId id = sources.addBuffer("diag.ns", readText(fixture));

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

    context.expect(engine.errorCount() == 1, "error count is tracked");
    context.expect(engine.warningCount() == 1, "warning count is tracked");
    context.expect(engine.hasErrors(), "hasErrors reflects error diagnostics");
    context.expect(engine.diagnostics().size() == 3, "all diagnostics are retained");

    DiagnosticEngine merged;
    for (const Diagnostic& diagnostic : engine.diagnostics()) merged.append(diagnostic);
    context.expect(
        merged.errorCount() == 1 && merged.warningCount() == 1,
        "appended task diagnostics preserve severity counts");
    context.expect(
        merged.diagnostics().size() == 3 &&
            merged.diagnostics()[0].message() == "unexpected character '@'" &&
            merged.diagnostics()[2].message() == "example note",
        "appended task diagnostics preserve stable local order");

    DiagnosticEngine late(2);
    late.report(
        DiagnosticId::UnexpectedToken,
        DiagnosticSeverity::Error,
        SourceLocation(id, 24),
        "late error");
    late.report(
        DiagnosticId::ExpectedToken,
        DiagnosticSeverity::Error,
        SourceLocation(id, 26),
        "last retained error");
    late.report(
        DiagnosticId::ExpectedExpression,
        DiagnosticSeverity::Error,
        SourceLocation(id, 28),
        "locally suppressed error");
    DiagnosticEngine early(2);
    early.report(
        DiagnosticId::UnknownIdentifier,
        DiagnosticSeverity::Error,
        SourceLocation(id, 4),
        "early error");
    early.report(
        DiagnosticId::InvalidType,
        DiagnosticSeverity::Error,
        SourceLocation(id, 14),
        "middle error");

    DiagnosticEngine forward(3);
    const std::vector<DiagnosticEngine> forwardShards = {late, early};
    forward.mergeDeterministically(forwardShards);
    DiagnosticEngine reverse(3);
    const std::vector<DiagnosticEngine> reverseShards = {early, late};
    reverse.mergeDeterministically(reverseShards);
    context.expect(
        forward.diagnostics().size() == 3 && reverse.diagnostics().size() == 3,
        "parallel diagnostic reduction applies one global error limit");
    context.expect(
        forward.diagnostics()[0].message() == "early error" &&
            forward.diagnostics()[1].message() == "middle error" &&
            forward.diagnostics()[2].message() == "late error",
        "parallel diagnostics publish in canonical source order");
    bool sameParallelOrder = forward.diagnostics().size() == reverse.diagnostics().size();
    for (std::size_t index = 0;
         sameParallelOrder && index < forward.diagnostics().size();
         ++index)
    {
        sameParallelOrder =
            forward.diagnostics()[index].id() == reverse.diagnostics()[index].id() &&
            forward.diagnostics()[index].location() == reverse.diagnostics()[index].location() &&
            forward.diagnostics()[index].message() == reverse.diagnostics()[index].message();
    }
    context.expect(
        sameParallelOrder,
        "reversing worker completion order preserves exact diagnostics");
    context.expect(
        forward.errorCount() == 3 && forward.errorLimitReached(),
        "canonical reduction retains exact error-budget state");
    context.expect(
        forward.suppressedDiagnosticCount() == 2 &&
            reverse.suppressedDiagnosticCount() == 2,
        "global and worker-local suppression counts are both retained");

    std::ostringstream output;
    DiagnosticRenderer renderer(sources);
    renderer.renderAll(engine, output);
    const std::string text = output.str();
    context.expect(
        text.find("diag.ns:3:12: error: unexpected character '@'") != std::string::npos,
        "renderer emits file, line, column, severity, and message");
    context.expect(text.find("return @;") != std::string::npos, "renderer emits source line");
    context.expect(text.find('^') != std::string::npos, "renderer emits caret marker");
    context.expect(
        text.find("warning: example warning") != std::string::npos,
        "renderer supports warnings");
    context.expect(
        text.find("note: example note") != std::string::npos,
        "renderer supports notes");

    engine.clear();
    context.expect(engine.diagnostics().empty(), "clear removes retained diagnostics");
    context.expect(
        engine.errorCount() == 0 && engine.warningCount() == 0,
        "clear resets counts");

    const FileId tabId = sources.addBuffer("tabs.ns", "\tvalue;\nnext;\n");
    Diagnostic tabDiagnostic(
        DiagnosticId::UnexpectedToken,
        DiagnosticSeverity::Error,
        SourceLocation(tabId, 1),
        SourceRange(SourceLocation(tabId, 1), SourceLocation(tabId, 6)),
        "range after tab");
    std::ostringstream tabOutput;
    renderer.render(tabDiagnostic, tabOutput);
    context.expect(
        tabOutput.str().find("\t^~~~~") != std::string::npos,
        "renderer preserves tabs while positioning range marker");

    Diagnostic multiline(
        DiagnosticId::UnexpectedToken,
        DiagnosticSeverity::Error,
        SourceLocation(tabId, 1),
        SourceRange(SourceLocation(tabId, 1), SourceLocation(tabId, 10)),
        "multiline range");
    std::ostringstream multilineOutput;
    renderer.render(multiline, multilineOutput);
    context.expect(
        multilineOutput.str().find('^') != std::string::npos,
        "multiline ranges render a stable marker at their starting location");
}
}

void registerDiagnosticsUnitTests(TestRegistry& registry)
{
    registry.add(TestCase{
        "nsc.diagnostics",
        "diagnostics",
        "diagnostic engine and renderer",
        {"engine", "renderer", "ordering"},
        verifyDiagnostics,
    });
}
}
