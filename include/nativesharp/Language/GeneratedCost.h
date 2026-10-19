#pragma once

namespace nativesharp::language
{
enum class GeneratedCost { Allocation, Copy, Drop, DynamicDispatch, UnwindEdge, AtomicFence };
}
