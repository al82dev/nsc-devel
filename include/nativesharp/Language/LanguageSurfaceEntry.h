#pragma once

#include "nativesharp/Language/LanguageDisposition.h"
#include "nativesharp/Language/LanguageFeature.h"

#include <string_view>

namespace nativesharp::language
{
struct LanguageSurfaceEntry final
{
    LanguageFeature feature;
    std::string_view name;
    LanguageDisposition disposition;
    std::string_view rationale;
};
}
