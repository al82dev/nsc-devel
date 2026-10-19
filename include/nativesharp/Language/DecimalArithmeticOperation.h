#pragma once

namespace nativesharp::language
{
// NS304-T15-F07: the single static helper for decimal arithmetic (the stable ABI that consteval and
// runtime both use, so they cannot diverge). Coefficients operate on magnitudes (sign is bit 127);
// add/sub/rem align to the max scale, mul sums scales, div computes the target scale and rounds
// half-up. Scale must stay 0..28 and the coefficient must fit 96 bits.
enum class DecimalArithmeticOperation { Add, Subtract, Multiply, Divide, Remainder };
}
