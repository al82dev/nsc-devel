#pragma once

#include "nativesharp/Language/SimdLaneKind.h"

#include <cstddef>
#include <string>

namespace nativesharp::language
{
struct PortableSimdType final
{
    SimdLaneKind laneKind = SimdLaneKind::SignedInteger;
    std::string laneTypeKey;
    std::size_t laneBits = 0;
    std::size_t laneCount = 0;
    bool mask = false;
};
}
