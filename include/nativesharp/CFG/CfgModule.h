#pragma once

#include <unordered_map>
#include <vector>

namespace nativesharp::mir
{
class MirFunction;
class MirModule;
}

namespace nativesharp::cfg
{
class CfgFunction;

class CfgModule final
{
public:
    explicit CfgModule(const mir::MirModule* mirModule);

    const mir::MirModule* mirModule() const noexcept;
    void addFunction(CfgFunction* function);
    const std::vector<CfgFunction*>& functions() const noexcept;
    CfgFunction* findFunction(const mir::MirFunction* function) const noexcept;

private:
    const mir::MirModule* mirModule_;
    std::vector<CfgFunction*> functions_;
    std::unordered_map<const mir::MirFunction*, CfgFunction*> functionsByMir_;
};
}
