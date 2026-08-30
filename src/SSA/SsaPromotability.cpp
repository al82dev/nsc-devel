#include "nativesharp/SSA/SsaPromotability.h"

#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"

namespace nativesharp::ssa
{
namespace
{
bool isFunctionStorage(const sema::Symbol* symbol) noexcept
{
    if (symbol == nullptr)
    {
        return false;
    }
    return symbol->kind() == sema::SymbolKind::Variable ||
        symbol->kind() == sema::SymbolKind::Parameter;
}
}

std::vector<sema::Symbol*> SsaPromotability::findPromotableSymbols(
    const mir::MirFunction* function) const
{
    std::vector<sema::Symbol*> result;
    if (function == nullptr || !function->defined())
    {
        return result;
    }

    for (sema::Symbol* symbol : function->locals())
    {
        if (isPromotable(function, symbol))
        {
            result.push_back(symbol);
        }
    }
    return result;
}

bool SsaPromotability::isPromotable(
    const mir::MirFunction* function,
    const sema::Symbol* symbol) const
{
    if (function == nullptr || !function->defined() || !isFunctionStorage(symbol))
    {
        return false;
    }

    bool belongsToFunction = false;
    for (const sema::Symbol* local : function->locals())
    {
        if (local == symbol)
        {
            belongsToFunction = true;
            break;
        }
    }
    if (!belongsToFunction)
    {
        return false;
    }

    for (const mir::MirBlock* block : function->blocks())
    {
        if (block == nullptr)
        {
            continue;
        }
        for (const mir::MirInstruction* instruction : block->instructions())
        {
            if (instruction == nullptr || instruction->symbol() != symbol)
            {
                continue;
            }
            if (instruction->opcode() == mir::MirOpcode::AddressOfSymbol)
            {
                return false;
            }
        }
    }
    return true;
}
}
