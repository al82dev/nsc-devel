#include "LanguageServiceTestSupport.h"

namespace nsc::suite
{
namespace
{
using namespace language_service_support;
using namespace nativesharp::language_service;

void analysisRequestContract(TestContext& context, std::size_t variant)
{
    if (variant == 3U)
    {
        LanguageService service;
        const AnalysisSnapshot snapshot = service.analyzeProject({});
        context.expect(snapshot.available() && snapshot.generation() == 1U &&
                !snapshot.currentAnalysisSucceeded() && !snapshot.error().empty(),
            "invalid project paths return generation-tagged error snapshots without compilation");
        return;
    }
    SemanticFixture fixture(variant == 5U ? 0U : variant == 8U ? 8U : 1U);
    if (variant == 4U)
    {
        const AnalysisSnapshot missing = fixture.service().analyzeProject(fixture.root() / "missing.nsproj");
        context.expect(!missing.currentAnalysisSucceeded() && !missing.error().empty(),
            "missing project descriptors fail in the current generation with an exact error channel");
        return;
    }
    const AnalysisSnapshot next = fixture.service().analyzeProject(
        fixture.project(), 1U, variant == 6U ? std::function<bool()>([] { return true; }) :
            std::function<bool()>{});
    context.expect(fixture.snapshot().available() && next.available() &&
            next.generation() > fixture.snapshot().generation() && next.projectPath() == fixture.project() &&
            (variant == 6U ? next.cancelled() && !next.currentAnalysisSucceeded() :
                next.currentAnalysisSucceeded() && !next.cancelled()),
        "analysis generations, project identity, jobs normalization, cancellation, and success remain explicit");
}

void overlayAnalysisContract(TestContext& context, std::size_t variant)
{
    SemanticFixture fixture;
    std::string error;
    if (variant == 3U)
    {
        context.expect(fixture.snapshot().currentAnalysisSucceeded(),
            "an absent overlay falls through to the project source on disk");
        return;
    }
    if (variant == 4U)
    {
        const bool opened = fixture.service().openDocument({}, "invalid", 1, error);
        const AnalysisSnapshot snapshot = fixture.service().analyzeProject(fixture.project());
        context.expect(!opened && !error.empty() && snapshot.currentAnalysisSucceeded(),
            "an invalid overlay path is rejected and cannot replace disk source");
        return;
    }
    const std::string overlay = variant == 5U ?
        "namespace Demo;\npublic partial class Counter { public int Run(int Value) { return Value; } }\n" :
        "\"unterminated overlay";
    const bool opened = fixture.service().openDocument(fixture.use(), overlay, 1, error);
    const AnalysisSnapshot snapshot = fixture.service().analyzeProject(fixture.project());
    context.expect(variant == 5U ? opened && snapshot.currentAnalysisSucceeded() :
            opened && !snapshot.currentAnalysisSucceeded() && !snapshot.diagnostics().empty(),
        "analysis consumes the generation-local open-document overlay instead of stale disk contents");
}

void publicationFallbackContract(TestContext& context, std::size_t variant)
{
    SemanticFixture fixture;
    if (variant == 3U || variant == 4U)
    {
        LanguageService service;
        TemporaryTree tree("fallback-empty");
        const auto project = tree.write(
            "Broken.nsproj",
            "<Project><PropertyGroup><Name>Broken</Name></PropertyGroup>"
            "<ItemGroup><Compile Include=\"Main.ns\"/></ItemGroup></Project>");
        const auto source = tree.write("Main.ns", "\"unterminated");
        std::string error;
        if (variant == 4U) service.openDocument(source, "\"still unterminated", 1, error);
        const AnalysisSnapshot broken = service.analyzeProject(project);
        context.expect(!broken.currentAnalysisSucceeded() && !broken.semanticModelAvailable() &&
                !broken.usedPreviousSemanticModel() && !broken.diagnostics().empty(),
            "a failed first analysis publishes current diagnostics without inventing prior semantics");
        return;
    }
    std::string error;
    const bool opened = fixture.service().openDocument(fixture.use(), "\"unterminated overlay", 1, error);
    const AnalysisSnapshot broken = fixture.service().analyzeProject(
        fixture.project(), variant == 8U ? 8U : 1U,
        variant == 6U ? std::function<bool()>([] { return true; }) : std::function<bool()>{});
    context.expect(fixture.snapshot().currentAnalysisSucceeded() && fixture.snapshot().semanticModelAvailable() &&
            opened && !broken.currentAnalysisSucceeded() && !broken.diagnostics().empty() &&
            broken.semanticModelAvailable() && broken.usedPreviousSemanticModel() &&
            broken.generation() > fixture.snapshot().generation(),
        "failed or cancelled current analysis publishes current diagnostics and the last-good semantic index");
}
}

void registerLanguageServiceAnalysis313To336Module(TestRegistry& registry)
{
    addLanguageServiceContract(registry, 40U, "component", analysisRequestContract);
    addLanguageServiceContract(registry, 41U, "component", overlayAnalysisContract);
    addLanguageServiceContract(registry, 42U, "component", publicationFallbackContract);
}
}
