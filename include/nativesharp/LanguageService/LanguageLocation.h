#pragma once

#include "nativesharp/LanguageService/TextRange.h"

#include <filesystem>

namespace nativesharp::language_service
{
struct LanguageLocation final
{
    std::filesystem::path path;
    TextRange range;
};
}
