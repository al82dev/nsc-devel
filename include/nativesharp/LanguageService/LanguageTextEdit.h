#pragma once

#include "nativesharp/LanguageService/LanguageLocation.h"

#include <string>

namespace nativesharp::language_service
{
struct LanguageTextEdit final
{
    LanguageLocation location;
    std::string newText;
};
}
