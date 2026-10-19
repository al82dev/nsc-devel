#include "nativesharp/Passes/PhiSimplificationPass.h"

#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/ConstantFolder.h"
#include "nativesharp/Passes/PassContext.h"

#include <unordered_map>
#include <vector>

namespace nativesharp::passes
{
namespace
{
using DefinitionMap =
    std::unordered_map<const mir::MirValue*, const mir::MirInstruction*>;

DefinitionMap collectDefinitions(const mir::MirFunction* function)
{
    DefinitionMap definitions;
    if (function == nullptr)
    {
        return definitions;
    }

    for (const mir::MirBlock* block : function->blocks())
    {
        for (const mir::MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->result() != nullptr)
            {
                definitions.emplace(instruction->result(), instruction);
            }
        }
    }
    return definitions;
}

bool incomingConstantsEqual(
    const mir::MirInstruction* phi,
    const DefinitionMap& definitions,
    sema::ConstantValue& value)
{
    bool first = true;
    for (const mir::MirPhiIncoming& incoming : phi->phiIncoming())
    {
        const auto found = definitions.find(incoming.value);
        if (found == definitions.end() ||
            found->second->opcode() != mir::MirOpcode::Constant ||
            !found->second->hasConstant())
        {
            return false;
        }

        if (first)
        {
            value = found->second->constant();
            first = false;
        }
        else if (!ConstantFolder::equal(value, found->second->constant()))
        {
            return false;
        }
    }
    return !first;
}

void simplifyPhiBlock(
    mir::MirBlock* block,
    const DefinitionMap& definitions,
    PassContext& context,
    PassRunResult& result)
{
    std::vector<mir::MirInstruction*> phis;
    std::vector<mir::MirInstruction*> replacements;
    std::vector<mir::MirInstruction*> body;
    bool inPhiPrefix = true;
    for (mir::MirInstruction* instruction : block->instructions())
    {
        if (inPhiPrefix && instruction->opcode() == mir::MirOpcode::Phi)
        {
            mir::MirValue* common = nullptr;
            bool sameValue = !instruction->phiIncoming().empty();
            for (const mir::MirPhiIncoming& incoming : instruction->phiIncoming())
            {
                if (common == nullptr)
                {
                    common = incoming.value;
                }
                else if (common != incoming.value)
                {
                    sameValue = false;
                    break;
                }
            }
            if (sameValue && common != nullptr && common != instruction->result())
            {
                mir::MirInstruction* copy = context.arena().makeInstruction(
                    mir::MirOpcode::Copy,
                    instruction->range());
                copy->setResult(instruction->result());
                copy->addOperand(common);
                replacements.push_back(copy);
                result.changed = true;
                ++result.transformations;
                continue;
            }
            sema::ConstantValue constant;
            if (incomingConstantsEqual(instruction, definitions, constant))
            {
                mir::MirInstruction* folded = context.arena().makeInstruction(
                    mir::MirOpcode::Constant,
                    instruction->range());
                folded->setResult(instruction->result());
                folded->setConstant(constant);
                replacements.push_back(folded);
                result.changed = true;
                ++result.transformations;
                continue;
            }
            phis.push_back(instruction);
            continue;
        }
        inPhiPrefix = false;
        body.push_back(instruction);
    }
    if (replacements.empty()) return;
    std::vector<mir::MirInstruction*> instructions;
    instructions.reserve(phis.size() + replacements.size() + body.size());
    instructions.insert(instructions.end(), phis.begin(), phis.end());
    instructions.insert(instructions.end(), replacements.begin(), replacements.end());
    instructions.insert(instructions.end(), body.begin(), body.end());
    block->replaceInstructions(std::move(instructions));
}
}

std::string_view PhiSimplificationPass::name() const noexcept
{
    return "phi-simplification";
}

PassRequirements PhiSimplificationPass::requirements() const noexcept
{
    return PassRequirements::none();
}

PreservedAnalyses PhiSimplificationPass::preservedAnalyses() const noexcept
{
    return PreservedAnalyses::cfgAndDominance();
}

PassRunResult PhiSimplificationPass::run(
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

    for (mir::MirFunction* function : module->functions())
    {
        if (function == nullptr || !function->defined())
        {
            continue;
        }

        const DefinitionMap definitions = collectDefinitions(function);
        for (mir::MirBlock* block : function->blocks())
        {
            simplifyPhiBlock(block, definitions, context, result);
        }
    }
    return result;
}
}
