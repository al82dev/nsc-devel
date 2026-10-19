#pragma once

#include <cstdint>

namespace nativesharp::language
{
// These values are the public NativeSharp.Core.Threading.MemoryOrder ABI. They are deliberately
// independent from LLVM's numeric ordering constants.
enum class MemoryOrder : std::uint8_t
{
    Relaxed = 0,
    Acquire = 1,
    Release = 2,
    AcquireRelease = 3,
    SequentiallyConsistent = 4
};
}
