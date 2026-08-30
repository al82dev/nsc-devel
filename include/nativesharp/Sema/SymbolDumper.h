#pragma once

#include <iosfwd>
#include <span>

namespace nativesharp::sema
{
class Symbol;

class SymbolDumper final
{
public:
    void dump(std::span<Symbol* const> symbols, std::ostream& output) const;
};
}
