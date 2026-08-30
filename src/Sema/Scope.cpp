#include "nativesharp/Sema/Scope.h"

#include "nativesharp/Sema/Symbol.h"

#include <string>

namespace nativesharp::sema
{
Scope::Scope(Scope* parent) noexcept
    : parent_(parent)
{
}

Scope* Scope::parent() const noexcept { return parent_; }

bool Scope::declare(Symbol* symbol)
{
    return symbols_.emplace(symbol->name(), symbol).second;
}

Symbol* Scope::lookupLocal(std::string_view name) const noexcept
{
    const auto iterator = symbols_.find(std::string(name));
    return iterator == symbols_.end() ? nullptr : iterator->second;
}

Symbol* Scope::lookup(std::string_view name) const noexcept
{
    const Scope* scope = this;
    while (scope != nullptr)
    {
        if (Symbol* symbol = scope->lookupLocal(name))
        {
            return symbol;
        }
        scope = scope->parent_;
    }
    return nullptr;
}
}
