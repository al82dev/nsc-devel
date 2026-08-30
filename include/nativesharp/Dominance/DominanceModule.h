#pragma once

#include <unordered_map>
#include <vector>

namespace nativesharp::cfg
{
class CfgFunction;
class CfgModule;
}

namespace nativesharp::dominance
{
class DominanceFunction;

class DominanceModule final
{
public:
    explicit DominanceModule(const cfg::CfgModule* cfgModule);

    const cfg::CfgModule* cfgModule() const noexcept;
    void addFunction(DominanceFunction* function);
    const std::vector<DominanceFunction*>& functions() const noexcept;
    DominanceFunction* findFunction(const cfg::CfgFunction* function) const noexcept;

private:
    const cfg::CfgModule* cfgModule_;
    std::vector<DominanceFunction*> functions_;
    std::unordered_map<const cfg::CfgFunction*, DominanceFunction*> functionsByCfg_;
};
}
