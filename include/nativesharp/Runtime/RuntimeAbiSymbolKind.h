#pragma once

#include <cstdint>

namespace nativesharp::runtime
{
enum class RuntimeAbiSymbolKind : std::uint8_t
{
    Function,
    Data
};
}
