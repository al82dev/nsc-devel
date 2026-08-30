#include "nativesharp/Passes/ConstantPropagationPass.h"

#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/ConstantFolder.h"
#include "nativesharp/Passes/PassContext.h"
#include "nativesharp/Sema/ConstantValue.h"

#include <unordered_map>

namespace nativesharp::passes
{
std::string_view ConstantPropagationPass::name() const noexcept
{
    return "constant-propagation";
}

PassRequirements ConstantPropagationPass::requirements() const noexcept
{
    return PassRequirements::none();
}

PreservedAnalyses ConstantPropagationPass::preservedAnalyses() const noexcept
{
    return PreservedAnalyses::cfgAndDominance();
}

PassRunResult ConstantPropagationPass::run(
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

    std::unordered_map<const mir::MirValue*, sema::ConstantValue> known;
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
                    instruction->opcode() == mir::MirOpcode::Constant &&
                    instruction->result() != nullptr &&
                    instruction->hasConstant())
                {
                    known.emplace(instruction->result(), instruction->constant());
                }
            }
        }
    }

    bool progress = true;
    while (progress)
    {
        progress = false;
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
                    if (instruction == nullptr || instruction->result() == nullptr ||
                        known.contains(instruction->result()))
                    {
                        continue;
                    }

                    const std::optional<sema::ConstantValue> folded =
                        ConstantFolder::fold(instruction, known);
                    if (folded.has_value())
                    {
                        known.emplace(instruction->result(), *folded);
                        progress = true;
                    }
                }
            }
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
            for (mir::MirInstruction* instruction : block->instructions())
            {
                if (instruction == nullptr || instruction->result() == nullptr)
                {
                    continue;
                }

                const mir::MirOpcode opcode = instruction->opcode();
                if (opcode == mir::MirOpcode::Constant ||
                    opcode == mir::MirOpcode::Parameter ||
                    opcode == mir::MirOpcode::Undef ||
                    opcode == mir::MirOpcode::Phi ||
                    opcode == mir::MirOpcode::Copy)
                {
                    continue;
                }

                const auto found = known.find(instruction->result());
                if (found == known.end())
                {
                    continue;
                }

                instruction->resetTo(mir::MirOpcode::Constant);
                instruction->setConstant(found->second);
                result.changed = true;
                ++result.transformations;
            }
        }
    }

    return result;
}
}
