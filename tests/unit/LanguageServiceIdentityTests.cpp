#include "nativesharp/LanguageService/LanguageService.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
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
    nativesharp::language_service::LanguageService service;
    const auto identity = service.identity();

    expect(identity.compilerRelease == "3.0.0", "compiler release is published");
    expect(identity.languageVersion == "3.0.0", "language version is published");
    expect(
        identity.defaultLanguageEdition == nativesharp::language::LanguageEdition::V2,
        "default language edition is published");
    expect(identity.supportedLanguageEditions == "2,3", "supported language editions are published");
    expect(identity.compilerStage == "70", "compiler stage is published");
    expect(identity.target == "x86_64-pc-windows-msvc", "Windows x64 target is published");
    expect(
        identity.analysisAbi == nativesharp::language_service::kAnalysisAbiVersion,
        "analysis ABI is published");
    expect(identity.buildId.size() == 16, "build identity contains 16 hexadecimal digits");
    expect(
        std::ranges::all_of(identity.buildId, [](char value) {
            return std::isdigit(static_cast<unsigned char>(value)) != 0 ||
                (value >= 'a' && value <= 'f');
        }),
        "build identity is lowercase hexadecimal");

    if (failures == 0)
    {
        std::cout << "PASS: language service identity contract\n";
    }
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
