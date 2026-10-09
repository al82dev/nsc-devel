#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"

#include <string>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::diagnostics::DiagnosticEngine;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::diagnostics::DiagnosticSeverity;

// Registers one case under the error-budget contract. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-005", "Host.Diagnostics", "component",
        "PR", {"diagnostics", "engine", "budget"}, std::move(function)});
}

// Reports one distinguishable error for transition-oriented fixtures. Complexity is O(message size).
void reportError(DiagnosticEngine& engine, std::uint32_t offset, std::string message)
{
    engine.report(DiagnosticId::InvalidType, DiagnosticSeverity::Error,
        SourceLocation(FileId(41), offset), std::move(message));
}

void registerNormalizedLimitCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-019", [](TestContext& context) {
        const DiagnosticEngine engine(0);
        context.expect(engine.errorLimit() == 1,
            "TD-DIA-019 a requested zero limit must normalize to one");
        context.expect(engine.diagnostics().empty() && !engine.errorLimitReached(),
            "TD-DIA-019 normalization must not change the empty initial state");
    });
}

void registerLimitReachingErrorCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-020", [](TestContext& context) {
        DiagnosticEngine engine(1);
        reportError(engine, 10, "first error reaches limit");
        context.expect(engine.diagnostics().size() == 1,
            "TD-DIA-020 the limit-reaching error must be stored");
        context.expect(engine.errorCount() == 1 && engine.hasErrors(),
            "TD-DIA-020 the accepted error must update error queries");
        context.expect(engine.errorLimitReached(),
            "TD-DIA-020 accepting the first error at limit one must reach the limit");
        context.expect(engine.suppressedDiagnosticCount() == 0,
            "TD-DIA-020 the limit-reaching error must not count as suppressed");
    });
}

void registerSuppressedErrorCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-021", [](TestContext& context) {
        DiagnosticEngine engine(1);
        reportError(engine, 10, "accepted error");
        reportError(engine, 20, "suppressed error");
        context.expect(engine.diagnostics().size() == 1 && engine.errorCount() == 1,
            "TD-DIA-021 an error after the limit must not be stored or counted");
        if (!engine.diagnostics().empty())
        {
            context.expect(engine.diagnostics().front().message() == "accepted error",
                "TD-DIA-021 suppression must preserve the accepted prefix");
        }
        context.expect(engine.suppressedDiagnosticCount() == 1,
            "TD-DIA-021 the rejected error must increment the suppressed count once");
    });
}

void registerSuppressedWarningCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-022", [](TestContext& context) {
        DiagnosticEngine engine(1);
        reportError(engine, 10, "accepted error");
        engine.report(DiagnosticId::InvalidConversion, DiagnosticSeverity::Warning,
            SourceLocation(FileId(41), 20), "suppressed warning");
        context.expect(engine.diagnostics().size() == 1 && engine.warningCount() == 0,
            "TD-DIA-022 Warning after the limit must not be stored or counted");
        context.expect(engine.suppressedDiagnosticCount() == 1,
            "TD-DIA-022 the rejected Warning must increment the suppressed count");
    });
}

void registerSuppressedLintCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-023", [](TestContext& context) {
        DiagnosticEngine engine(1);
        reportError(engine, 10, "accepted error");
        engine.report(DiagnosticId::InvalidCondition, DiagnosticSeverity::Lint,
            SourceLocation(FileId(41), 20), "suppressed lint");
        context.expect(engine.diagnostics().size() == 1 && engine.lintCount() == 0,
            "TD-DIA-023 Lint after the limit must not be stored or counted");
        context.expect(engine.suppressedDiagnosticCount() == 1,
            "TD-DIA-023 the rejected Lint must increment the suppressed count");
    });
}

void registerSuppressedNoteCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-024", [](TestContext& context) {
        DiagnosticEngine engine(1);
        reportError(engine, 10, "accepted error");
        engine.report(DiagnosticId::PreviousDeclaration, DiagnosticSeverity::Note,
            SourceLocation(FileId(41), 20), "suppressed note");
        context.expect(engine.diagnostics().size() == 1,
            "TD-DIA-024 Note after the limit must not be stored");
        context.expect(engine.errorCount() == 1 && engine.warningCount() == 0 && engine.lintCount() == 0,
            "TD-DIA-024 suppressing Note must leave all accepted severity counts unchanged");
        context.expect(engine.suppressedDiagnosticCount() == 1,
            "TD-DIA-024 the rejected Note must increment the suppressed count");
    });
}

void registerBelowLimitCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-025", [](TestContext& context) {
        DiagnosticEngine engine(3);
        reportError(engine, 10, "first accepted error");
        reportError(engine, 20, "second accepted error");
        context.expect(engine.diagnostics().size() == 2 && engine.errorCount() == 2,
            "TD-DIA-025 two errors below limit three must both be accepted");
        context.expect(!engine.errorLimitReached(),
            "TD-DIA-025 two errors with limit three must not reach the limit");
        context.expect(engine.suppressedDiagnosticCount() == 0,
            "TD-DIA-025 reports below the limit must not be suppressed");
    });
}

void registerThirdErrorCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-026", [](TestContext& context) {
        DiagnosticEngine engine(3);
        reportError(engine, 10, "first accepted error");
        reportError(engine, 20, "second accepted error");
        reportError(engine, 30, "third accepted error");
        context.expect(engine.diagnostics().size() == 3 && engine.errorCount() == 3,
            "TD-DIA-026 the third error at limit three must be stored and counted");
        context.expect(engine.errorLimitReached(),
            "TD-DIA-026 accepting the third error must transition to reached");
        context.expect(engine.suppressedDiagnosticCount() == 0,
            "TD-DIA-026 the limit-reaching error must not be suppressed");
    });
}
}

// Registers the complete CD-DIA-005 transition matrix without testing downstream append or merge behavior.
void registerDiagnosticEngineBudgetModule(TestRegistry& registry)
{
    registerNormalizedLimitCase(registry);
    registerLimitReachingErrorCase(registry);
    registerSuppressedErrorCase(registry);
    registerSuppressedWarningCase(registry);
    registerSuppressedLintCase(registry);
    registerSuppressedNoteCase(registry);
    registerBelowLimitCase(registry);
    registerThirdErrorCase(registry);
}
}
