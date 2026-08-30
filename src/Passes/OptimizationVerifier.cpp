#include "nativesharp/Passes/OptimizationVerifier.h"

#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/CFG/CfgVerifier.h"
#include "nativesharp/Dominance/DominanceFunction.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/Dominance/DominatorTree.h"
#include "nativesharp/Dominance/DominanceVerifier.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/MIR/MirVerifier.h"
#include "nativesharp/Passes/AnalysisManager.h"
#include "nativesharp/Passes/OptimizationModule.h"

#include <unordered_set>
#include <utility>

namespace nativesharp::passes
{
bool OptimizationVerifier::verify(
    OptimizationModule* module,
    AnalysisManager& analyses)
{
    error_.clear();
    definitions_.clear();

    if (module == nullptr || module->module() == nullptr)
    {
        return fail("optimization verifier requires an optimized MIR module");
    }

    const cfg::CfgModule* cfgModule = analyses.ensureCfg();
    const dominance::DominanceModule* dominanceModule =
        analyses.ensureDominance();
    if (cfgModule == nullptr || dominanceModule == nullptr)
    {
        return fail("optimized MIR analysis rebuild failed: " + analyses.error());
    }

    return verifyWithAnalyses(module, cfgModule, dominanceModule);
}

bool OptimizationVerifier::verify(const OptimizationModule* module)
{
    error_.clear();
    definitions_.clear();

    if (module == nullptr || module->module() == nullptr ||
        module->cfgModule() == nullptr ||
        module->dominanceModule() == nullptr)
    {
        return fail(
            "optimization verifier requires cached final CFG/dominance analyses");
    }

    return verifyWithAnalyses(
        module,
        module->cfgModule(),
        module->dominanceModule());
}

bool OptimizationVerifier::verifyWithAnalyses(
    const OptimizationModule* module,
    const cfg::CfgModule* cfgModule,
    const dominance::DominanceModule* dominanceModule)
{
    mir::MirVerifier mirVerifier;
    if (!mirVerifier.verify(
            module->module(),
            mir::MirVerificationMode::Ssa))
    {
        return fail("optimized MIR structural verification failed: " +
            mirVerifier.error());
    }

    cfg::CfgVerifier cfgVerifier;
    if (!cfgVerifier.verify(cfgModule))
    {
        return fail("cached optimized CFG is stale or invalid: " +
            cfgVerifier.error());
    }

    dominance::DominanceVerifier dominanceVerifier;
    if (!dominanceVerifier.verify(dominanceModule))
    {
        return fail("cached optimized dominance is stale or invalid: " +
            dominanceVerifier.error());
    }

    for (const mir::MirFunction* function : module->module()->functions())
    {
        if (function == nullptr || !function->defined())
        {
            continue;
        }

        const cfg::CfgFunction* cfgFunction =
            cfgModule->findFunction(function);
        const dominance::DominanceFunction* dominanceFunction =
            cfgFunction == nullptr
                ? nullptr
                : dominanceModule->findFunction(cfgFunction);

        if (cfgFunction == nullptr || dominanceFunction == nullptr)
        {
            return fail(
                "optimized defined function lacks CFG/dominance analysis");
        }
        if (!verifyFunction(cfgFunction, dominanceFunction))
        {
            return false;
        }
    }

    return true;
}

const std::string& OptimizationVerifier::error() const noexcept
{
    return error_;
}

bool OptimizationVerifier::verifyFunction(
    const cfg::CfgFunction* cfgFunction,
    const dominance::DominanceFunction* dominanceFunction)
{
    definitions_.clear();

    for (const cfg::CfgNode* node : cfgFunction->nodes())
    {
        if (node == nullptr || node->block() == nullptr)
        {
            return fail("optimized CFG contains invalid node");
        }
        if (!node->reachable())
        {
            return fail(
                "optimized SSA pipeline must not retain unreachable blocks");
        }
    }

    if (!collectDefinitions(cfgFunction))
    {
        return false;
    }
    return verifyUses(cfgFunction, dominanceFunction);
}

bool OptimizationVerifier::collectDefinitions(
    const cfg::CfgFunction* cfgFunction)
{
    const mir::MirFunction* function = cfgFunction->mirFunction();
    for (const mir::MirBlock* block : function->blocks())
    {
        for (std::size_t index = 0;
             index < block->instructions().size();
             ++index)
        {
            const mir::MirInstruction* instruction =
                block->instructions()[index];
            if (instruction == nullptr || instruction->result() == nullptr)
            {
                continue;
            }

            if (!definitions_.emplace(
                    instruction->result(),
                    Definition{block, index}).second)
            {
                return fail(
                    "optimized SSA value has multiple definitions");
            }
        }
    }
    return true;
}

bool OptimizationVerifier::verifyUses(
    const cfg::CfgFunction* cfgFunction,
    const dominance::DominanceFunction* dominanceFunction)
{
    const mir::MirFunction* function = cfgFunction->mirFunction();
    for (const mir::MirBlock* block : function->blocks())
    {
        for (std::size_t index = 0;
             index < block->instructions().size();
             ++index)
        {
            const mir::MirInstruction* instruction =
                block->instructions()[index];

            if (instruction->opcode() == mir::MirOpcode::Phi)
            {
                if (!verifyPhi(
                        instruction,
                        block,
                        cfgFunction,
                        dominanceFunction))
                {
                    return false;
                }
                continue;
            }

            for (const mir::MirValue* operand : instruction->operands())
            {
                if (!verifyOrdinaryUse(
                        operand,
                        block,
                        index,
                        cfgFunction,
                        dominanceFunction))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool OptimizationVerifier::verifyOrdinaryUse(
    const mir::MirValue* value,
    const mir::MirBlock* useBlock,
    std::size_t useIndex,
    const cfg::CfgFunction* cfgFunction,
    const dominance::DominanceFunction* dominanceFunction)
{
    const auto found = definitions_.find(value);
    if (found == definitions_.end())
    {
        return fail("optimized SSA use has no definition");
    }

    const Definition& definition = found->second;
    if (definition.block == useBlock)
    {
        return definition.instructionIndex < useIndex
            ? true
            : fail(
                "optimized SSA same-block definition must precede use");
    }

    return definitionDominatesBlock(
               definition,
               useBlock,
               cfgFunction,
               dominanceFunction)
        ? true
        : fail("optimized SSA definition does not dominate ordinary use");
}

bool OptimizationVerifier::verifyPhi(
    const mir::MirInstruction* instruction,
    const mir::MirBlock* block,
    const cfg::CfgFunction* cfgFunction,
    const dominance::DominanceFunction* dominanceFunction)
{
    const cfg::CfgNode* node = cfgFunction->findNode(block);
    if (node == nullptr)
    {
        return fail("optimized PHI block is absent from CFG");
    }

    const std::vector<cfg::CfgNode*> predecessors = node->predecessors();
    if (instruction->phiIncoming().size() != predecessors.size())
    {
        return fail(
            "optimized PHI must have one incoming value per predecessor");
    }

    std::unordered_set<const mir::MirBlock*> seen;
    for (const mir::MirPhiIncoming& incoming : instruction->phiIncoming())
    {
        if (incoming.block == nullptr || incoming.value == nullptr ||
            !seen.insert(incoming.block).second)
        {
            return fail(
                "optimized PHI incoming predecessors must be unique");
        }

        const cfg::CfgNode* predecessor =
            cfgFunction->findNode(incoming.block);
        if (predecessor == nullptr)
        {
            return fail(
                "optimized PHI incoming block is absent from CFG");
        }

        bool expectedPredecessor = false;
        for (const cfg::CfgNode* candidate : predecessors)
        {
            if (candidate == predecessor)
            {
                expectedPredecessor = true;
                break;
            }
        }
        if (!expectedPredecessor)
        {
            return fail(
                "optimized PHI incoming block is not a CFG predecessor");
        }

        const auto definition = definitions_.find(incoming.value);
        if (definition == definitions_.end())
        {
            return fail("optimized PHI incoming value has no definition");
        }
        if (!definitionDominatesBlock(
                definition->second,
                incoming.block,
                cfgFunction,
                dominanceFunction))
        {
            return fail(
                "optimized PHI definition does not dominate predecessor edge");
        }
    }

    return true;
}

bool OptimizationVerifier::definitionDominatesBlock(
    const Definition& definition,
    const mir::MirBlock* block,
    const cfg::CfgFunction* cfgFunction,
    const dominance::DominanceFunction* dominanceFunction) const noexcept
{
    const cfg::CfgNode* definitionNode =
        cfgFunction->findNode(definition.block);
    const cfg::CfgNode* useNode = cfgFunction->findNode(block);
    if (definitionNode == nullptr || useNode == nullptr ||
        dominanceFunction == nullptr ||
        dominanceFunction->dominatorTree() == nullptr)
    {
        return false;
    }

    if (definition.block == block)
    {
        return true;
    }
    return dominanceFunction->dominatorTree()->dominates(
        definitionNode,
        useNode);
}

bool OptimizationVerifier::fail(std::string message)
{
    error_ = std::move(message);
    return false;
}
}
