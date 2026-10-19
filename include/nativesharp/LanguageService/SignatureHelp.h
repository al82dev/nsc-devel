#pragma once

#include "nativesharp/LanguageService/SignatureInformation.h"

#include <cstddef>
#include <vector>

namespace nativesharp::language_service
{
struct SignatureHelp final
{
    std::vector<SignatureInformation> signatures;
    std::size_t activeSignature = 0;
    std::size_t activeParameter = 0;
};
}
