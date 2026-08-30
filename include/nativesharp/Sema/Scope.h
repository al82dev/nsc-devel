#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace nativesharp::sema
{
class Symbol;

class Scope final
{
public:
    explicit Scope(Scope* parent) noexcept;

    Scope* parent() const noexcept;
    bool declare(Symbol* symbol);
    Symbol* lookupLocal(std::string_view name) const noexcept;
    Symbol* lookup(std::string_view name) const noexcept;

private:
    Scope* parent_;
    std::unordered_map<std::string, Symbol*> symbols_;
};
}
