#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/Diagnostic.h"

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
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::diagnostics::DiagnosticLabel;
using nativesharp::diagnostics::DiagnosticSeverity;

// Registers one case under the fixed owned-diagnostic-record contract. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-003", "Host.Diagnostics", "unit",
        "PR", {"diagnostics", "record"}, std::move(function)});
}

// Returns a record after every caller-owned text container has left scope. Complexity is O(N) in text size.
Diagnostic makeOwnedDiagnostic()
{
    std::string message = "temporary primary message";
    std::vector<DiagnosticLabel> labels{
        {SourceRange(SourceLocation(FileId(17), 8), SourceLocation(FileId(17), 13)),
            "temporary label message"},
    };
    std::vector<std::string> notes{"temporary first note", "temporary second note"};
    return Diagnostic(DiagnosticId::TypeMismatch, DiagnosticSeverity::Warning,
        SourceLocation(FileId(17), 5), std::nullopt, std::move(message), std::move(labels), std::move(notes));
}

void registerLocationOnlyCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-007", [](TestContext& context) {
        const SourceLocation location(FileId(11), 29);
        const Diagnostic diagnostic(DiagnosticId::UnexpectedToken, DiagnosticSeverity::Error,
            location, std::nullopt, "unexpected closing brace");
        context.expect(diagnostic.id() == DiagnosticId::UnexpectedToken,
            "TD-DIA-007 must preserve the diagnostic ID");
        context.expect(diagnostic.severity() == DiagnosticSeverity::Error,
            "TD-DIA-007 must preserve the severity");
        context.expect(diagnostic.location() == location,
            "TD-DIA-007 must preserve the primary location");
        context.expect(diagnostic.message() == "unexpected closing brace",
            "TD-DIA-007 must preserve the primary message");
        context.expect(!diagnostic.range().has_value(),
            "TD-DIA-007 must preserve an absent primary range");
    });
}

void registerRangeCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-008", [](TestContext& context) {
        const SourceRange range(SourceLocation(FileId(12), 31), SourceLocation(FileId(12), 44));
        const Diagnostic diagnostic(DiagnosticId::ExpectedExpression, DiagnosticSeverity::Error,
            range.begin(), range, "expression expected");
        context.expect(diagnostic.range().has_value(),
            "TD-DIA-008 must preserve the presence of the primary range");
        if (diagnostic.range())
        {
            context.expect(diagnostic.range()->begin() == range.begin(),
                "TD-DIA-008 must preserve the exact range beginning");
            context.expect(diagnostic.range()->end() == range.end(),
                "TD-DIA-008 must preserve the exact range end");
        }
    });
}

void registerOwnedTextCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-009", [](TestContext& context) {
        const Diagnostic diagnostic = makeOwnedDiagnostic();
        context.expect(diagnostic.message() == "temporary primary message",
            "TD-DIA-009 must own the primary message after its input lifetime ends");
        context.expect(diagnostic.secondaryLabels().size() == 1,
            "TD-DIA-009 must own the label sequence after its input lifetime ends");
        if (!diagnostic.secondaryLabels().empty())
        {
            context.expect(diagnostic.secondaryLabels().front().message == "temporary label message",
                "TD-DIA-009 must own label text after its input lifetime ends");
        }
        context.expect(diagnostic.notes().size() == 2,
            "TD-DIA-009 must own the note sequence after its input lifetime ends");
        if (diagnostic.notes().size() == 2)
        {
            context.expect(diagnostic.notes()[0] == "temporary first note" &&
                    diagnostic.notes()[1] == "temporary second note",
                "TD-DIA-009 must preserve owned note order and text");
        }
    });
}

void registerLabelCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-010", [](TestContext& context) {
        const SourceRange labelRange(SourceLocation(FileId(19), 40), SourceLocation(FileId(19), 47));
        std::vector<DiagnosticLabel> labels{{labelRange, "previous declaration is here"}};
        const Diagnostic diagnostic(DiagnosticId::DuplicateSymbol, DiagnosticSeverity::Error,
            SourceLocation(FileId(19), 80), std::nullopt, "duplicate symbol", std::move(labels));
        context.expect(diagnostic.secondaryLabels().size() == 1,
            "TD-DIA-010 must preserve the label count");
        if (!diagnostic.secondaryLabels().empty())
        {
            const DiagnosticLabel& label = diagnostic.secondaryLabels().front();
            context.expect(label.range.begin() == labelRange.begin() && label.range.end() == labelRange.end(),
                "TD-DIA-010 must preserve both label range endpoints");
            context.expect(label.message == "previous declaration is here",
                "TD-DIA-010 must preserve the label message");
        }
    });
}

void registerIncoherentDataCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-011", [](TestContext& context) {
        const SourceLocation location(FileId(23), 90);
        const SourceRange range(SourceLocation(FileId(24), 70), SourceLocation(FileId(24), 30));
        const Diagnostic diagnostic(DiagnosticId::InvalidType, DiagnosticSeverity::Lint,
            location, range, "deliberately incoherent source coordinates");
        context.expect(diagnostic.location() == location,
            "TD-DIA-011 must preserve the independent primary location");
        context.expect(diagnostic.range().has_value(),
            "TD-DIA-011 must preserve the deliberately incoherent range");
        if (diagnostic.range())
        {
            context.expect(diagnostic.range()->begin() == range.begin() &&
                    diagnostic.range()->end() == range.end(),
                "TD-DIA-011 must not normalize or reject incoherent range data");
            context.expect(!diagnostic.range()->isValid(),
                "TD-DIA-011 fixture must remain observably incoherent");
        }
    });
}
}

// Registers the complete CD-DIA-003 case set. Each record owns its text and sequence state.
void registerDiagnosticRecordModule(TestRegistry& registry)
{
    registerLocationOnlyCase(registry);
    registerRangeCase(registry);
    registerOwnedTextCase(registry);
    registerLabelCase(registry);
    registerIncoherentDataCase(registry);
}
}
