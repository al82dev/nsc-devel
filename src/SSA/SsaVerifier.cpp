#include "nativesharp/SSA/SsaVerifier.h"

#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/Dominance/DominanceFrontier.h"
#include "nativesharp/Dominance/DominanceFunction.h"
#include "nativesharp/Dominance/DominatorTree.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/MIR/MirVerifier.h"
#include "nativesharp/SSA/SsaFunction.h"
#include "nativesharp/SSA/SsaModule.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"

#include <cstddef>
#include <utility>

namespace nativesharp::ssa
{
bool SsaVerifier::verify(const SsaModule* module)
{
    error_.clear();
    definitions_.clear();
    duplicateDefinitions_.clear();

    if (module == nullptr || module->sourceModule() == nullptr || module->module() == nullptr)
    {
        return fail("SSA module/source/output pointers must be non-null");
    }

    mir::MirVerifier mirVerifier;
    if (!mirVerifier.verify(module->module(), mir::MirVerificationMode::Ssa))
    {
        return fail("SSA MIR shape/type verification failed: " + mirVerifier.error());
    }

    if (module->module()->functions().size() != module->sourceModule()->functions().size() ||
        module->module()->globals().size() != module->sourceModule()->globals().size())
    {
        return fail("SSA module must preserve source function/global cardinality");
    }
    for (std::size_t index = 0; index < module->sourceModule()->globals().size(); ++index)
    {
        if (module->sourceModule()->globals()[index]->symbol() != module->module()->globals()[index]->symbol())
        {
            return fail("SSA module must preserve global-symbol order and identity");
        }
    }
    for (std::size_t index = 0; index < module->sourceModule()->functions().size(); ++index)
    {
        if (module->sourceModule()->functions()[index]->symbol() !=
            module->module()->functions()[index]->symbol())
        {
            return fail("SSA module must preserve function-symbol order and identity");
        }
    }

    std::size_t definedCount = 0;
    for (const mir::MirFunction* sourceFunction : module->sourceModule()->functions())
    {
        if (sourceFunction != nullptr && sourceFunction->defined())
        {
            ++definedCount;
            const SsaFunction* function = module->findFunction(sourceFunction);
            if (function == nullptr || !verifyFunction(function))
            {
                return false;
            }
        }
    }
    if (module->functions().size() != definedCount)
    {
        return fail("SSA function metadata must exist exactly for defined source MIR functions");
    }
    return true;
}

const std::string& SsaVerifier::error() const noexcept { return error_; }

bool SsaVerifier::verifyFunction(const SsaFunction* function)
{
    definitions_.clear();
    duplicateDefinitions_.clear();

    if (function == nullptr || function->sourceFunction() == nullptr || function->function() == nullptr ||
        function->cfgFunction() == nullptr || function->dominanceFunction() == nullptr ||
        !function->sourceFunction()->defined() || !function->function()->defined() ||
        function->sourceFunction()->symbol() != function->function()->symbol())
    {
        return fail("SSA function metadata does not correspond to one defined source MIR function");
    }
    if (function->cfgFunction()->mirFunction() != function->sourceFunction() ||
        function->dominanceFunction()->cfgFunction() != function->cfgFunction())
    {
        return fail("SSA function CFG/dominance provenance is inconsistent");
    }
    if (function->function()->locals().size() != function->sourceFunction()->locals().size())
    {
        return fail("SSA function must preserve source local-symbol cardinality");
    }
    for (std::size_t index = 0; index < function->sourceFunction()->locals().size(); ++index)
    {
        if (function->function()->locals()[index] != function->sourceFunction()->locals()[index])
        {
            return fail("SSA function must preserve source local-symbol order and identity");
        }
    }
    if (!verifyPromotionSet(function) || !verifyBlockMapping(function) ||
        !collectDefinitions(function) || !verifyInstructions(function) ||
        !verifyExpectedSymbolPhis(function) || !verifyVirtualMergeCounts(function))
    {
        return false;
    }
    return true;
}

bool SsaVerifier::verifyPromotionSet(const SsaFunction* function)
{
    std::vector<sema::Symbol*> expected;
    for (sema::Symbol* symbol : function->sourceFunction()->locals())
    {
        if (symbol == nullptr ||
            (symbol->kind() != sema::SymbolKind::Variable &&
             symbol->kind() != sema::SymbolKind::Parameter))
        {
            continue;
        }

        bool addressTaken = false;
        for (const mir::MirBlock* block : function->sourceFunction()->blocks())
        {
            if (block == nullptr)
            {
                continue;
            }
            for (const mir::MirInstruction* instruction : block->instructions())
            {
                if (instruction != nullptr && instruction->opcode() == mir::MirOpcode::AddressOfSymbol &&
                    instruction->symbol() == symbol)
                {
                    addressTaken = true;
                    break;
                }
            }
            if (addressTaken)
            {
                break;
            }
        }
        if (!addressTaken)
        {
            expected.push_back(symbol);
        }
    }

    if (expected.size() != function->promotedSymbols().size())
    {
        return fail("SSA promoted-symbol set differs from independent promotability analysis");
    }
    std::unordered_set<const sema::Symbol*> unique;
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        if (expected[index] != function->promotedSymbols()[index] ||
            !unique.insert(function->promotedSymbols()[index]).second)
        {
            return fail("SSA promoted-symbol order/content/uniqueness differs from source local order");
        }
    }
    return true;
}

