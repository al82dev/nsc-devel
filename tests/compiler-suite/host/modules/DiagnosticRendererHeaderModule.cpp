#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"

#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace nsc::suite
{
namespace
{
using nativesharp::FileId;
using nativesharp::SourceLocation;
using nativesharp::diagnostics::Diagnostic;
using nativesharp::diagnostics::DiagnosticId;
using nativesharp::diagnostics::DiagnosticRenderer;
using nativesharp::diagnostics::DiagnosticSeverity;
using nativesharp::source::SourceManager;

void addCase(TestRegistry& registry, std::string id, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-009", "Host.Diagnostics", "component",
        "PR", {"diagnostics", "renderer", "header"}, std::move(function)});
}

std::string header(const DiagnosticRenderer& renderer, const Diagnostic& diagnostic)
{
    std::ostringstream output;
    renderer.render(diagnostic, output);
    const std::string rendered = output.str();
    const std::size_t newline = rendered.find('\n');
    return rendered.substr(0, newline == std::string::npos ? rendered.size() : newline + 1);
}

Diagnostic makeDiagnostic(DiagnosticId id, DiagnosticSeverity severity,
    SourceLocation location, std::string message)
{
    return Diagnostic(id, severity, location, std::nullopt, std::move(message));
}

void registerValidSourceHeaderCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-047", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("sample.ns", "alpha\nbeta\n");
        const Diagnostic diagnostic = makeDiagnostic(DiagnosticId::InvalidType,
            DiagnosticSeverity::Error, SourceLocation(file, 6), "problem");
        const DiagnosticRenderer renderer(sources);
        context.expect(header(renderer, diagnostic) == "sample.ns:2:1: error: problem\n",
            "TD-DIA-047 a normal ID must render exact path, position, severity and message without code");
    });
}

void registerInvalidLocationHeaderCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-048", [](TestContext& context) {
        const SourceManager sources;
        const DiagnosticRenderer renderer(sources);
        const Diagnostic diagnostic = makeDiagnostic(DiagnosticId::InvalidType,
            DiagnosticSeverity::Warning, SourceLocation(), "invalid location");
        context.expect(header(renderer, diagnostic) == "<source>:1:1: warning: invalid location\n",
            "TD-DIA-048 an invalid location must use the source fallback at 1:1");
    });
}

void registerSeverityHeaderCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-049", [](TestContext& context) {
        const SourceManager sources;
        const DiagnosticRenderer renderer(sources);
        const std::array<std::pair<DiagnosticSeverity, std::string_view>, 4> cases{{
            {DiagnosticSeverity::Error, "error"},
            {DiagnosticSeverity::Warning, "warning"},
            {DiagnosticSeverity::Lint, "lint"},
            {DiagnosticSeverity::Note, "note"},
        }};
        for (const auto& [severity, name] : cases)
        {
            const Diagnostic diagnostic = makeDiagnostic(
                DiagnosticId::InvalidType, severity, SourceLocation(), "message");
            const std::string expected = "<source>:1:1: " + std::string(name) + ": message\n";
            context.expect(header(renderer, diagnostic) == expected,
                "TD-DIA-049 every severity must use its exact machine-facing name");
        }
    });
}

void registerNativeSharpCodesCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-050", [](TestContext& context) {
        const SourceManager sources;
        const DiagnosticRenderer renderer(sources);
        const std::array<std::pair<DiagnosticId, std::string_view>, 5> cases{{
            {DiagnosticId::NS3010InvalidCompilationStage, "NS3010"},
            {DiagnosticId::NS3020InvalidValueCategory, "NS3020"},
            {DiagnosticId::NS3030MissingEffectContext, "NS3030"},
            {DiagnosticId::NS3049DeferredLanguageSurface, "NS3049"},
            {DiagnosticId::NS3099InvalidConformanceManifest, "NS3099"},
        }};
        for (const auto& [id, code] : cases)
        {
            const Diagnostic diagnostic = makeDiagnostic(
                id, DiagnosticSeverity::Error, SourceLocation(), "message");
            const std::string expected = "<source>:1:1: error[" + std::string(code) + "]: message\n";
            context.expect(header(renderer, diagnostic) == expected,
                "TD-DIA-050 each coded identity must render brackets without an extra separator");
        }
    });
}

void registerUncodedIdentitiesCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-051", [](TestContext& context) {
        const SourceManager sources;
        const DiagnosticRenderer renderer(sources);
        const Diagnostic normal = makeDiagnostic(DiagnosticId::InvalidType,
            DiagnosticSeverity::Error, SourceLocation(), "normal");
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) -- probes the documented unknown-ID path.
        const Diagnostic unknown = makeDiagnostic(static_cast<DiagnosticId>(999),
            DiagnosticSeverity::Error, SourceLocation(), "unknown");
        context.expect(header(renderer, normal) == "<source>:1:1: error: normal\n",
            "TD-DIA-051 a normal identity must not render brackets");
        context.expect(header(renderer, unknown) == "<source>:1:1: error: unknown\n",
            "TD-DIA-051 an unknown identity must not render brackets");
    });
}

void registerMessageBytesCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-052", [](TestContext& context) {
        const SourceManager sources;
        const DiagnosticRenderer renderer(sources);
        const Diagnostic empty = makeDiagnostic(DiagnosticId::InvalidType,
            DiagnosticSeverity::Error, SourceLocation(), "");
        const Diagnostic punctuated = makeDiagnostic(DiagnosticId::InvalidType,
            DiagnosticSeverity::Error, SourceLocation(), "value: [x]!?");
        context.expect(header(renderer, empty) == "<source>:1:1: error: \n",
            "TD-DIA-052 an empty message must preserve its separator and final newline");
        context.expect(header(renderer, punctuated) == "<source>:1:1: error: value: [x]!?\n",
            "TD-DIA-052 punctuation bytes must be preserved exactly");
    });
}
}

void registerDiagnosticRendererHeaderModule(TestRegistry& registry)
{
    registerValidSourceHeaderCase(registry);
    registerInvalidLocationHeaderCase(registry);
    registerSeverityHeaderCase(registry);
    registerNativeSharpCodesCase(registry);
    registerUncodedIdentitiesCase(registry);
    registerMessageBytesCase(registry);
}
}
