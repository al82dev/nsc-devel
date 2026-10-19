#pragma once

#include <cstddef>
#include <string>

#include "nativesharp/Language/WholeProgramWitnessKind.h"

namespace nativesharp::language
{
struct WholeProgramWitness final
{
    std::string identity;
    bool deterministic = true;
    bool supportsIncremental = true;
    bool requiresSdk = false;
    using Kind = WholeProgramWitnessKind;
    Kind kind = Kind::Parser;
    bool debugAndRelease = true;
    bool cleanAndIncremental = true;
    bool ltoAndNoLto = true;
    bool sanitizerReady = true;
    bool partialNoHeap = false;
    std::size_t minimumSourceUnits = 1;
    int expectedExitCode = 42;
};
}
