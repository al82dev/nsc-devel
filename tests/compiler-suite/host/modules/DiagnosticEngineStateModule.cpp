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

// Registers one case under the engine-query and reset contract. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-008", "Host.Diagnostics", "unit",
        "PR", {"diagnostics", "engine", "state"}, std::move(function)});
}

// Reports one record at a stable source coordinate. Complexity is O(message size).
void report(DiagnosticEngine& engine, DiagnosticSeverity severity, std::uint32_t offset, std::string message)
{
    engine.report(DiagnosticId::InvalidType, severity,
        SourceLocation(FileId(61), offset), std::move(message));
}

void registerMixedQueryCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-043", [](TestContext& context) {
        DiagnosticEngine engine(7);
        report(engine, DiagnosticSeverity::Error, 1, "error");
        report(engine, DiagnosticSeverity::Warning, 2, "warning");
        report(engine, DiagnosticSeverity::Lint, 3, "lint");
        report(engine, DiagnosticSeverity::Note, 4, "note");
        context.expect(engine.diagnostics().size() == 4,
            "TD-DIA-043 diagnostics must expose every accepted record");
        context.expect(engine.errorCount() == 1 && engine.warningCount() == 1 && engine.lintCount() == 1,
            "TD-DIA-043 accepted severity counters must reflect the mixed reports");
        context.expect(engine.errorLimit() == 7 && !engine.errorLimitReached(),
            "TD-DIA-043 one error must remain below the configured limit");
        context.expect(engine.suppressedDiagnosticCount() == 0 && engine.hasErrors(),
            "TD-DIA-043 suppression and hasErrors queries must reflect stored state");
    });
}

void registerClearAccumulatedStateCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-044", [](TestContext& context) {
        DiagnosticEngine engine(1);
        report(engine, DiagnosticSeverity::Error, 1, "accepted");
        report(engine, DiagnosticSeverity::Warning, 2, "suppressed");
        engine.clear();
        context.expect(engine.diagnostics().empty(),
            "TD-DIA-044 clear must destroy all stored diagnostics");
        context.expect(engine.errorCount() == 0 && engine.warningCount() == 0 && engine.lintCount() == 0,
            "TD-DIA-044 clear must reset accepted severity counts");
        context.expect(engine.suppressedDiagnosticCount() == 0,
            "TD-DIA-044 clear must reset the suppressed count");
        context.expect(!engine.errorLimitReached() && !engine.hasErrors(),
            "TD-DIA-044 clear must reset state flags");
    });
}

void registerPreservedLimitCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-045", [](TestContext& context) {
        DiagnosticEngine engine(7);
        report(engine, DiagnosticSeverity::Error, 1, "transient");
        engine.clear();
        context.expect(engine.errorLimit() == 7,
            "TD-DIA-045 clear must preserve the configured error limit");
        context.expect(engine.diagnostics().empty() && !engine.errorLimitReached(),
            "TD-DIA-045 preserving configuration must not preserve accumulated state");
    });
}

void registerReuseAfterClearCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-046", [](TestContext& context) {
        DiagnosticEngine engine(2);
        report(engine, DiagnosticSeverity::Error, 1, "old first");
        report(engine, DiagnosticSeverity::Error, 2, "old second");
        report(engine, DiagnosticSeverity::Warning, 3, "old suppressed");
        engine.clear();
        report(engine, DiagnosticSeverity::Error, 4, "new first");
        report(engine, DiagnosticSeverity::Error, 5, "new second");
        context.expect(engine.diagnostics().size() == 2 && engine.errorCount() == 2,
            "TD-DIA-046 a cleared engine must accept reports up to its original limit again");
        context.expect(engine.errorLimitReached() && engine.suppressedDiagnosticCount() == 0,
            "TD-DIA-046 reuse must start with a clean suppression state");
        if (engine.diagnostics().size() == 2)
        {
            context.expect(engine.diagnostics()[0].message() == "new first" &&
                    engine.diagnostics()[1].message() == "new second",
                "TD-DIA-046 reuse must expose only the new accepted records");
        }
    });
}
}

// Registers every CD-DIA-008 query and reset case without exercising downstream rendering.
void registerDiagnosticEngineStateModule(TestRegistry& registry)
{
    registerMixedQueryCase(registry);
    registerClearAccumulatedStateCase(registry);
    registerPreservedLimitCase(registry);
    registerReuseAfterClearCase(registry);
}
}
