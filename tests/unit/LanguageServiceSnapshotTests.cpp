#include "nativesharp/LanguageService/LanguageService.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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

void writeText(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
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

    const std::filesystem::path compatibilityRoot =
        std::filesystem::temp_directory_path() / "nativesharp_lsp_compatibility_snapshot";
    std::error_code ignored;
    std::filesystem::remove_all(compatibilityRoot, ignored);
    const std::filesystem::path compatibilityProject = compatibilityRoot / "Universal.nsproj";
    const std::filesystem::path compatibilitySource = compatibilityRoot / "Main.ns";
    const auto projectText = [](bool spaced) {
        return spaced
            ? "<Project>\n <PropertyGroup><Name>LspUniversal</Name></PropertyGroup>\n"
              " <ItemGroup><Compile Include=\"Main.ns\"/></ItemGroup>\n</Project>"
            : "<Project><PropertyGroup><Name>LspUniversal</Name></PropertyGroup>"
              "<ItemGroup><Compile Include=\"Main.ns\"/></ItemGroup></Project>";
    };
    writeText(compatibilityProject, projectText(false));
    writeText(compatibilitySource, "int Answer() { return 42; }\n");
    LanguageService compatibilityService;
    const auto initial = compatibilityService.analyzeProject(compatibilityProject, 1);
    expect(initial.currentAnalysisSucceeded(),
        "language service publishes a snapshot without a language edition");
    error.clear();
    expect(compatibilityService.openDocument(
            compatibilitySource, "\"unterminated overlay", 1, error),
        "compatibility cache fixture opens an invalid source overlay");
    writeText(compatibilityProject, projectText(true));
    const auto brokenCompatibility =
        compatibilityService.analyzeProject(compatibilityProject, 1);
    expect(!brokenCompatibility.currentAnalysisSucceeded(),
        "project formatting does not hide an invalid source overlay");
    expect(brokenCompatibility.semanticModelAvailable() &&
            brokenCompatibility.usedPreviousSemanticModel(),
        "semantic fallback is keyed only by project identity");
    std::filesystem::remove_all(compatibilityRoot, ignored);

    if (failures == 0)
    {
        std::cout << "PASS: immutable analysis snapshots, cancellation and semantic fallback\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
