#include "nativesharp/Passes/MirRewriter.h"

#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirValue.h"

namespace nativesharp::passes
{
std::size_t MirRewriter::replaceAllUses(
    mir::MirModule* module,
    const mir::MirValue* from,
    mir::MirValue* to)
{
    if (module == nullptr || from == nullptr || to == nullptr || from == to)
    {
        return 0;
    }

    std::size_t replacements = 0;
    for (mir::MirFunction* function : module->functions())
    {
        if (function == nullptr || !function->defined())
        {
            continue;
        }

        for (mir::MirBlock* block : function->blocks())
        {
            for (mir::MirInstruction* instruction : block->instructions())
            {
                if (instruction == nullptr)
                {
                    continue;
                }

                for (const mir::MirValue* operand : instruction->operands())
                {
                    if (operand == from)
                    {
                        ++replacements;
                    }
                }
                for (const mir::MirPhiIncoming& incoming : instruction->phiIncoming())
                {
                    if (incoming.value == from)
                    {
                        ++replacements;
                    }
                }
                instruction->replaceValueUse(from, to);
            }
        }
    }
    return replacements;
}

std::size_t MirRewriter::instructionCount(const mir::MirModule* module) noexcept
{
    if (module == nullptr)
    {
        return 0;
    }

    std::size_t count = 0;
    for (const mir::MirFunction* function : module->functions())
    {
        if (function == nullptr || !function->defined())
        {
            continue;
        }
        for (const mir::MirBlock* block : function->blocks())
        {
            if (block != nullptr)
            {
                count += block->instructions().size();
            }
        }
    }
    return count;
}

std::size_t MirRewriter::blockCount(const mir::MirModule* module) noexcept
{
    if (module == nullptr)
    {
        return 0;
    }

    std::size_t count = 0;
    for (const mir::MirFunction* function : module->functions())
    {
        if (function != nullptr && function->defined())
        {
            count += function->blocks().size();
        }
    }
    return count;
}
}
