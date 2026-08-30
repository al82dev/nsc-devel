#include "nativesharp/SSA/SsaModule.h"

#include "nativesharp/SSA/SsaFunction.h"

namespace nativesharp::ssa
{
SsaModule::SsaModule(const mir::MirModule* sourceModule, mir::MirModule* module)
    : sourceModule_(sourceModule), module_(module)
{
}

const mir::MirModule* SsaModule::sourceModule() const noexcept { return sourceModule_; }
mir::MirModule* SsaModule::module() const noexcept { return module_; }

void SsaModule::addFunction(SsaFunction* function)
{
    functions_.push_back(function);
    if (function != nullptr && function->sourceFunction() != nullptr)
    {
        functionsBySource_.emplace(function->sourceFunction(), function);
    }
}

const std::vector<SsaFunction*>& SsaModule::functions() const noexcept { return functions_; }

SsaFunction* SsaModule::findFunction(const mir::MirFunction* sourceFunction) const noexcept
{
    const auto found = functionsBySource_.find(sourceFunction);
    return found == functionsBySource_.end() ? nullptr : found->second;
}
}
