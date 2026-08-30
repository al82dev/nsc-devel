#pragma once

#include "nativesharp/Sema/ConstantValue.h"

namespace nativesharp::sema
{
class Symbol;
}

namespace nativesharp::mir
{
class MirGlobal final
{
public:
    explicit MirGlobal(sema::Symbol* symbol);

    sema::Symbol* symbol() const noexcept;
    bool hasExplicitInitializer() const noexcept;
    void setExplicitInitializer(sema::ConstantValue value);
    const sema::ConstantValue& initializer() const noexcept;

private:
    sema::Symbol* symbol_;
    bool hasExplicitInitializer_ = false;
    sema::ConstantValue initializer_;
};
}
