#include "nativesharp/MIR/MirModule.h"

#include "nativesharp/MIR/MirFunction.h"

namespace nativesharp::mir
{
void MirModule::addGlobal(MirGlobal* global) { globals_.push_back(global); }
void MirModule::addFunction(MirFunction* function) { functions_.push_back(function); }
const std::vector<MirGlobal*>& MirModule::globals() const noexcept { return globals_; }
const std::vector<MirFunction*>& MirModule::functions() const noexcept { return functions_; }
MirFunction* MirModule::findFunction(const sema::Symbol* symbol) const noexcept
{
    for (MirFunction* function : functions_)
    {
        if (function != nullptr && function->symbol() == symbol)
        {
            return function;
        }
    }
    return nullptr;
}
}
