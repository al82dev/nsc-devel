#include "nativesharp/LanguageService/LanguageService.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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
    const std::filesystem::path path = "A:/actions/Example.ns";
    const std::string source = "namespace Demo {\n@int value;   \n}\n";
    std::string error;
    expect(service.openDocument(path, source, 1, error), "document opens for code actions");
    const std::vector<std::string> codes{"UnexpectedCharacter"};
    const auto actions = service.codeActions(path, TextRange{{1, 0}, {1, 1}}, codes);
    const auto quickFix = std::ranges::find_if(actions, [](const LanguageCodeAction& action) {
        return action.kind == LanguageCodeActionKind::QuickFix;
    });
    expect(quickFix != actions.end() && quickFix->edits.size() == 1 &&
        quickFix->edits[0].newText.empty(), "unexpected character offers an explicit deletion");
    const auto fixAll = std::ranges::find_if(actions, [](const LanguageCodeAction& action) {
        return action.kind == LanguageCodeActionKind::SourceFixAll;
    });
    expect(fixAll != actions.end() && fixAll->edits.size() == 1,
        "unformatted document offers a source fix-all action");

    const std::vector<std::string> unrelated{"UnknownIdentifier"};
    const auto safeActions = service.codeActions(path, TextRange{{1, 1}, {1, 4}}, unrelated);
    expect(std::ranges::none_of(safeActions, [](const LanguageCodeAction& action) {
        return action.kind == LanguageCodeActionKind::QuickFix;
    }), "unsupported diagnostics never produce speculative quick fixes");

    if (failures == 0) std::cout << "PASS: safe NativeSharp code actions\n";
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
