#pragma once

#include <unordered_map>
#include <vector>

namespace nativesharp::mir
{
class MirFunction;
class MirModule;
}

namespace nativesharp::ssa
{
class SsaFunction;

class SsaModule final
{
public:
    SsaModule(const mir::MirModule* sourceModule, mir::MirModule* module);

    const mir::MirModule* sourceModule() const noexcept;
    mir::MirModule* module() const noexcept;

    void addFunction(SsaFunction* function);
    const std::vector<SsaFunction*>& functions() const noexcept;
    SsaFunction* findFunction(const mir::MirFunction* sourceFunction) const noexcept;

private:
    const mir::MirModule* sourceModule_;
    mir::MirModule* module_;
    std::vector<SsaFunction*> functions_;
    std::unordered_map<const mir::MirFunction*, SsaFunction*> functionsBySource_;
};
}
