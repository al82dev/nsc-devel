#pragma once

#include <optional>
#include <string_view>

namespace nativesharp::language
{
enum class LanguageEdition
{
    V2 = 2,
    V3 = 3
};

constexpr std::string_view languageEditionName(LanguageEdition edition) noexcept
{
    switch (edition)
    {
        case LanguageEdition::V2: return "2";
        case LanguageEdition::V3: return "3";
    }
    return "2";
}

constexpr bool languageEditionSupports(
    LanguageEdition selected,
    LanguageEdition required) noexcept
{
    return static_cast<int>(selected) >= static_cast<int>(required);
}

constexpr std::string_view languageEditionFingerprint(LanguageEdition edition) noexcept
{
    return edition == LanguageEdition::V3 ? "nativesharp-language-edition-3" :
                                            "nativesharp-language-edition-2";
}

std::optional<LanguageEdition> parseLanguageEdition(std::string_view text) noexcept;
}
