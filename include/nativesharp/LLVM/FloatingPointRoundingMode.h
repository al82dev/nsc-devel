#pragma once

namespace nativesharp::llvm_backend
{
enum class FloatingPointRoundingMode
{
    Dynamic,
    NearestEven,
    TowardZero,
    Upward,
    Downward
};
}
