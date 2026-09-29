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
    expect(good.languageEdition() == nativesharp::language::LanguageEdition::V2,
        "snapshot identity retains the analyzed project edition");
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

    const std::filesystem::path editionRoot =
        std::filesystem::temp_directory_path() / "nativesharp_lsp_edition_snapshot";
    std::error_code ignored;
    std::filesystem::remove_all(editionRoot, ignored);
    const std::filesystem::path editionProject = editionRoot / "Edition.nsproj";
    const std::filesystem::path editionSource = editionRoot / "Main.ns";
    const auto projectText = [](std::string_view edition) {
        return "<Project><PropertyGroup><Name>LspEdition</Name><Language>" +
            std::string(edition) +
            "</Language></PropertyGroup><ItemGroup><Compile Include=\"Main.ns\"/>"
            "</ItemGroup></Project>";
    };
    writeText(editionProject, projectText("3"));
    writeText(editionSource, "int Answer() { return 42; }\n");
    LanguageService editionService;
    const auto edition3 = editionService.analyzeProject(editionProject, 1);
    expect(edition3.currentAnalysisSucceeded() &&
            edition3.languageEdition() == nativesharp::language::LanguageEdition::V3,
        "language service snapshot publishes edition 3");
    error.clear();
    expect(editionService.openDocument(editionSource, "\"unterminated overlay", 1, error),
        "edition cache fixture opens an invalid source overlay");
    writeText(editionProject, projectText("2"));
    const auto edition2Broken = editionService.analyzeProject(editionProject, 1);
    expect(edition2Broken.languageEdition() == nativesharp::language::LanguageEdition::V2 &&
            !edition2Broken.currentAnalysisSucceeded(),
        "language service observes a project edition change before semantic fallback");
    expect(!edition2Broken.semanticModelAvailable() &&
            !edition2Broken.usedPreviousSemanticModel(),
        "semantic fallback never crosses a language-edition identity boundary");
    std::filesystem::remove_all(editionRoot, ignored);

    if (failures == 0)
    {
        std::cout << "PASS: immutable analysis snapshots, cancellation and semantic fallback\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
