#pragma once

#include "nativesharp/LanguageService/LanguageCodeActionKind.h"
#include "nativesharp/LanguageService/LanguageTextEdit.h"

#include <string>
#include <vector>

namespace nativesharp::language_service
{
struct LanguageCodeAction final
{
    std::string title;
    LanguageCodeActionKind kind = LanguageCodeActionKind::QuickFix;
    bool preferred = false;
    std::vector<std::string> diagnosticCodes;
    std::vector<LanguageTextEdit> edits;
};
}
