#pragma once

#include "nativesharp/LanguageService/LanguageLocation.h"

#include <cstddef>
#include <vector>

namespace nativesharp::language_service
{
struct IndexedReference final
{
    LanguageLocation location;
    std::vector<std::size_t> targetSymbolIndices;
    bool exactIdentifierRange = false;
};
}
