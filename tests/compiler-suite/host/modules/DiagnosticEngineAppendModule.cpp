#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"

#include <optional>
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

// Registers one case under the append-fidelity contract. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-006", "Host.Diagnostics", "component",
        "PR", {"diagnostics", "engine", "append"}, std::move(function)});
}

// Verifies range endpoints after checking optional presence. Complexity is O(1).
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

void registerSimpleAppendCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-027", [](TestContext& context) {
        const SourceLocation location(FileId(51), 14);
        const Diagnostic source(DiagnosticId::InvalidCall, DiagnosticSeverity::Error,
            location, std::nullopt, "invalid call");
        DiagnosticEngine receiver(10);
        receiver.append(source);
        context.expect(receiver.diagnostics().size() == 1 && receiver.errorCount() == 1,
            "TD-DIA-027 append must store and count one simple Error");
        if (!receiver.diagnostics().empty())
        {
            const Diagnostic& stored = receiver.diagnostics().front();
            context.expect(stored.id() == source.id() && stored.severity() == source.severity(),
                "TD-DIA-027 append must preserve ID and severity");
            context.expect(stored.location() == location && !stored.range().has_value(),
                "TD-DIA-027 append must preserve location and absent range");
            context.expect(stored.message() == "invalid call",
                "TD-DIA-027 append must preserve the message");
        }
    });
}

void registerRangeAppendCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-028", [](TestContext& context) {
        const SourceRange range(SourceLocation(FileId(52), 20), SourceLocation(FileId(52), 28));
        const Diagnostic source(DiagnosticId::InvalidIndex, DiagnosticSeverity::Warning,
            range.begin(), range, "suspicious index");
        DiagnosticEngine receiver;
        receiver.append(source);
        context.expect(receiver.diagnostics().size() == 1 && receiver.warningCount() == 1,
            "TD-DIA-028 append must store and count one ranged Warning");
        if (!receiver.diagnostics().empty())
        {
            const Diagnostic& stored = receiver.diagnostics().front();
            context.expect(stored.location() == range.begin(),
                "TD-DIA-028 append must preserve range.begin as location");
            expectRange(context, stored, range, "TD-DIA-028");
        }
    });
}

void registerStructuredRangeAppendCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-029", [](TestContext& context) {
        const SourceRange primary(SourceLocation(FileId(53), 30), SourceLocation(FileId(53), 42));
        const SourceRange secondary(SourceLocation(FileId(54), 7), SourceLocation(FileId(54), 11));
        const Diagnostic source(DiagnosticId::DuplicateSymbol, DiagnosticSeverity::Lint,
            primary.begin(), primary, "duplicate symbol", {{secondary, "first symbol"}}, {"rename it"});
        DiagnosticEngine receiver;
        receiver.append(source);
        context.expect(receiver.diagnostics().size() == 1 && receiver.lintCount() == 1,
            "TD-DIA-029 append must store and count one structured Lint");
        if (!receiver.diagnostics().empty())
        {
            const Diagnostic& stored = receiver.diagnostics().front();
            expectRange(context, stored, primary, "TD-DIA-029");
            context.expect(stored.secondaryLabels().size() == 1 && stored.notes().size() == 1,
                "TD-DIA-029 append must preserve label and note counts");
            if (!stored.secondaryLabels().empty())
            {
                const DiagnosticLabel& label = stored.secondaryLabels().front();
                context.expect(label.range.begin() == secondary.begin() && label.range.end() == secondary.end(),
                    "TD-DIA-029 append must preserve the secondary range");
                context.expect(label.message == "first symbol",
                    "TD-DIA-029 append must preserve the secondary message");
            }
            if (!stored.notes().empty())
                context.expect(stored.notes().front() == "rename it",
                    "TD-DIA-029 append must preserve the note");
        }
    });
}

void registerStructuredNoRangeAppendCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-030", [](TestContext& context) {
        const SourceLocation location(FileId(55), 60);
        const SourceRange secondary(SourceLocation(FileId(55), 10), SourceLocation(FileId(55), 15));
        const Diagnostic source(DiagnosticId::PreviousDeclaration, DiagnosticSeverity::Note,
            location, std::nullopt, "related declaration", {{secondary, "origin"}}, {"additional context"});
        DiagnosticEngine receiver;
        receiver.append(source);
        context.expect(receiver.diagnostics().size() == 1,
            "TD-DIA-030 append must store the structured no-range source");
        if (!receiver.diagnostics().empty())
        {
            const Diagnostic& stored = receiver.diagnostics().front();
            context.expect(stored.range().has_value(),
                "TD-DIA-030 structured append must synthesize a primary range");
            if (stored.range())
            {
                context.expect(stored.range()->begin() == location && stored.range()->end() == location,
                    "TD-DIA-030 the synthesized range must be empty at source.location");
            }
            context.expect(stored.secondaryLabels().size() == 1 && stored.notes().size() == 1,
                "TD-DIA-030 synthesized range must not discard secondary data");
        }
    });
}

void registerSuppressedAppendCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-031", [](TestContext& context) {
        DiagnosticEngine receiver(1);
        receiver.report(DiagnosticId::InvalidType, DiagnosticSeverity::Error,
            SourceLocation(FileId(56), 1), "accepted error");
        const Diagnostic source(DiagnosticId::InvalidConversion, DiagnosticSeverity::Warning,
            SourceLocation(FileId(56), 2), std::nullopt, "suppressed append");
        receiver.append(source);
        context.expect(receiver.diagnostics().size() == 1,
            "TD-DIA-031 append after the limit must not copy its source");
        context.expect(receiver.errorCount() == 1 && receiver.warningCount() == 0,
            "TD-DIA-031 suppressed append must not alter accepted severity counts");
        context.expect(receiver.suppressedDiagnosticCount() == 1,
            "TD-DIA-031 suppressed append must apply the receiver budget exactly once");
        context.expect(source.message() == "suppressed append",
            "TD-DIA-031 append must not mutate its source record");
    });
}
}

// Registers the complete CD-DIA-006 append path matrix without exercising deterministic merge.
void registerDiagnosticEngineAppendModule(TestRegistry& registry)
{
    registerSimpleAppendCase(registry);
    registerRangeAppendCase(registry);
    registerStructuredRangeAppendCase(registry);
    registerStructuredNoRangeAppendCase(registry);
    registerSuppressedAppendCase(registry);
}
}
