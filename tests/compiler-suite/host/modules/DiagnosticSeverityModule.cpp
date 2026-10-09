#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticSeverity.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::diagnostics::DiagnosticSeverity;
using nativesharp::diagnostics::diagnosticSeverityName;

using SeverityName = std::pair<DiagnosticSeverity, std::string_view>;

// Registers one case under the fixed Host.Diagnostics severity contract. Complexity is O(1).
void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-001", "Host.Diagnostics", "unit",
        "PR", {"diagnostics", "severity"}, std::move(function)});
}
}

// Registers the complete CD-DIA-001 vocabulary and fallback cases without shared mutable state.
void registerDiagnosticSeverityModule(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-001", [](TestContext& context) {
        constexpr std::array expected{
            SeverityName{DiagnosticSeverity::Note, "note"},
            SeverityName{DiagnosticSeverity::Warning, "warning"},
            SeverityName{DiagnosticSeverity::Error, "error"},
            SeverityName{DiagnosticSeverity::Lint, "lint"},
        };
        for (const SeverityName& entry : expected)
            context.expect(diagnosticSeverityName(entry.first) == entry.second,
                "TD-DIA-001 every known severity must preserve its exact lowercase name");
    });
    addCase(registry, "TD-DIA-002", [](TestContext& context) {
        // The frozen API intentionally defines a fallback for values outside the enumerator set.
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
        constexpr auto unknown = static_cast<DiagnosticSeverity>(4);
        context.expect(diagnosticSeverityName(unknown) == "error",
            "TD-DIA-002 an unknown severity value must use the error fallback");
    });
}
}
