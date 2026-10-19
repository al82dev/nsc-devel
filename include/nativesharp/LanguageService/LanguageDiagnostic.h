#pragma once

#include "nativesharp/LanguageService/LanguageDiagnosticSeverity.h"
#include "nativesharp/LanguageService/TextRange.h"

#include <filesystem>
#include <string>

namespace nativesharp::language_service
{
struct LanguageDiagnostic final
{
    std::filesystem::path path;
    TextRange range;
    LanguageDiagnosticSeverity severity = LanguageDiagnosticSeverity::Error;
    std::string code;
    std::string message;
};
}