bool SsaVerifier::verifyBlockMapping(const SsaFunction* function)
{
    std::size_t reachableCount = 0;
    std::size_t unreachableCount = 0;
    for (const cfg::CfgNode* node : function->cfgFunction()->nodes())
    {
        if (node == nullptr || node->block() == nullptr)
        {
            return fail("SSA verifier encountered invalid CFG node");
        }
        if (node->reachable())
        {
            ++reachableCount;
            const mir::MirBlock* output = function->findOutputBlock(node->block());
            if (output == nullptr || function->findSourceBlock(output) != node->block())
            {
                return fail("every reachable source block must map bidirectionally to one SSA block");
            }
        }
        else
        {
            ++unreachableCount;
            if (function->findOutputBlock(node->block()) != nullptr)
            {
                return fail("unreachable source block must be excluded from SSA representation");
            }
        }
    }
    if (function->function()->blocks().size() != reachableCount ||
        function->excludedUnreachableBlocks() != unreachableCount)
    {
        return fail("SSA reachable/excluded block counts do not match CFG reachability");
    }
    std::size_t outputIndex = 0;
    for (const mir::MirBlock* sourceBlock : function->sourceFunction()->blocks())
    {
        const cfg::CfgNode* node = function->cfgFunction()->findNode(sourceBlock);
        if (node == nullptr || !node->reachable())
        {
            continue;
        }
        if (outputIndex >= function->function()->blocks().size() ||
            function->findOutputBlock(sourceBlock) != function->function()->blocks()[outputIndex])
        {
            return fail("SSA blocks must preserve reachable source MIR order");
        }
        ++outputIndex;
    }
    if (function->function()->entryBlock() == nullptr ||
        function->findSourceBlock(function->function()->entryBlock()) !=
            function->sourceFunction()->entryBlock())
    {
        return fail("SSA entry block must map to source MIR entry block");
    }
    return true;
}

bool SsaVerifier::collectDefinitions(const SsaFunction* function)
{
    for (const mir::MirBlock* block : function->function()->blocks())
    {
        if (block == nullptr)
        {
            return fail("SSA output function contains null block");
        }
        for (std::size_t index = 0; index < block->instructions().size(); ++index)
        {
            const mir::MirInstruction* instruction = block->instructions()[index];
            if (instruction == nullptr || instruction->result() == nullptr)
            {
                continue;
            }
            if (!definitions_.emplace(instruction->result(), Definition{block, index}).second)
            {
                duplicateDefinitions_.insert(instruction->result());
            }
        }
    }
    if (!duplicateDefinitions_.empty())
    {
        return fail("SSA virtual value has more than one definition");
    }
    return true;
}

