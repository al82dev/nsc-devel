#pragma once

#include "nativesharp/MIR/MirArena.h"

#include <vector>

namespace nativesharp::cfg
{
class CfgFunction;
}

namespace nativesharp::dominance
{
class DominanceFunction;
}

namespace nativesharp::mir
{
class MirFunction;
class MirModule;
}

namespace nativesharp::ssa
{
class SsaFunction;
class SsaModule;

class SsaArena final
{
public:
    SsaArena() = default;
    SsaArena(const SsaArena&) = delete;
    SsaArena& operator=(const SsaArena&) = delete;
    ~SsaArena();

    mir::MirArena& mirArena() noexcept;

    SsaModule* makeModule(const mir::MirModule* sourceModule, mir::MirModule* module);
    SsaFunction* makeFunction(
        const mir::MirFunction* sourceFunction,
        mir::MirFunction* function,
        const cfg::CfgFunction* cfgFunction,
        const dominance::DominanceFunction* dominanceFunction);

private:
    mir::MirArena mirArena_;
    std::vector<SsaModule*> modules_;
    std::vector<SsaFunction*> functions_;
};
}
