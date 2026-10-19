#pragma once

#include <cstdint>

namespace nativesharp::language
{
// NS304-T15-F02: the exact value of a decimal literal spelled in source. The digits and scale are
// recovered verbatim from the text (no binary floating-point round-trip), so `0.1` is exactly
// coefficient 1 / scale 1 and round-trips text→decimal→text. Scale must be 0..28 and the coefficient
// must fit 96 bits.
struct DecimalLiteralValue final
{
    std::uint64_t coefficientLow = 0;
    std::uint32_t coefficientHigh = 0;
    unsigned scale = 0;
    bool negative = false;
};
}