bool SsaVerifier::verifyInstructions(const SsaFunction* function)
{
    std::unordered_map<const sema::Symbol*, std::size_t> symbolUndefCounts;
    std::size_t virtualUndefCount = 0;
    for (const mir::MirBlock* block : function->function()->blocks())
    {
        for (std::size_t index = 0; index < block->instructions().size(); ++index)
        {
            const mir::MirInstruction* instruction = block->instructions()[index];
            if (!verifyInstruction(function, block, instruction, index))
            {
                return false;
            }
            if (instruction->opcode() == mir::MirOpcode::Undef)
            {
                if (instruction->symbol() == nullptr)
                {
                    ++virtualUndefCount;
                }
                else
                {
                    ++symbolUndefCounts[instruction->symbol()];
                }
            }
        }
    }

    for (const sema::Symbol* symbol : function->promotedSymbols())
    {
        const std::size_t count = symbolUndefCounts[symbol];
        if (symbol->kind() == sema::SymbolKind::Variable && count != 1)
        {
            return fail("each promoted local variable must have exactly one entry undef seed");
        }
        if (symbol->kind() == sema::SymbolKind::Parameter && count != 0)
        {
            return fail("promoted parameters must not receive artificial undef seeds");
        }
    }
    for (const auto& pair : symbolUndefCounts)
    {
        if (!isPromoted(function, pair.first))
        {
            return fail("SSA symbol undef may exist only for promoted local variable");
        }
    }
    if (virtualUndefCount != sourceMultiDefinedValueCount(function))
    {
        return fail("SSA virtual undef count must equal source multi-defined virtual count");
    }
    return true;
}

bool SsaVerifier::verifyInstruction(
    const SsaFunction* function,
    const mir::MirBlock* block,
    const mir::MirInstruction* instruction,
    std::size_t instructionIndex)
{
    if (instruction == nullptr)
    {
        return fail("SSA block contains null instruction");
    }
    if ((instruction->opcode() == mir::MirOpcode::LoadSymbol ||
         instruction->opcode() == mir::MirOpcode::StoreSymbol ||
         instruction->opcode() == mir::MirOpcode::AddressOfSymbol) &&
        isPromoted(function, instruction->symbol()))
    {
        return fail("promoted SSA symbol must not retain load/store/address.symbol operations");
    }
    if (instruction->opcode() == mir::MirOpcode::Undef)
    {
        if (block != function->function()->entryBlock())
        {
            return fail("SSA undef seeds must be materialized only in entry block");
        }
        if (instruction->symbol() != nullptr && !isPromoted(function, instruction->symbol()))
        {
            return fail("symbol-associated SSA undef must belong to promoted symbol");
        }
    }
    if (instruction->opcode() == mir::MirOpcode::Phi && !verifyPhi(function, block, instruction))
    {
        return false;
    }

    if (instruction->opcode() != mir::MirOpcode::Phi)
    {
        for (const mir::MirValue* operand : instruction->operands())
        {
            if (!verifyUseDominance(function, operand, block, instructionIndex, false))
            {
                return false;
            }
        }
    }
    return true;
}

bool SsaVerifier::verifyPhi(
    const SsaFunction* function,
    const mir::MirBlock* block,
    const mir::MirInstruction* instruction)
{
    const cfg::CfgNode* cfgNode = sourceCfgNode(function, block);
    if (cfgNode == nullptr)
    {
        return fail("SSA PHI block has no source CFG node");
    }

    const std::vector<cfg::CfgNode*> predecessors = cfgNode->predecessors();
    std::unordered_set<const mir::MirBlock*> expectedBlocks;
    for (const cfg::CfgNode* predecessor : predecessors)
    {
        if (predecessor != nullptr && predecessor->reachable() && predecessor->block() != nullptr)
        {
            const mir::MirBlock* output = function->findOutputBlock(predecessor->block());
            if (output == nullptr)
            {
                return fail("reachable PHI predecessor has no SSA block mapping");
            }
            expectedBlocks.insert(output);
        }
    }
    if (instruction->phiIncoming().size() != expectedBlocks.size())
    {
        return fail("SSA PHI must contain exactly one incoming value per unique reachable predecessor");
    }

    std::unordered_set<const mir::MirBlock*> seen;
    for (const mir::MirPhiIncoming& incoming : instruction->phiIncoming())
    {
        if (incoming.block == nullptr || incoming.value == nullptr ||
            !expectedBlocks.contains(incoming.block) || !seen.insert(incoming.block).second)
        {
            return fail("SSA PHI incoming predecessor set is invalid or duplicated");
        }
        const std::size_t edgeUseIndex = incoming.block->instructions().size();
        if (!verifyUseDominance(function, incoming.value, incoming.block, edgeUseIndex, true))
        {
            return false;
        }
    }

    if (instruction->symbol() != nullptr && !isPromoted(function, instruction->symbol()))
    {
        return fail("symbol-associated SSA PHI must correspond to promoted storage");
    }
    return true;
}

