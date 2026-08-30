#include "nativesharp/SSA/SsaArena.h"

#include "nativesharp/SSA/SsaFunction.h"
#include "nativesharp/SSA/SsaModule.h"

namespace nativesharp::ssa
{
SsaArena::~SsaArena()
{
    for (SsaFunction* function : functions_)
    {
        delete function;
    }
    for (SsaModule* module : modules_)
    {
        delete module;
    }
}

mir::MirArena& SsaArena::mirArena() noexcept { return mirArena_; }

SsaModule* SsaArena::makeModule(const mir::MirModule* sourceModule, mir::MirModule* module)
{
    SsaModule* value = new SsaModule(sourceModule, module);
    modules_.push_back(value);
    return value;
}

SsaFunction* SsaArena::makeFunction(
    const mir::MirFunction* sourceFunction,
    mir::MirFunction* function,
    const cfg::CfgFunction* cfgFunction,
    const dominance::DominanceFunction* dominanceFunction)
{
    SsaFunction* value = new SsaFunction(
        sourceFunction,
        function,
        cfgFunction,
        dominanceFunction);
    functions_.push_back(value);
    return value;
}
}
