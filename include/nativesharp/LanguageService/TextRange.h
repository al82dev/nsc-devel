#pragma once

#include "nativesharp/LanguageService/TextPosition.h"

namespace nativesharp::language_service
{
struct TextRange final
{
    TextPosition start;
    TextPosition end;

    friend constexpr bool operator==(TextRange, TextRange) noexcept = default;
};
}
