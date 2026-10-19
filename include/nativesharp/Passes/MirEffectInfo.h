#pragma once

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
}
