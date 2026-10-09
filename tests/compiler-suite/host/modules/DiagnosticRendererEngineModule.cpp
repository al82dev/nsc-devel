#include "nsc_suite/TestRegistry.h"
#include "nativesharp/Diagnostics/DiagnosticEngine.h"
#include "nativesharp/Diagnostics/DiagnosticRenderer.h"

#include <ios>
#include <ostream>
#include <sstream>
#include <streambuf>
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
using nativesharp::diagnostics::DiagnosticRenderer;
using nativesharp::diagnostics::DiagnosticSeverity;
using nativesharp::source::SourceManager;

class FailingStreamBuffer final : public std::streambuf
{
protected:
    std::streamsize xsputn(const char_type*, std::streamsize) override
    {
        return 0;
    }

    int_type overflow(int_type) override
    {
        return traits_type::eof();
    }
};

void addCase(TestRegistry& registry, std::string id, std::string profile, TestFunction function)
{
    registry.add(TestCase{std::move(id), "CD-DIA-012", "Host.Diagnostics", "component",
        std::move(profile), {"diagnostics", "renderer", "engine"}, std::move(function)});
}

std::string renderAll(const SourceManager& sources, const DiagnosticEngine& engine)
{
    std::ostringstream output;
    DiagnosticRenderer(sources).renderAll(engine, output);
    return output.str();
}

void report(
    DiagnosticEngine& engine,
    DiagnosticSeverity severity,
    SourceLocation location,
    std::string message)
{
    engine.report(DiagnosticId::InvalidType, severity, location, std::move(message));
}

void registerEmptyEngineCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-070", "PR", [](TestContext& context) {
        SourceManager sources;
        const DiagnosticEngine engine;
        context.expect(renderAll(sources, engine).empty(),
            "TD-DIA-070 an empty engine must render no bytes");
    });
}

void registerVectorOrderCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-071", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("ordered.ns", "x");
        DiagnosticEngine engine;
        report(engine, DiagnosticSeverity::Warning, SourceLocation(file, 0), "first");
        report(engine, DiagnosticSeverity::Error, SourceLocation(file, 0), "second");
        report(engine, DiagnosticSeverity::Note, SourceLocation(file, 0), "third");
        const std::string actual = renderAll(sources, engine);
        const std::size_t first = actual.find(": warning: first\n");
        const std::size_t second = actual.find(": error: second\n");
        const std::size_t third = actual.find(": note: third\n");
        context.expect(first != std::string::npos && first < second && second < third,
            "TD-DIA-071 renderAll must preserve the diagnostic vector order exactly");
    });
}

void registerNoSummaryCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-072", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("ordinary.ns", "x");
        DiagnosticEngine engine;
        report(engine, DiagnosticSeverity::Warning, SourceLocation(file, 0), "ordinary");
        const std::string actual = renderAll(sources, engine);
        context.expect(actual.find("diagnostic limit reached") == std::string::npos,
            "TD-DIA-072 an engine without suppressed diagnostics must omit the summary");
    });
}

void registerSummaryCountsCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-073", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("limited.ns", "x");
        DiagnosticEngine engine(2);
        report(engine, DiagnosticSeverity::Error, SourceLocation(file, 0), "first error");
        report(engine, DiagnosticSeverity::Error, SourceLocation(file, 0), "second error");
        report(engine, DiagnosticSeverity::Warning, SourceLocation(file, 0), "suppressed warning");
        report(engine, DiagnosticSeverity::Error, SourceLocation(file, 0), "suppressed error");
        const std::string actual = renderAll(sources, engine);
        context.expect(actual.ends_with(
                "nsc: error: diagnostic limit reached after 2 errors; "
                "2 additional diagnostics suppressed\n"),
            "TD-DIA-073 the suppression summary must expose exact accepted and suppressed counts");
    });
}

void registerPluralLiteralCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-074", "PR", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("singular.ns", "x");
        DiagnosticEngine engine(1);
        report(engine, DiagnosticSeverity::Error, SourceLocation(file, 0), "accepted");
        report(engine, DiagnosticSeverity::Warning, SourceLocation(file, 0), "suppressed");
        const std::string actual = renderAll(sources, engine);
        context.expect(actual.ends_with(
                "nsc: error: diagnostic limit reached after 1 errors; "
                "1 additional diagnostics suppressed\n"),
            "TD-DIA-074 the current summary must retain its literal plural words for count one");
    });
}

void registerStreamFailureCase(TestRegistry& registry)
{
    addCase(registry, "TD-DIA-075", "NIGHTLY", [](TestContext& context) {
        SourceManager sources;
        const FileId file = sources.addBuffer("stream.ns", "x");
        DiagnosticEngine engine;
        report(engine, DiagnosticSeverity::Error, SourceLocation(file, 0), "stable");
        FailingStreamBuffer buffer;
        std::ostream output(&buffer);
        output.exceptions(std::ios::badbit);
        bool threw = false;
        try
        {
            DiagnosticRenderer(sources).renderAll(engine, output);
        }
        catch (const std::ios_base::failure&)
        {
            threw = true;
        }
        context.expect(threw, "TD-DIA-075 an enabled stream exception must propagate");
        context.expect(engine.diagnostics().size() == 1 && engine.errorCount() == 1 &&
                engine.suppressedDiagnosticCount() == 0,
            "TD-DIA-075 stream failure must not mutate diagnostic engine state");
    });
}
}

void registerDiagnosticRendererEngineModule(TestRegistry& registry)
{
    registerEmptyEngineCase(registry);
    registerVectorOrderCase(registry);
    registerNoSummaryCase(registry);
    registerSummaryCountsCase(registry);
    registerPluralLiteralCase(registry);
    registerStreamFailureCase(registry);
}
}
