#pragma once

#include <cstdint>

namespace nativesharp::language_service
{
struct DocumentFormattingOptions final
{
    std::uint32_t tabSize = 4;
    bool insertSpaces = true;
};
}
