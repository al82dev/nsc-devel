#pragma once

#include <vector>

namespace nativesharp::mir
{
class MirFunction;
}

namespace nativesharp::sema
{
class Symbol;
}

namespace nativesharp::ssa
{
class SsaPromotability final
{
public:
    std::vector<sema::Symbol*> findPromotableSymbols(const mir::MirFunction* function) const;
    bool isPromotable(const mir::MirFunction* function, const sema::Symbol* symbol) const;
};
}
