#include "nativesharp/Passes/CopyPropagationPass.h"

#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/MirRewriter.h"
#include "nativesharp/Passes/PassContext.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::passes
{
namespace
{
mir::MirValue* resolveReplacement(
    mir::MirValue* value,
    const std::unordered_map<const mir::MirValue*, mir::MirValue*>& replacements)
{
    std::unordered_set<const mir::MirValue*> visited;
    mir::MirValue* current = value;

    while (current != nullptr)
    {
        const auto found = replacements.find(current);
        if (found == replacements.end() || found->second == current)
        {
            break;
        }
        if (!visited.insert(current).second)
        {
            break;
        }
        current = found->second;
    }
    return current;
}
}

std::string_view CopyPropagationPass::name() const noexcept
{
    return "copy-propagation";
}

PassRequirements CopyPropagationPass::requirements() const noexcept
{
    return PassRequirements::none();
}

PreservedAnalyses CopyPropagationPass::preservedAnalyses() const noexcept
{
    return PreservedAnalyses::cfgAndDominance();
}

PassRunResult CopyPropagationPass::run(
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

    std::unordered_map<const mir::MirValue*, mir::MirValue*> replacements;
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
                if (instruction != nullptr &&
                    instruction->opcode() == mir::MirOpcode::Copy &&
                    instruction->result() != nullptr &&
                    instruction->operands().size() == 1 &&
                    instruction->operands()[0] != instruction->result())
                {
                    replacements.emplace(
                        instruction->result(),
                        instruction->operands()[0]);
                }
            }
        }
    }

    if (replacements.empty())
    {
        return result;
    }

    for (const auto& pair : replacements)
    {
        mir::MirValue* replacement = resolveReplacement(pair.second, replacements);
        if (replacement != nullptr && replacement != pair.first)
        {
            MirRewriter::replaceAllUses(module, pair.first, replacement);
        }
    }

    for (mir::MirFunction* function : module->functions())
    {
        if (function == nullptr || !function->defined())
        {
            continue;
        }
        for (mir::MirBlock* block : function->blocks())
        {
            std::vector<mir::MirInstruction*> retained;
            retained.reserve(block->instructions().size());
            for (mir::MirInstruction* instruction : block->instructions())
            {
                if (instruction != nullptr &&
                    instruction->opcode() == mir::MirOpcode::Copy &&
                    instruction->result() != nullptr &&
                    replacements.contains(instruction->result()))
                {
                    result.changed = true;
                    ++result.transformations;
                    continue;
                }
                retained.push_back(instruction);
            }
            block->replaceInstructions(std::move(retained));
        }
    }

    return result;
}
}
