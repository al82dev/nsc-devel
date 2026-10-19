#pragma once

#include "nativesharp/LanguageService/LanguageLocation.h"
#include "nativesharp/LanguageService/SignatureInformation.h"

#include <cstddef>
#include <vector>

namespace nativesharp::language_service
{
struct IndexedCall final
{
    LanguageLocation location;
    SignatureInformation signature;
    std::vector<TextRange> argumentRanges;
    std::vector<std::size_t> argumentParameterOrdinals;
};
}
