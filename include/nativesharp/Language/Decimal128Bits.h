#pragma once

#include <cstdint>

namespace nativesharp::language
{
// Stores the exact NativeSharp decimal payload as two little-endian 64-bit words.
// `low` contains coefficient bits 0..63. `high` contains coefficient bits 64..95,
// scale bits 96..101, reserved zero bits 102..126, and the sign at bit 127.
struct Decimal128Bits final
{
    std::uint64_t low = 0;
    std::uint64_t high = 0;
};
}
