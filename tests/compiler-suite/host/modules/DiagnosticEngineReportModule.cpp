#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"

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
using nativesharp::diagnostics::DiagnosticEngine;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::diagnostics::DiagnosticLabel;
using nativesharp::diagnostics::DiagnosticSeverity;

// Registers one case under the accepted-report storage contract. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-004", "Host.Diagnostics", "component",
        "PR", {"diagnostics", "engine", "report"}, std::move(function)});
}

// Verifies an optional range after its presence has been checked. Complexity is O(1).
void expectRange(TestContext& context, const Diagnostic& diagnostic, SourceRange expected, std::string prefix)
{
    context.expect(diagnostic.range().has_value(), prefix + " must preserve a primary range");
    if (diagnostic.range())
    {
        context.expect(diagnostic.range()->begin() == expected.begin(),
            prefix + " must preserve the range beginning");
        context.expect(diagnostic.range()->end() == expected.end(),
            prefix + " must preserve the range end");
    }
}

void registerInitialStateCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-012", [](TestContext& context) {
        const DiagnosticEngine engine;
        context.expect(engine.diagnostics().empty(), "TD-DIA-012 a new engine must have no diagnostics");
        context.expect(engine.errorCount() == 0, "TD-DIA-012 a new engine must have zero errors");
        context.expect(engine.warningCount() == 0, "TD-DIA-012 a new engine must have zero warnings");
        context.expect(engine.lintCount() == 0, "TD-DIA-012 a new engine must have zero lints");
        context.expect(!engine.hasErrors(), "TD-DIA-012 a new engine must report no errors");
        context.expect(engine.errorLimit() == 100, "TD-DIA-012 the default error limit must be 100");
        context.expect(!engine.errorLimitReached(), "TD-DIA-012 the default limit must not be reached");
        context.expect(engine.suppressedDiagnosticCount() == 0,
            "TD-DIA-012 a new engine must have zero suppressed diagnostics");
    });
}

void registerErrorByLocationCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-013", [](TestContext& context) {
        DiagnosticEngine engine;
        const SourceLocation location(FileId(31), 17);
        engine.report(DiagnosticId::InvalidType, DiagnosticSeverity::Error, location, "invalid type here");
        context.expect(engine.diagnostics().size() == 1,
            "TD-DIA-013 one accepted report must append one record");
        if (!engine.diagnostics().empty())
        {
            const Diagnostic& diagnostic = engine.diagnostics().front();
            context.expect(diagnostic.id() == DiagnosticId::InvalidType,
                "TD-DIA-013 must preserve the diagnostic ID");
            context.expect(diagnostic.severity() == DiagnosticSeverity::Error,
                "TD-DIA-013 must preserve Error severity");
            context.expect(diagnostic.location() == location,
                "TD-DIA-013 must preserve the source location");
            context.expect(!diagnostic.range().has_value(),
                "TD-DIA-013 a location report must not synthesize a range");
            context.expect(diagnostic.message() == "invalid type here",
                "TD-DIA-013 must preserve the exact message");
        }
        context.expect(engine.errorCount() == 1 && engine.warningCount() == 0 && engine.lintCount() == 0,
            "TD-DIA-013 only the error counter must increment");
    });
}

void registerWarningByRangeCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-014", [](TestContext& context) {
        DiagnosticEngine engine;
        const SourceRange range(SourceLocation(FileId(32), 21), SourceLocation(FileId(32), 29));
        engine.report(DiagnosticId::InvalidConversion, DiagnosticSeverity::Warning, range, "lossy conversion");
        context.expect(engine.diagnostics().size() == 1,
            "TD-DIA-014 one accepted range report must append one record");
        if (!engine.diagnostics().empty())
        {
            const Diagnostic& diagnostic = engine.diagnostics().front();
            context.expect(diagnostic.location() == range.begin(),
                "TD-DIA-014 the primary location must equal range.begin");
            expectRange(context, diagnostic, range, "TD-DIA-014");
        }
        context.expect(engine.errorCount() == 0 && engine.warningCount() == 1 && engine.lintCount() == 0,
            "TD-DIA-014 only the warning counter must increment");
    });
}

void registerLintCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-015", [](TestContext& context) {
        DiagnosticEngine engine;
        engine.report(DiagnosticId::InvalidCondition, DiagnosticSeverity::Lint,
            SourceLocation(FileId(33), 7), "prefer an explicit comparison");
        context.expect(engine.diagnostics().size() == 1,
            "TD-DIA-015 an accepted lint must be stored");
        context.expect(engine.errorCount() == 0 && engine.warningCount() == 0 && engine.lintCount() == 1,
            "TD-DIA-015 only the lint counter must increment");
    });
}

void registerNoteCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-016", [](TestContext& context) {
        DiagnosticEngine engine;
        engine.report(DiagnosticId::PreviousDeclaration, DiagnosticSeverity::Note,
            SourceLocation(FileId(34), 41), "previous declaration is here");
        context.expect(engine.diagnostics().size() == 1,
            "TD-DIA-016 an accepted note must be stored");
        context.expect(engine.errorCount() == 0 && engine.warningCount() == 0 && engine.lintCount() == 0,
            "TD-DIA-016 Note must not increment any severity counter");
        context.expect(!engine.hasErrors(), "TD-DIA-016 Note must not make hasErrors true");
    });
}

void registerStructuredCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-017", [](TestContext& context) {
        DiagnosticEngine engine;
        const SourceRange primary(SourceLocation(FileId(35), 50), SourceLocation(FileId(35), 61));
        const SourceRange secondary(SourceLocation(FileId(36), 12), SourceLocation(FileId(36), 18));
        engine.reportStructured(DiagnosticId::DuplicateSymbol, DiagnosticSeverity::Warning, primary,
            "duplicate symbol", {{secondary, "first declaration"}}, {"rename one declaration"});
        context.expect(engine.diagnostics().size() == 1,
            "TD-DIA-017 one structured report must append one record");
        if (!engine.diagnostics().empty())
        {
            const Diagnostic& diagnostic = engine.diagnostics().front();
            context.expect(diagnostic.location() == primary.begin(),
                "TD-DIA-017 structured location must equal primaryRange.begin");
            expectRange(context, diagnostic, primary, "TD-DIA-017");
            context.expect(diagnostic.message() == "duplicate symbol",
                "TD-DIA-017 must preserve the primary message");
            context.expect(diagnostic.secondaryLabels().size() == 1 && diagnostic.notes().size() == 1,
                "TD-DIA-017 must preserve label and note counts");
            if (!diagnostic.secondaryLabels().empty())
            {
                const DiagnosticLabel& label = diagnostic.secondaryLabels().front();
                context.expect(label.range.begin() == secondary.begin() && label.range.end() == secondary.end(),
                    "TD-DIA-017 must preserve the secondary range");
                context.expect(label.message == "first declaration",
                    "TD-DIA-017 must preserve the secondary message");
            }
            if (!diagnostic.notes().empty())
                context.expect(diagnostic.notes().front() == "rename one declaration",
                    "TD-DIA-017 must preserve the note text");
        }
    });
}

void registerMixedSeverityCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-018", [](TestContext& context) {
        DiagnosticEngine engine(10);
        const SourceLocation location(FileId(37), 3);
        engine.report(DiagnosticId::PreviousDeclaration, DiagnosticSeverity::Note, location, "note-first");
        engine.report(DiagnosticId::InvalidReturn, DiagnosticSeverity::Error, location, "error-second");
        engine.report(DiagnosticId::InvalidConversion, DiagnosticSeverity::Warning, location, "warning-third");
        engine.report(DiagnosticId::InvalidCondition, DiagnosticSeverity::Lint, location, "lint-fourth");
        context.expect(engine.diagnostics().size() == 4,
            "TD-DIA-018 every report below the limit must be stored");
        if (engine.diagnostics().size() == 4)
        {
            context.expect(engine.diagnostics()[0].message() == "note-first" &&
                    engine.diagnostics()[1].message() == "error-second" &&
                    engine.diagnostics()[2].message() == "warning-third" &&
                    engine.diagnostics()[3].message() == "lint-fourth",
                "TD-DIA-018 accepted reports must preserve insertion order");
        }
        context.expect(engine.errorCount() == 1 && engine.warningCount() == 1 && engine.lintCount() == 1,
            "TD-DIA-018 severity counters must remain independent");
        context.expect(engine.hasErrors() && !engine.errorLimitReached(),
            "TD-DIA-018 one error below limit ten must set hasErrors without reaching the limit");
        context.expect(engine.suppressedDiagnosticCount() == 0,
            "TD-DIA-018 reports below the limit must not be suppressed");
    });
}
}

// Registers every CD-DIA-004 report path and count invariant in insertion order.
void registerDiagnosticEngineReportModule(TestRegistry& registry)
{
    registerInitialStateCase(registry);
    registerErrorByLocationCase(registry);
    registerWarningByRangeCase(registry);
    registerLintCase(registry);
    registerNoteCase(registry);
    registerStructuredCase(registry);
    registerMixedSeverityCase(registry);
}
}
