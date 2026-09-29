#include "nativesharp/LanguageService/LanguageService.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
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
        std::filesystem::path(NSC_SOURCE_DIR) / "examples" / "project_stage27" / "App";
    const std::filesystem::path project = directory / "Stage27App.nsproj";
    const std::filesystem::path source = directory / "B_Run.ns";

    const auto snapshot = service.analyzeProject(project, 2);
    expect(snapshot.currentAnalysisSucceeded(), "Stage 27 project analyzes successfully");

    const auto document = snapshot.documentSymbols(source);
    expect(std::ranges::any_of(document, [](const auto& symbol) { return symbol.name == "Run"; }),
        "document symbols contain Run");
    expect(std::ranges::any_of(document, [](const auto& symbol) { return symbol.name == "total"; }),
        "document symbols contain local total");

    const auto workspace = snapshot.workspaceSymbols("Add");
    expect(workspace.size() >= 2, "workspace search returns both Add overloads");

    const nativesharp::language_service::TextPosition firstAdd{6, 20};
    const auto hover = snapshot.symbolAt(source, firstAdd);
    expect(hover.has_value() && hover->name == "Add", "symbol at call resolves selected Add");
    expect(hover.has_value() && hover->detail.find("builtin:int") != std::string::npos,
        "hover detail contains semantic result type");
    const auto definitions = snapshot.definitions(source, firstAdd);
    expect(definitions.size() == 1, "resolved call has one definition");
    expect(!definitions.empty() && definitions[0].path.filename() == "A_StateAndOverloads.ns",
        "definition points to declaring source");

    if (failures == 0)
    {
        std::cout << "PASS: semantic document/workspace symbols, hover and definition\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
