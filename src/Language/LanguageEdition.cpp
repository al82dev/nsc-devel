#include "nativesharp/Language/LanguageEdition.h"

namespace nativesharp::language
{
std::optional<LanguageEdition> parseLanguageEdition(std::string_view text) noexcept
{
    if (text == "2") return LanguageEdition::V2;
    if (text == "3") return LanguageEdition::V3;
    return std::nullopt;
}
}
