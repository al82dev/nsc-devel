#include "nativesharp/Passes/OptimizationArena.h"

#include "nativesharp/Passes/OptimizationModule.h"

namespace nativesharp::passes
{
OptimizationArena::~OptimizationArena()
{
    for (OptimizationModule* module : modules_)
    {
        delete module;
    }
}

mir::MirArena& OptimizationArena::mirArena() noexcept { return mirArena_; }
cfg::CfgArena& OptimizationArena::cfgArena() noexcept { return cfgArena_; }

dominance::DominanceArena& OptimizationArena::dominanceArena() noexcept
{
    return dominanceArena_;
}

OptimizationModule* OptimizationArena::makeModule(
    const ssa::SsaModule* sourceSsa,
    mir::MirModule* module)
{
    OptimizationModule* value = new OptimizationModule(sourceSsa, module, &mirArena_);
    modules_.push_back(value);
    return value;
}
}
