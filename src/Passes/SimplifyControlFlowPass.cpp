#include "nativesharp/Passes/SimplifyControlFlowPass.h"

#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/PassContext.h"
#include "nativesharp/Passes/AnalysisManager.h"
#include "nativesharp/Sema/ConstantValue.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nativesharp::passes
{
namespace
{
using ConstantDefinitions =
    std::unordered_map<const mir::MirValue*, const mir::MirInstruction*>;

ConstantDefinitions collectConstants(const mir::MirFunction* function)
{
    ConstantDefinitions result;
    for (const mir::MirBlock* block : function->blocks())
    {
        for (const mir::MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr &&
                instruction->opcode() == mir::MirOpcode::Constant &&
                instruction->result() != nullptr &&
                instruction->hasConstant())
            {
                result.emplace(instruction->result(), instruction);
            }
        }
    }
    return result;
}

void pushSuccessors(
    const mir::MirInstruction* terminator,
    std::vector<mir::MirBlock*>& stack)
{
    if (terminator == nullptr)
    {
        return;
    }
    if (terminator->firstTarget() != nullptr)
    {
        stack.push_back(terminator->firstTarget());
    }
    if (terminator->secondTarget() != nullptr &&
        terminator->secondTarget() != terminator->firstTarget())
    {
        stack.push_back(terminator->secondTarget());
    }
}

std::unordered_set<mir::MirBlock*> computeReachable(
    mir::MirFunction* function)
{
    std::unordered_set<mir::MirBlock*> reachable;
    if (function == nullptr || function->entryBlock() == nullptr)
    {
        return reachable;
    }

    std::vector<mir::MirBlock*> stack;
    stack.push_back(function->entryBlock());
    while (!stack.empty())
    {
        mir::MirBlock* block = stack.back();
        stack.pop_back();
        if (block == nullptr || !reachable.insert(block).second ||
            block->instructions().empty())
        {
            continue;
        }
        pushSuccessors(block->instructions().back(), stack);
    }
    return reachable;
}

using PredecessorMap =
    std::unordered_map<mir::MirBlock*, std::unordered_set<mir::MirBlock*>>;

PredecessorMap buildPredecessors(
    const std::vector<mir::MirBlock*>& blocks)
{
    PredecessorMap predecessors;
    std::unordered_set<mir::MirBlock*> blockSet(blocks.begin(), blocks.end());

    for (mir::MirBlock* block : blocks)
    {
        predecessors.emplace(
            block,
            std::unordered_set<mir::MirBlock*>());
    }

    for (mir::MirBlock* block : blocks)
    {
        if (block == nullptr || block->instructions().empty())
        {
            continue;
        }
        const mir::MirInstruction* terminator = block->instructions().back();
        mir::MirBlock* targets[2] = {
            terminator->firstTarget(),
            terminator->secondTarget()
        };
        for (mir::MirBlock* target : targets)
        {
            if (target != nullptr && blockSet.contains(target))
            {
                predecessors[target].insert(block);
            }
        }
    }
    return predecessors;
}

std::size_t prunePhiIncoming(
    mir::MirBlock* block,
    const std::unordered_set<mir::MirBlock*>& predecessors)
{
    std::size_t removed = 0;
    for (mir::MirInstruction* instruction : block->instructions())
    {
        if (instruction->opcode() != mir::MirOpcode::Phi)
        {
            break;
        }

        std::vector<mir::MirPhiIncoming> retained;
        std::unordered_set<mir::MirBlock*> seen;
        for (const mir::MirPhiIncoming& incoming : instruction->phiIncoming())
        {
            if (incoming.block != nullptr &&
                predecessors.contains(incoming.block) &&
                seen.insert(incoming.block).second)
            {
                retained.push_back(incoming);
            }
            else
            {
                ++removed;
            }
        }

        instruction->clearPhiIncoming();
        for (const mir::MirPhiIncoming& incoming : retained)
        {
            instruction->addPhiIncoming(incoming.block, incoming.value);
        }
    }
    return removed;
}
}

std::string_view SimplifyControlFlowPass::name() const noexcept
{
    return "simplify-control-flow";
}

PassRequirements SimplifyControlFlowPass::requirements() const noexcept
{
    return PassRequirements::cfg();
}

PreservedAnalyses SimplifyControlFlowPass::preservedAnalyses() const noexcept
{
    return PreservedAnalyses::none();
}

PassRunResult SimplifyControlFlowPass::run(
    PassContext& context,
    std::string& error)
{
    PassRunResult result;
    mir::MirModule* module = context.module();
    if (module == nullptr)
    {
        return result;
    }
    if (context.analyses().ensureCfg() == nullptr)
    {
        error = context.analyses().error();
        return result;
    }

    for (mir::MirFunction* function : module->functions())
    {
        if (function == nullptr || !function->defined())
        {
            continue;
        }

        const ConstantDefinitions constants = collectConstants(function);
        bool functionChanged = false;

        for (mir::MirBlock* block : function->blocks())
        {
            if (block == nullptr || block->instructions().empty())
            {
                continue;
            }

            mir::MirInstruction* terminator = block->instructions().back();
            if (terminator->opcode() != mir::MirOpcode::Branch)
            {
                continue;
            }

            mir::MirBlock* selected = nullptr;
            if (terminator->firstTarget() == terminator->secondTarget())
            {
                selected = terminator->firstTarget();
            }
            else if (terminator->operands().size() == 1)
            {
                const auto found = constants.find(terminator->operands()[0]);
                if (found != constants.end() &&
                    found->second->constant().kind() ==
                        sema::ConstantKind::Boolean)
                {
                    selected = found->second->constant().boolean()
                        ? terminator->firstTarget()
                        : terminator->secondTarget();
                }
            }

            if (selected != nullptr)
            {
                terminator->resetTo(mir::MirOpcode::Jump);
                terminator->setFirstTarget(selected);
                functionChanged = true;
                result.changed = true;
                ++result.transformations;
            }
        }

        if (!functionChanged)
        {
            continue;
        }

        const std::unordered_set<mir::MirBlock*> reachable =
            computeReachable(function);
        std::vector<mir::MirBlock*> retainedBlocks;
        retainedBlocks.reserve(function->blocks().size());
        for (mir::MirBlock* block : function->blocks())
        {
            if (reachable.contains(block))
            {
                retainedBlocks.push_back(block);
            }
            else
            {
                ++result.transformations;
            }
        }

        if (retainedBlocks.empty() ||
            retainedBlocks.front() != function->entryBlock())
        {
            error = "control-flow simplification lost the function entry block";
            return result;
        }

        function->replaceBlocks(retainedBlocks);
        const PredecessorMap predecessors =
            buildPredecessors(function->blocks());

        for (mir::MirBlock* block : function->blocks())
        {
            const auto found = predecessors.find(block);
            if (found == predecessors.end())
            {
                error = "control-flow simplification cannot derive predecessor set";
                return result;
            }
            result.transformations += prunePhiIncoming(block, found->second);
        }
    }

    return result;
}
}
