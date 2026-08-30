#include "nativesharp/Dominance/DominanceModule.h"

#include "nativesharp/Dominance/DominanceFunction.h"

namespace nativesharp::dominance
{
DominanceModule::DominanceModule(const cfg::CfgModule* cfgModule) : cfgModule_(cfgModule) {}
const cfg::CfgModule* DominanceModule::cfgModule() const noexcept { return cfgModule_; }

void DominanceModule::addFunction(DominanceFunction* function)
{
    functions_.push_back(function);
    if (function != nullptr && function->cfgFunction() != nullptr)
    {
        functionsByCfg_.emplace(function->cfgFunction(), function);
    }
}

const std::vector<DominanceFunction*>& DominanceModule::functions() const noexcept { return functions_; }

DominanceFunction* DominanceModule::findFunction(const cfg::CfgFunction* function) const noexcept
{
    const auto found = functionsByCfg_.find(function);
    return found == functionsByCfg_.end() ? nullptr : found->second;
}
}