bool SsaVerifier::verifyUseDominance(
    const SsaFunction* function,
    const mir::MirValue* value,
    const mir::MirBlock* useBlock,
    std::size_t useInstructionIndex,
    bool edgeUse)
{
    const auto found = definitions_.find(value);
    if (found == definitions_.end())
    {
        return fail("SSA use references virtual value without definition");
    }
    const Definition& definition = found->second;
    if (definition.block == useBlock)
    {
        if (definition.instructionIndex >= useInstructionIndex)
        {
            return fail(edgeUse
                ? "SSA PHI edge-use must be defined before predecessor terminator"
                : "SSA value must be defined earlier than its same-block use");
        }
        return true;
    }

    const cfg::CfgNode* definitionNode = sourceCfgNode(function, definition.block);
    const cfg::CfgNode* useNode = sourceCfgNode(function, useBlock);
    const dominance::DominatorTree* tree = function->dominanceFunction()->dominatorTree();
    if (definitionNode == nullptr || useNode == nullptr || tree == nullptr ||
        !tree->dominates(definitionNode, useNode))
    {
        return fail(edgeUse
            ? "SSA PHI incoming definition must dominate its predecessor edge source"
            : "SSA definition must dominate every ordinary use");
    }
    return true;
}

bool SsaVerifier::verifyExpectedSymbolPhis(const SsaFunction* function)
{
    for (const sema::Symbol* symbol : function->promotedSymbols())
    {
        const std::vector<const cfg::CfgNode*> definitions = symbolDefinitionNodes(function, symbol);
        const std::vector<const cfg::CfgNode*> expected = expectedPhiNodes(function, definitions);
        std::vector<const cfg::CfgNode*> actual;
        for (const mir::MirBlock* block : function->function()->blocks())
        {
            for (const mir::MirInstruction* instruction : block->instructions())
            {
                if (instruction != nullptr && instruction->opcode() == mir::MirOpcode::Phi &&
                    instruction->symbol() == symbol)
                {
                    const cfg::CfgNode* node = sourceCfgNode(function, block);
                    if (node == nullptr)
                    {
                        return fail("symbol PHI has no source CFG node");
                    }
                    actual.push_back(node);
                }
            }
        }
        if (actual.size() != expected.size())
        {
            return fail("SSA symbol PHI placement cardinality differs from iterated dominance frontier");
        }
        for (const cfg::CfgNode* node : expected)
        {
            if (!containsNode(actual, node))
            {
                return fail("SSA symbol PHI placement misses iterated-dominance-frontier block");
            }
        }
    }
    return true;
}

bool SsaVerifier::verifyVirtualMergeCounts(const SsaFunction* function)
{
    std::size_t actual = 0;
    for (const mir::MirBlock* block : function->function()->blocks())
    {
        for (const mir::MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->opcode() == mir::MirOpcode::Phi &&
                instruction->symbol() == nullptr)
            {
                ++actual;
            }
        }
    }
    return actual == expectedVirtualPhiCount(function)
        ? true
        : fail("SSA virtual-value PHI count differs from source multi-definition IDF plan");
}

std::vector<const cfg::CfgNode*> SsaVerifier::expectedPhiNodes(
    const SsaFunction* function,
    const std::vector<const cfg::CfgNode*>& definitionNodes) const
{
    std::vector<const cfg::CfgNode*> result;
    if (function == nullptr || function->dominanceFunction() == nullptr ||
        function->dominanceFunction()->dominanceFrontier() == nullptr)
    {
        return result;
    }

    std::vector<const cfg::CfgNode*> worklist = definitionNodes;
    std::unordered_set<const cfg::CfgNode*> definitions;
    std::unordered_set<const cfg::CfgNode*> seen;
    for (const cfg::CfgNode* node : definitionNodes)
    {
        if (node != nullptr)
        {
            definitions.insert(node);
        }
    }

    std::size_t index = 0;
    while (index < worklist.size())
    {
        const cfg::CfgNode* node = worklist[index++];
        if (node == nullptr)
        {
            continue;
        }
        for (const cfg::CfgNode* frontierNode :
             function->dominanceFunction()->dominanceFrontier()->frontier(node))
        {
            if (frontierNode == nullptr || !frontierNode->reachable() || !seen.insert(frontierNode).second)
            {
                continue;
            }
            result.push_back(frontierNode);
            if (!definitions.contains(frontierNode))
            {
                worklist.push_back(frontierNode);
            }
        }
    }
    return result;
}

