#include "nativesharp/LanguageService/LanguageService.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
int failures = 0;

void expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

int main()
{
    using nativesharp::language_service::LanguageService;
    LanguageService service;
    const std::filesystem::path directory =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples" / "project_stage22";
    const std::filesystem::path project = directory / "Hello.nsproj";
    const std::filesystem::path source = directory / "Main.ns";

    const auto good = service.analyzeProject(project, 2);
    expect(good.available(), "analysis produces a snapshot");
    expect(good.currentAnalysisSucceeded(), "valid project reaches method-body semantics");
    expect(good.semanticModelAvailable(), "valid project publishes semantic model");
    expect(!good.usedPreviousSemanticModel(), "first valid snapshot is current");
    expect(good.diagnostics().empty(), "valid project has no diagnostics");

    std::string error;
    expect(service.openDocument(source, "\"unterminated overlay", 1, error), "invalid edit opens");
    const auto broken = service.analyzeProject(project, 2);
    expect(!broken.currentAnalysisSucceeded(), "invalid edit fails current analysis");
    expect(!broken.diagnostics().empty(), "invalid edit publishes diagnostics");
    expect(broken.semanticModelAvailable(), "last valid semantic model remains available");
    expect(broken.usedPreviousSemanticModel(), "snapshot identifies semantic fallback");
    expect(broken.generation() > good.generation(), "snapshot generations are monotonic");

    const auto cancelled = service.analyzeProject(project, 1, [] { return true; });
    expect(cancelled.cancelled(), "cancelled request is marked");
    expect(!cancelled.currentAnalysisSucceeded(), "cancelled request does not succeed");
    expect(cancelled.semanticModelAvailable(), "cancelled request retains last valid semantics");

    if (failures == 0)
    {
        std::cout << "PASS: immutable analysis snapshots, cancellation and semantic fallback\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
