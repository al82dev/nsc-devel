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
    using namespace nativesharp::language_service;
    LanguageService service;
    const std::filesystem::path directory =
        std::filesystem::path(NSC_SOURCE_DIR) / "examples" / "project_stage27" / "App";
    const std::filesystem::path project = directory / "Stage27App.nsproj";
    const std::filesystem::path source = directory / "B_Run.ns";

    const AnalysisSnapshot snapshot = service.analyzeProject(project, 2);
    expect(snapshot.currentAnalysisSucceeded(), "Stage 27 project analyzes successfully");

    const auto completions = snapshot.completions(source, "Ad");
    expect(completions.size() == 2, "completion retains both semantic Add overloads");
    expect(std::ranges::all_of(completions, [](const CompletionItem& item) {
        return item.label == "Add" && item.kind == LanguageSymbolKind::Method;
    }), "completion items carry method kinds and semantic details");

    const TextPosition firstAdd{6, 20};
    const auto signature = snapshot.signatureHelp(source, TextPosition{6, 25});
    expect(signature.has_value() && signature->signatures.size() == 1,
        "resolved call exposes one selected signature");
    expect(signature.has_value() && signature->signatures[0].label.find("builtin:int") != std::string::npos,
        "signature label is sourced from the selected int overload");
    expect(signature.has_value() && signature->activeParameter == 0,
        "signature help maps the active argument to its parameter");

    const auto references = snapshot.references(source, firstAdd);
    expect(references.size() >= 5, "overload-family references include declarations and all Add calls");
    const auto renameRange = snapshot.prepareRename(source, firstAdd);
    expect(renameRange.has_value() && renameRange->start == TextPosition{6, 20} &&
        renameRange->end == TextPosition{6, 23}, "prepare rename returns only the identifier token");

    const RenameResult rename = snapshot.rename(source, firstAdd, "Sum");
    expect(rename.allowed && rename.edits.size() >= 5, "safe rename returns a workspace edit set");
    expect(std::ranges::all_of(rename.edits, [](const LanguageTextEdit& edit) {
        return edit.newText == "Sum" && edit.location.range.start.line == edit.location.range.end.line &&
            edit.location.range.end.character - edit.location.range.start.character == 3;
    }), "every rename edit is an exact Add identifier range");
    expect(!snapshot.rename(source, firstAdd, "class").allowed,
        "rename rejects a NativeSharp keyword");

    const auto tokens = snapshot.semanticTokens(source);
    expect(std::ranges::any_of(tokens, [&](const LanguageSemanticToken& token) {
        return !token.declaration && token.kind == LanguageSymbolKind::Method &&
            token.range.start == firstAdd;
    }), "semantic tokens classify the selected Add use as a method");

    if (failures == 0)
        std::cout << "PASS: semantic completion, signatures, references, rename and tokens\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
