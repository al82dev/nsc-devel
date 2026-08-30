#pragma once

namespace nativesharp::mir
{
class MirInstruction;
}

namespace nativesharp::passes
{
struct MirEffectInfo
{
    bool hasSideEffects = false;
    bool mayTrap = false;
    bool readsMemory = false;
    bool writesMemory = false;
    bool structural = false;
};

class MirEffectClassifier final
{
public:
    static MirEffectInfo classify(const mir::MirInstruction* instruction) noexcept;
    static bool removableWhenUnused(const mir::MirInstruction* instruction) noexcept;
};
}
