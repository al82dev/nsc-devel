#include "nativesharp/CFG/CfgModule.h"

#include "nativesharp/CFG/CfgFunction.h"

namespace nativesharp::cfg
{
CfgModule::CfgModule(const mir::MirModule* mirModule) : mirModule_(mirModule) {}
const mir::MirModule* CfgModule::mirModule() const noexcept { return mirModule_; }
void CfgModule::addFunction(CfgFunction* function)
{
    functions_.push_back(function);
    if (function != nullptr && function->mirFunction() != nullptr)
    {
        functionsByMir_.emplace(function->mirFunction(), function);
    }
}
const std::vector<CfgFunction*>& CfgModule::functions() const noexcept { return functions_; }

CfgFunction* CfgModule::findFunction(const mir::MirFunction* function) const noexcept
{
    const auto found = functionsByMir_.find(function);
    return found == functionsByMir_.end() ? nullptr : found->second;
}
}
