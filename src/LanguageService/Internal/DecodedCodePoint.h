#pragma once

#include <cstddef>
#include <cstdint>

namespace nativesharp::language_service
{
struct DecodedCodePoint final
{
    std::uint32_t value = 0xFFFDU;
    std::size_t bytes = 1;
};
}
