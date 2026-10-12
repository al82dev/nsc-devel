#include "nativesharp/Sema/SymbolDumper.h"

#include "nativesharp/Sema/SemanticDumper.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/Type.h"

#include <ostream>

namespace nativesharp::sema
{
namespace
{
void dumpSymbol(const Symbol* symbol, std::ostream& output, int depth)
{
    for (int index = 0; index < depth; ++index)
    {
        output << "  ";
    }

    output << symbolKindName(symbol->kind()) << ' ' << symbol->name();
    if (symbol->type() != nullptr)
    {
        output << " : " << symbol->type()->displayName();
    }
    if (symbol->isConst()) output << " const";
    if (symbol->isExternC()) output << " extern-C";
    if (symbol->isDefined()) output << " defined";
    if (symbol->kind() == SymbolKind::EnumMember) output << " = " << symbol->enumValue();
    if (symbol->kind() == SymbolKind::Field && symbol->hasFieldOffset())
    {
        output << " offset=" << symbol->fieldOffset();
    }
    if (symbol->hasConstantValue())
    {
        output << " constant=";
        SemanticDumper::dumpConstant(symbol->constantValue(), output);
    }
    output << '\n';

    for (const Symbol* member : symbol->members())
    {
        dumpSymbol(member, output, depth + 1);
    }
}
}

void SymbolDumper::dump(std::span<Symbol* const> symbols, std::ostream& output) const
{
    for (const Symbol* symbol : symbols)
    {
        dumpSymbol(symbol, output, 0);
    }
}
}
