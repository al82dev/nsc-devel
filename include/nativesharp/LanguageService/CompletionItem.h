#pragma once

#include "nativesharp/LanguageService/LanguageSymbolKind.h"

#include <string>

namespace nativesharp::language_service
{
struct CompletionItem final
{
    std::string label;
    std::string detail;
    LanguageSymbolKind kind = LanguageSymbolKind::Variable;
};
}
