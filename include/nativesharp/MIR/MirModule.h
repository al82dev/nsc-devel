#pragma once

#include <vector>

namespace nativesharp::sema
{
class Symbol;
}

namespace nativesharp::mir
{
class MirFunction;
class MirGlobal;

class MirModule final
{
public:
    void addGlobal(MirGlobal* global);
    void addFunction(MirFunction* function);

    const std::vector<MirGlobal*>& globals() const noexcept;
    const std::vector<MirFunction*>& functions() const noexcept;
    MirFunction* findFunction(const sema::Symbol* symbol) const noexcept;

private:
    std::vector<MirGlobal*> globals_;
    std::vector<MirFunction*> functions_;
};
}
