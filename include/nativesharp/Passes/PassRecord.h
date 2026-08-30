#pragma once

#include <cstddef>
#include <string>

namespace nativesharp::passes
{
struct PassRecord
{
    std::size_t iteration = 0;
    std::string name;
    bool changed = false;
    std::size_t transformations = 0;
    std::size_t instructionsBefore = 0;
    std::size_t instructionsAfter = 0;
    std::size_t blocksBefore = 0;
    std::size_t blocksAfter = 0;
    bool preservedCfg = false;
    bool preservedDominance = false;
    std::size_t cfgBuildsAfter = 0;
    std::size_t dominanceBuildsAfter = 0;
};
}
