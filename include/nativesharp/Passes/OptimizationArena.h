#pragma once

#include "nativesharp/CFG/CfgArena.h"
#include "nativesharp/Dominance/DominanceArena.h"
#include "nativesharp/MIR/MirArena.h"

#include <vector>

namespace nativesharp::mir
{
class MirModule;
}

namespace nativesharp::ssa
{
class SsaModule;
}

namespace nativesharp::passes
{
class OptimizationModule;

class OptimizationArena final
{
public:
    OptimizationArena() = default;
    OptimizationArena(const OptimizationArena&) = delete;
    OptimizationArena& operator=(const OptimizationArena&) = delete;
    ~OptimizationArena();

    mir::MirArena& mirArena() noexcept;
    cfg::CfgArena& cfgArena() noexcept;
    dominance::DominanceArena& dominanceArena() noexcept;

    OptimizationModule* makeModule(
        const ssa::SsaModule* sourceSsa,
        mir::MirModule* module);

private:
    mir::MirArena mirArena_;
    cfg::CfgArena cfgArena_;
    dominance::DominanceArena dominanceArena_;
    std::vector<OptimizationModule*> modules_;
};
}
