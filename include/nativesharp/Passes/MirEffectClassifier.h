#pragma once

#include "nativesharp/Passes/MirEffectInfo.h"

namespace nativesharp::mir
{
class MirInstruction;
}

namespace nativesharp::passes
{
class MirEffectClassifier final
{
public:
    static MirEffectInfo classify(const mir::MirInstruction* instruction) noexcept;
    static bool removableWhenUnused(const mir::MirInstruction* instruction) noexcept;
};
}
