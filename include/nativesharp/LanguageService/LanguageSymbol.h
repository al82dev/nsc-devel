#pragma once

#include "nativesharp/LanguageService/LanguageLocation.h"
#include "nativesharp/LanguageService/LanguageSymbolKind.h"

#include <string>

namespace nativesharp::language_service
{
struct LanguageSymbol final
{
    std::string name;
    std::string qualifiedName;
    std::string detail;
    LanguageSymbolKind kind = LanguageSymbolKind::Variable;
    LanguageLocation location;
    TextRange selectionRange;
    bool renameable = false;
};
}
