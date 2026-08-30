#include "nativesharp/MIR/MirGlobal.h"

#include <utility>

namespace nativesharp::mir
{
MirGlobal::MirGlobal(sema::Symbol* symbol) : symbol_(symbol) {}
sema::Symbol* MirGlobal::symbol() const noexcept { return symbol_; }
bool MirGlobal::hasExplicitInitializer() const noexcept { return hasExplicitInitializer_; }
void MirGlobal::setExplicitInitializer(sema::ConstantValue value)
{
    initializer_ = std::move(value);
    hasExplicitInitializer_ = true;
}
const sema::ConstantValue& MirGlobal::initializer() const noexcept { return initializer_; }
}
