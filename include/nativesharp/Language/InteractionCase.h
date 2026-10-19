#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nativesharp/Language/FeatureArea.h"
#include "nativesharp/Language/InteractionRisk.h"

namespace nativesharp::language
{
struct InteractionCase final
{
    std::string identity;
    std::vector<FeatureArea> areas;
    std::uint64_t deterministicSeed = 0;
    InteractionRisk risk = InteractionRisk::High;
    bool changesMir = true;
    bool changesOwnership = true;
    bool minimizesOnFailure = true;
};
}
