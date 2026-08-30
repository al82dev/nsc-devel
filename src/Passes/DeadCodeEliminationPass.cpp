#include "nativesharp/Passes/DeadCodeEliminationPass.h"

#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/MirEffectClassifier.h"
#include "nativesharp/Passes/PassContext.h"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace nativesharp::passes
{
namespace
{
std::unordered_map<const mir::MirValue*, std::size_t> collectUseCounts(
    const mir::MirFunction* function)
{
    std::unordered_map<const mir::MirValue*, std::size_t> counts;
    for (const mir::MirBlock* block : function->blocks())
    {
        for (const mir::MirInstruction* instruction : block->instructions())
        {
            for (const mir::MirValue* operand : instruction->operands())
            {
                ++counts[operand];
            }
            for (const mir::MirPhiIncoming& incoming : instruction->phiIncoming())
            {
                ++counts[incoming.value];
            }
        }
    }
    return counts;
}
}

std::string_view DeadCodeEliminationPass::name() const noexcept
{
    return "dead-code-elimination";
}

PassRequirements DeadCodeEliminationPass::requirements() const noexcept
{
    return PassRequirements::none();
}

PreservedAnalyses DeadCodeEliminationPass::preservedAnalyses() const noexcept
{
    return PreservedAnalyses::cfgAndDominance();
}

PassRunResult DeadCodeEliminationPass::run(
    PassContext& context,
    std::string& error)
{
    (void)error;
    PassRunResult result;
    mir::MirModule* module = context.module();
    if (module == nullptr)
    {
        return result;
    }

    bool removed = true;
    while (removed)
    {
        removed = false;
        for (mir::MirFunction* function : module->functions())
        {
            if (function == nullptr || !function->defined())
            {
                continue;
            }

            const auto uses = collectUseCounts(function);
            for (mir::MirBlock* block : function->blocks())
            {
                std::vector<mir::MirInstruction*> retained;
                retained.reserve(block->instructions().size());

                for (mir::MirInstruction* instruction : block->instructions())
                {
                    const auto found = instruction->result() == nullptr
                        ? uses.end()
                        : uses.find(instruction->result());
                    const std::size_t useCount =
                        found == uses.end() ? 0 : found->second;

                    if (instruction->result() != nullptr && useCount == 0 &&
                        MirEffectClassifier::removableWhenUnused(instruction))
                    {
                        removed = true;
                        result.changed = true;
                        ++result.transformations;
                        continue;
                    }
                    retained.push_back(instruction);
                }
                block->replaceInstructions(std::move(retained));
            }
        }
    }

    return result;
}
}
