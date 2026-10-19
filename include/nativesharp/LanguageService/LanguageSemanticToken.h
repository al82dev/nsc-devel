#pragma once

#include "nativesharp/LanguageService/LanguageSymbolKind.h"
#include "nativesharp/LanguageService/TextRange.h"

namespace nativesharp::language_service
{
struct LanguageSemanticToken final
{
    TextRange range;
    LanguageSymbolKind kind = LanguageSymbolKind::Variable;
    bool declaration = false;
};
}