std::vector<const cfg::CfgNode*> SsaVerifier::symbolDefinitionNodes(
    const SsaFunction* function,
    const sema::Symbol* symbol) const
{
    std::vector<const cfg::CfgNode*> result;
    if (function == nullptr || symbol == nullptr)
    {
        return result;
    }
    if (symbol->kind() == sema::SymbolKind::Variable && function->cfgFunction()->entryNode() != nullptr)
    {
        result.push_back(function->cfgFunction()->entryNode());
    }
    for (const cfg::CfgNode* node : function->cfgFunction()->nodes())
    {
        if (node == nullptr || !node->reachable() || node->block() == nullptr)
        {
            continue;
        }
        for (const mir::MirInstruction* instruction : node->block()->instructions())
        {
            if (instruction != nullptr && instruction->opcode() == mir::MirOpcode::StoreSymbol &&
                instruction->symbol() == symbol)
            {
                if (!containsNode(result, node))
                {
                    result.push_back(node);
                }
            }
        }
    }
    return result;
}

std::size_t SsaVerifier::sourceMultiDefinedValueCount(const SsaFunction* function) const
{
    std::unordered_map<const mir::MirValue*, std::size_t> counts;
    for (const cfg::CfgNode* node : function->cfgFunction()->nodes())
    {
        if (node == nullptr || !node->reachable() || node->block() == nullptr)
        {
            continue;
        }
        for (const mir::MirInstruction* instruction : node->block()->instructions())
        {
            if (instruction != nullptr && instruction->result() != nullptr)
            {
                ++counts[instruction->result()];
            }
        }
    }
    std::size_t result = 0;
    for (const auto& pair : counts)
    {
        if (pair.second > 1)
        {
            ++result;
        }
    }
    return result;
}

std::size_t SsaVerifier::expectedVirtualPhiCount(const SsaFunction* function) const
{
    std::unordered_map<const mir::MirValue*, std::vector<const cfg::CfgNode*>> definitions;
    for (const cfg::CfgNode* node : function->cfgFunction()->nodes())
    {
        if (node == nullptr || !node->reachable() || node->block() == nullptr)
        {
            continue;
        }
        for (const mir::MirInstruction* instruction : node->block()->instructions())
        {
            if (instruction == nullptr || instruction->result() == nullptr)
            {
                continue;
            }
            std::vector<const cfg::CfgNode*>& nodes = definitions[instruction->result()];
            if (!containsNode(nodes, node))
            {
                nodes.push_back(node);
            }
        }
    }

    std::size_t result = 0;
    for (auto& pair : definitions)
    {
        std::size_t definitionCount = 0;
        for (const cfg::CfgNode* node : function->cfgFunction()->nodes())
        {
            if (node == nullptr || !node->reachable() || node->block() == nullptr)
            {
                continue;
            }
            for (const mir::MirInstruction* instruction : node->block()->instructions())
            {
                if (instruction != nullptr && instruction->result() == pair.first)
                {
                    ++definitionCount;
                }
            }
        }
        if (definitionCount <= 1)
        {
            continue;
        }
        std::vector<const cfg::CfgNode*> nodes = pair.second;
        if (function->cfgFunction()->entryNode() != nullptr &&
            !containsNode(nodes, function->cfgFunction()->entryNode()))
        {
            nodes.push_back(function->cfgFunction()->entryNode());
        }
        result += expectedPhiNodes(function, nodes).size();
    }
    return result;
}

const cfg::CfgNode* SsaVerifier::sourceCfgNode(
    const SsaFunction* function,
    const mir::MirBlock* outputBlock) const noexcept
{
    if (function == nullptr || outputBlock == nullptr)
    {
        return nullptr;
    }
    const mir::MirBlock* sourceBlock = function->findSourceBlock(outputBlock);
    return sourceBlock == nullptr ? nullptr : function->cfgFunction()->findNode(sourceBlock);
}

bool SsaVerifier::isPromoted(
    const SsaFunction* function,
    const sema::Symbol* symbol) const noexcept
{
    return function != nullptr && symbol != nullptr && function->isPromoted(symbol);
}

bool SsaVerifier::containsNode(
    const std::vector<const cfg::CfgNode*>& nodes,
    const cfg::CfgNode* node) const noexcept
{
    for (const cfg::CfgNode* candidate : nodes)
    {
        if (candidate == node)
        {
            return true;
        }
    }
    return false;
}

bool SsaVerifier::fail(std::string message)
{
    if (error_.empty())
    {
        error_ = std::move(message);
    }
    return false;
}
}
