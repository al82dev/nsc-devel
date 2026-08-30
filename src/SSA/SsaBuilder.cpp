#include "nativesharp/SSA/SsaBuilder.h"

#include "nativesharp/CFG/CfgFunction.h"
#include "nativesharp/CFG/CfgModule.h"
#include "nativesharp/CFG/CfgNode.h"
#include "nativesharp/Dominance/DominanceFrontier.h"
#include "nativesharp/Dominance/DominanceFunction.h"
#include "nativesharp/Dominance/DominanceModule.h"
#include "nativesharp/Dominance/DominatorTree.h"
#include "nativesharp/Dominance/DominatorTreeNode.h"
#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/SSA/SsaFunction.h"
#include "nativesharp/SSA/SsaModule.h"
#include "nativesharp/SSA/SsaPromotability.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"

#include <cstddef>
#include <utility>

namespace nativesharp::ssa
{
SsaBuilder::SsaBuilder(SsaArena& arena) : arena_(arena), outputArena_(&arena.mirArena()) {}

SsaModule* SsaBuilder::build(
    const mir::MirModule* sourceModule,
    const cfg::CfgModule* cfgModule,
    const dominance::DominanceModule* dominanceModule)
{
    error_.clear();
    resetFunctionState();
    sourceModule_ = sourceModule;
    cfgModule_ = cfgModule;
    dominanceModule_ = dominanceModule;

    if (sourceModule_ == nullptr || cfgModule_ == nullptr || dominanceModule_ == nullptr)
    {
        fail("SSA builder requires MIR, CFG and dominance modules");
        return nullptr;
    }
    if (cfgModule_->mirModule() != sourceModule_ || dominanceModule_->cfgModule() != cfgModule_)
    {
        fail("SSA builder analysis modules do not correspond to the source MIR module");
        return nullptr;
    }

    mir::MirModule* outputModule = outputArena_->makeModule();
    SsaModule* ssaModule = arena_.makeModule(sourceModule_, outputModule);

    for (const mir::MirGlobal* global : sourceModule_->globals())
    {
        if (!cloneGlobal(global, outputModule))
        {
            return nullptr;
        }
    }

    for (const mir::MirFunction* sourceFunction : sourceModule_->functions())
    {
        if (sourceFunction == nullptr)
        {
            fail("SSA builder encountered null MIR function");
            return nullptr;
        }
        if (!sourceFunction->defined())
        {
            if (!clonePrototype(sourceFunction, outputModule))
            {
                return nullptr;
            }
            continue;
        }

        const cfg::CfgFunction* cfgFunction = cfgModule_->findFunction(sourceFunction);
        const dominance::DominanceFunction* dominanceFunction =
            cfgFunction == nullptr ? nullptr : dominanceModule_->findFunction(cfgFunction);
        if (cfgFunction == nullptr || dominanceFunction == nullptr)
        {
            fail("SSA builder cannot find CFG/dominance information for defined MIR function");
            return nullptr;
        }
        if (!buildFunction(
                sourceFunction,
                cfgFunction,
                dominanceFunction,
                outputModule,
                ssaModule))
        {
            return nullptr;
        }
    }

    return ssaModule;
}

bool SsaBuilder::succeeded() const noexcept { return error_.empty(); }
const std::string& SsaBuilder::error() const noexcept { return error_; }

bool SsaBuilder::cloneGlobal(const mir::MirGlobal* sourceGlobal, mir::MirModule* outputModule)
{
    if (sourceGlobal == nullptr || sourceGlobal->symbol() == nullptr || outputModule == nullptr)
    {
        return fail("SSA builder cannot clone invalid MIR global");
    }
    mir::MirGlobal* outputGlobal = outputArena_->makeGlobal(sourceGlobal->symbol());
    if (sourceGlobal->hasExplicitInitializer())
    {
        outputGlobal->setExplicitInitializer(sourceGlobal->initializer());
    }
    outputModule->addGlobal(outputGlobal);
    return true;
}

bool SsaBuilder::clonePrototype(const mir::MirFunction* sourceFunction, mir::MirModule* outputModule)
{
    if (sourceFunction == nullptr || sourceFunction->symbol() == nullptr || outputModule == nullptr)
    {
        return fail("SSA builder cannot clone invalid MIR prototype");
    }
    mir::MirFunction* outputFunction = outputArena_->makeFunction(sourceFunction->symbol());
    outputFunction->setDefined(false);
    outputModule->addFunction(outputFunction);
    return true;
}

bool SsaBuilder::buildFunction(
    const mir::MirFunction* sourceFunction,
    const cfg::CfgFunction* cfgFunction,
    const dominance::DominanceFunction* dominanceFunction,
    mir::MirModule* outputModule,
    SsaModule* ssaModule)
{
    resetFunctionState();
    sourceFunction_ = sourceFunction;
    cfgFunction_ = cfgFunction;
    dominanceFunction_ = dominanceFunction;

    if (sourceFunction_ == nullptr || cfgFunction_ == nullptr || dominanceFunction_ == nullptr ||
        outputModule == nullptr || ssaModule == nullptr || sourceFunction_->symbol() == nullptr)
    {
        return fail("SSA builder received invalid function inputs");
    }

    outputFunction_ = outputArena_->makeFunction(sourceFunction_->symbol());
    outputFunction_->setDefined(true);
    outputModule->addFunction(outputFunction_);
    ssaFunction_ = arena_.makeFunction(
        sourceFunction_,
        outputFunction_,
        cfgFunction_,
        dominanceFunction_);
    ssaModule->addFunction(ssaFunction_);

    for (sema::Symbol* local : sourceFunction_->locals())
    {
        outputFunction_->addLocal(local);
    }

    if (!classifyVariables() || !createOutputBlocks() || !planPhiPlacement() ||
        !materializePlannedPhis() || !renameFunction())
    {
        return false;
    }

    for (mir::MirValue* parameter : outputParameters_)
    {
        if (parameter == nullptr)
        {
            return fail("SSA builder did not materialize every function parameter");
        }
        outputFunction_->addParameter(parameter);
    }
    return true;
}

void SsaBuilder::resetFunctionState()
{
    sourceFunction_ = nullptr;
    outputFunction_ = nullptr;
    cfgFunction_ = nullptr;
    dominanceFunction_ = nullptr;
    ssaFunction_ = nullptr;
    promotedSymbols_.clear();
    promotedSymbolOrder_.clear();
    multiDefinedValues_.clear();
    multiDefinedValueOrder_.clear();
    phiSpecs_.clear();
    phiInstructions_.clear();
    symbolStacks_.clear();
    valueStacks_.clear();
    directValues_.clear();
    outputParameters_.clear();
}

bool SsaBuilder::classifyVariables()
{
    SsaPromotability promotability;
    promotedSymbolOrder_ = promotability.findPromotableSymbols(sourceFunction_);
    for (sema::Symbol* symbol : promotedSymbolOrder_)
    {
        if (symbol == nullptr)
        {
            return fail("SSA promotability returned null symbol");
        }
        promotedSymbols_.insert(symbol);
        ssaFunction_->addPromotedSymbol(symbol);
    }

    collectMultiDefinedValues();
    outputParameters_.assign(sourceFunction_->parameters().size(), nullptr);
    return true;
}

void SsaBuilder::collectMultiDefinedValues()
{
    std::unordered_map<const mir::MirValue*, std::size_t> definitionCounts;
    for (const cfg::CfgNode* node : cfgFunction_->nodes())
    {
        if (node == nullptr || !node->reachable() || node->block() == nullptr)
        {
            continue;
        }
        for (const mir::MirInstruction* instruction : node->block()->instructions())
        {
            if (instruction != nullptr && instruction->result() != nullptr)
            {
                ++definitionCounts[instruction->result()];
            }
        }
    }

    for (const auto& pair : definitionCounts)
    {
        if (pair.first != nullptr && pair.second > 1)
        {
            multiDefinedValues_.insert(pair.first);
            multiDefinedValueOrder_.push_back(pair.first);
        }
    }

    for (std::size_t i = 1; i < multiDefinedValueOrder_.size(); ++i)
    {
        const mir::MirValue* value = multiDefinedValueOrder_[i];
        std::size_t position = i;
        while (position > 0 && multiDefinedValueOrder_[position - 1]->id() > value->id())
        {
            multiDefinedValueOrder_[position] = multiDefinedValueOrder_[position - 1];
            --position;
        }
        multiDefinedValueOrder_[position] = value;
    }
}

bool SsaBuilder::createOutputBlocks()
{
    std::size_t reachableCount = 0;
    for (const mir::MirBlock* sourceBlock : sourceFunction_->blocks())
    {
        if (!isReachableBlock(sourceBlock))
        {
            continue;
        }
        mir::MirBlock* outputBlock = outputArena_->makeBlock(sourceBlock->name());
        outputFunction_->addBlock(outputBlock);
        ssaFunction_->mapBlock(sourceBlock, outputBlock);
        ++reachableCount;
    }
    ssaFunction_->setExcludedUnreachableBlocks(sourceFunction_->blocks().size() - reachableCount);

    if (outputFunction_->blocks().empty() ||
        ssaFunction_->findOutputBlock(sourceFunction_->entryBlock()) != outputFunction_->entryBlock())
    {
        return fail("SSA output function must preserve the reachable MIR entry block first");
    }
    return true;
}

bool SsaBuilder::planPhiPlacement()
{
    for (sema::Symbol* symbol : promotedSymbolOrder_)
    {
        if (!planSymbolPhis(symbol))
        {
            return false;
        }
    }
    for (const mir::MirValue* value : multiDefinedValueOrder_)
    {
        if (!planValuePhis(value))
        {
            return false;
        }
    }
    return true;
}

bool SsaBuilder::planSymbolPhis(sema::Symbol* symbol)
{
    if (symbol == nullptr)
    {
        return fail("cannot plan SSA PHI for null symbol");
    }
    std::vector<const cfg::CfgNode*> definitions;
    const cfg::CfgNode* entry = cfgFunction_->entryNode();

    if (symbol->kind() == sema::SymbolKind::Variable && entry != nullptr)
    {
        definitions.push_back(entry);
    }

    for (const cfg::CfgNode* node : cfgFunction_->nodes())
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
                bool alreadyAdded = false;
                for (const cfg::CfgNode* definition : definitions)
                {
                    if (definition == node)
                    {
                        alreadyAdded = true;
                        break;
                    }
                }
                if (!alreadyAdded)
                {
                    definitions.push_back(node);
                }
            }
        }
    }

    if (definitions.empty())
    {
        return fail("promoted parameter has no reachable SSA definition");
    }
    return placePhiSpecs(definitions, {symbol, nullptr, symbol->type()});
}

bool SsaBuilder::planValuePhis(const mir::MirValue* value)
{
    if (value == nullptr || value->type() == nullptr)
    {
        return fail("cannot plan SSA PHI for invalid multi-defined MIR value");
    }
    std::vector<const cfg::CfgNode*> definitions;
    if (cfgFunction_->entryNode() != nullptr)
    {
        definitions.push_back(cfgFunction_->entryNode());
    }

    for (const cfg::CfgNode* node : cfgFunction_->nodes())
    {
        if (node == nullptr || !node->reachable() || node->block() == nullptr)
        {
            continue;
        }
        for (const mir::MirInstruction* instruction : node->block()->instructions())
        {
            if (instruction != nullptr && instruction->result() == value)
            {
                bool alreadyAdded = false;
                for (const cfg::CfgNode* definition : definitions)
                {
                    if (definition == node)
                    {
                        alreadyAdded = true;
                        break;
                    }
                }
                if (!alreadyAdded)
                {
                    definitions.push_back(node);
                }
            }
        }
    }
    return placePhiSpecs(definitions, {nullptr, value, value->type()});
}

bool SsaBuilder::placePhiSpecs(
    const std::vector<const cfg::CfgNode*>& definitionNodes,
    PhiSpec spec)
{
    if (dominanceFunction_ == nullptr || dominanceFunction_->dominanceFrontier() == nullptr ||
        spec.type == nullptr)
    {
        return fail("SSA PHI placement requires dominance frontier and variable type");
    }

    std::vector<const cfg::CfgNode*> worklist = definitionNodes;
    std::unordered_set<const cfg::CfgNode*> definitionSet;
    std::unordered_set<const cfg::CfgNode*> hasPhi;
    for (const cfg::CfgNode* node : definitionNodes)
    {
        if (node != nullptr)
        {
            definitionSet.insert(node);
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
        for (const cfg::CfgNode* frontierNode : dominanceFunction_->dominanceFrontier()->frontier(node))
        {
            if (frontierNode == nullptr || !frontierNode->reachable() || frontierNode->block() == nullptr)
            {
                continue;
            }
            if (!hasPhi.insert(frontierNode).second)
            {
                continue;
            }
            phiSpecs_[frontierNode->block()].push_back(spec);
            if (!definitionSet.contains(frontierNode))
            {
                worklist.push_back(frontierNode);
            }
        }
    }
    return true;
}


bool SsaBuilder::materializePlannedPhis()
{
    for (const mir::MirBlock* sourceBlock : sourceFunction_->blocks())
    {
        const auto found = phiSpecs_.find(sourceBlock);
        if (found == phiSpecs_.end())
        {
            continue;
        }
        mir::MirBlock* outputBlock = ssaFunction_->findOutputBlock(sourceBlock);
        if (outputBlock == nullptr)
        {
            return fail("SSA PHI materialization cannot find output block");
        }
        std::vector<mir::MirInstruction*>& phis = phiInstructions_[sourceBlock];
        for (const PhiSpec& spec : found->second)
        {
            mir::MirInstruction* phi = outputArena_->makeInstruction(mir::MirOpcode::Phi, {});
            mir::MirValue* result = makeResult(spec.type);
            if (result == nullptr)
            {
                return false;
            }
            phi->setResult(result);
            if (spec.symbol != nullptr)
            {
                phi->setSymbol(spec.symbol);
            }
            outputBlock->addInstruction(phi);
            phis.push_back(phi);
        }
    }
    return true;
}

bool SsaBuilder::renameFunction()
{
    const dominance::DominatorTree* tree = dominanceFunction_->dominatorTree();
    if (tree == nullptr || tree->root() == nullptr)
    {
        return fail("SSA renaming requires a non-empty dominator tree");
    }

    std::vector<RenameFrame> stack;
    stack.push_back({tree->root(), 0, false, {}, {}});

    while (!stack.empty())
    {
        RenameFrame& frame = stack.back();
        if (!frame.entered)
        {
            if (!enterBlock(frame))
            {
                return false;
            }
            frame.entered = true;
        }

        if (frame.nextChild < frame.node->children().size())
        {
            dominance::DominatorTreeNode* child = frame.node->children()[frame.nextChild++];
            if (child == nullptr)
            {
                return fail("dominator tree contains null child during SSA renaming");
            }
            stack.push_back({child, 0, false, {}, {}});
            continue;
        }

        leaveBlock(frame);
        stack.pop_back();
    }
    return true;
}

bool SsaBuilder::enterBlock(RenameFrame& frame)
{
    if (frame.node == nullptr || frame.node->cfgNode() == nullptr || frame.node->cfgNode()->block() == nullptr)
    {
        return fail("SSA renaming encountered invalid dominator-tree node");
    }
    const mir::MirBlock* sourceBlock = frame.node->cfgNode()->block();
    if (!emitPlannedPhis(sourceBlock, frame))
    {
        return false;
    }
    if (sourceBlock == sourceFunction_->entryBlock() && !emitInitialUndefs(sourceBlock))
    {
        return false;
    }
    if (!cloneBlockInstructions(sourceBlock, frame))
    {
        return false;
    }
    return addPhiIncomingToSuccessors(sourceBlock);
}

void SsaBuilder::leaveBlock(RenameFrame& frame)
{
    for (std::size_t index = frame.valuePushes.size(); index > 0; --index)
    {
        popValue(frame.valuePushes[index - 1]);
    }
    for (std::size_t index = frame.symbolPushes.size(); index > 0; --index)
    {
        popSymbol(frame.symbolPushes[index - 1]);
    }
}

bool SsaBuilder::emitPlannedPhis(const mir::MirBlock* sourceBlock, RenameFrame& frame)
{
    const auto specFound = phiSpecs_.find(sourceBlock);
    if (specFound == phiSpecs_.end())
    {
        return true;
    }
    const auto phiFound = phiInstructions_.find(sourceBlock);
    if (phiFound == phiInstructions_.end() || phiFound->second.size() != specFound->second.size())
    {
        return fail("SSA PHI materialization/spec mapping is incomplete");
    }

    for (std::size_t index = 0; index < specFound->second.size(); ++index)
    {
        const PhiSpec& spec = specFound->second[index];
        mir::MirInstruction* phi = phiFound->second[index];
        if (phi == nullptr || phi->result() == nullptr)
        {
            return fail("SSA PHI has no materialized result");
        }
        if (spec.symbol != nullptr)
        {
            pushSymbol(spec.symbol, phi->result(), &frame);
        }
        else if (spec.sourceValue != nullptr)
        {
            pushValue(spec.sourceValue, phi->result(), &frame);
        }
        else
        {
            return fail("SSA PHI spec has no variable identity");
        }
    }
    return true;
}

bool SsaBuilder::emitInitialUndefs(const mir::MirBlock* sourceBlock)
{
    mir::MirBlock* outputBlock = ssaFunction_->findOutputBlock(sourceBlock);
    if (outputBlock == nullptr)
    {
        return fail("SSA undef seeding cannot find entry output block");
    }

    for (sema::Symbol* symbol : promotedSymbolOrder_)
    {
        if (symbol == nullptr || symbol->kind() != sema::SymbolKind::Variable)
        {
            continue;
        }
        mir::MirInstruction* undef = outputArena_->makeInstruction(mir::MirOpcode::Undef, {});
        mir::MirValue* value = makeResult(symbol->type());
        undef->setResult(value);
        undef->setSymbol(symbol);
        outputBlock->addInstruction(undef);
        pushSymbol(symbol, value, nullptr);
    }

    for (const mir::MirValue* sourceValue : multiDefinedValueOrder_)
    {
        mir::MirInstruction* undef = outputArena_->makeInstruction(mir::MirOpcode::Undef, {});
        mir::MirValue* value = makeResult(sourceValue->type());
        undef->setResult(value);
        outputBlock->addInstruction(undef);
        pushValue(sourceValue, value, nullptr);
    }
    return true;
}

bool SsaBuilder::cloneBlockInstructions(const mir::MirBlock* sourceBlock, RenameFrame& frame)
{
    if (sourceBlock == nullptr)
    {
        return fail("SSA builder cannot clone null source block");
    }
    for (const mir::MirInstruction* instruction : sourceBlock->instructions())
    {
        if (!cloneInstruction(instruction, frame))
        {
            return false;
        }
    }
    return true;
}

bool SsaBuilder::cloneInstruction(
    const mir::MirInstruction* sourceInstruction,
    RenameFrame& frame)
{
    if (sourceInstruction == nullptr)
    {
        return fail("SSA builder encountered null MIR instruction");
    }

    if (sourceInstruction->opcode() == mir::MirOpcode::LoadSymbol &&
        isPromotedSymbol(sourceInstruction->symbol()))
    {
        if (sourceInstruction->result() == nullptr)
        {
            return fail("promoted load.symbol has no source result");
        }
        mir::MirValue* current = currentSymbolValue(sourceInstruction->symbol());
        if (current == nullptr)
        {
            return fail("promoted symbol read has no current SSA definition");
        }
        directValues_[sourceInstruction->result()] = current;
        return true;
    }

    if (sourceInstruction->opcode() == mir::MirOpcode::StoreSymbol &&
        isPromotedSymbol(sourceInstruction->symbol()))
    {
        if (sourceInstruction->operands().size() != 1)
        {
            return fail("promoted store.symbol must contain exactly one source operand");
        }
        mir::MirValue* value = resolveValue(sourceInstruction->operands()[0]);
        if (value == nullptr)
        {
            return false;
        }
        pushSymbol(sourceInstruction->symbol(), value, &frame);
        return true;
    }

    mir::MirBlock* outputBlock = ssaFunction_->findOutputBlock(
        frame.node->cfgNode()->block());
    if (outputBlock == nullptr)
    {
        return fail("SSA instruction clone cannot find output block");
    }

    mir::MirInstruction* outputInstruction = cloneInstructionShell(sourceInstruction, outputBlock);
    if (outputInstruction == nullptr)
    {
        return false;
    }

    for (const mir::MirValue* operand : sourceInstruction->operands())
    {
        mir::MirValue* resolved = resolveValue(operand);
        if (resolved == nullptr)
        {
            return false;
        }
        outputInstruction->addOperand(resolved);
    }

    if (sourceInstruction->result() != nullptr)
    {
        mir::MirValue* result = makeResult(sourceInstruction->result()->type());
        outputInstruction->setResult(result);
        if (isMultiDefinedValue(sourceInstruction->result()))
        {
            pushValue(sourceInstruction->result(), result, &frame);
        }
        else
        {
            directValues_[sourceInstruction->result()] = result;
        }

        if (sourceInstruction->opcode() == mir::MirOpcode::Parameter)
        {
            if (!sourceInstruction->hasImmediate() ||
                sourceInstruction->immediate() >= outputParameters_.size())
            {
                return fail("SSA parameter clone has invalid positional index");
            }
            outputParameters_[sourceInstruction->immediate()] = result;
        }
    }
    return true;
}

mir::MirInstruction* SsaBuilder::cloneInstructionShell(
    const mir::MirInstruction* sourceInstruction,
    mir::MirBlock* outputBlock)
{
    if (sourceInstruction == nullptr || outputBlock == nullptr)
    {
        fail("cannot clone invalid MIR instruction shell");
        return nullptr;
    }
    if (sourceInstruction->opcode() == mir::MirOpcode::Phi ||
        sourceInstruction->opcode() == mir::MirOpcode::Undef)
    {
        fail("source MIR unexpectedly contains SSA-only opcode");
        return nullptr;
    }

    mir::MirInstruction* outputInstruction = outputArena_->makeInstruction(
        sourceInstruction->opcode(),
        sourceInstruction->range());
    outputInstruction->setSymbol(sourceInstruction->symbol());
    outputInstruction->setAuxiliaryType(sourceInstruction->auxiliaryType());
    if (sourceInstruction->hasConstant())
    {
        outputInstruction->setConstant(sourceInstruction->constant());
    }
    if (sourceInstruction->hasConversion())
    {
        outputInstruction->setConversionKind(sourceInstruction->conversionKind());
    }
    if (sourceInstruction->hasImmediate())
    {
        outputInstruction->setImmediate(sourceInstruction->immediate());
    }
    if (sourceInstruction->hasOffset())
    {
        outputInstruction->setOffset(sourceInstruction->offset());
    }
    if (sourceInstruction->firstTarget() != nullptr)
    {
        mir::MirBlock* target = ssaFunction_->findOutputBlock(sourceInstruction->firstTarget());
        if (target == nullptr)
        {
            fail("reachable SSA instruction targets an excluded MIR block");
            return nullptr;
        }
        outputInstruction->setFirstTarget(target);
    }
    if (sourceInstruction->secondTarget() != nullptr)
    {
        mir::MirBlock* target = ssaFunction_->findOutputBlock(sourceInstruction->secondTarget());
        if (target == nullptr)
        {
            fail("reachable SSA branch targets an excluded MIR block");
            return nullptr;
        }
        outputInstruction->setSecondTarget(target);
    }
    outputBlock->addInstruction(outputInstruction);
    return outputInstruction;
}

bool SsaBuilder::addPhiIncomingToSuccessors(const mir::MirBlock* sourceBlock)
{
    const cfg::CfgNode* sourceNode = cfgFunction_->findNode(sourceBlock);
    mir::MirBlock* outputPredecessor = ssaFunction_->findOutputBlock(sourceBlock);
    if (sourceNode == nullptr || outputPredecessor == nullptr)
    {
        return fail("SSA PHI incoming construction cannot resolve predecessor block");
    }

    for (const cfg::CfgNode* successor : sourceNode->successors())
    {
        if (successor == nullptr || !successor->reachable() || successor->block() == nullptr)
        {
            continue;
        }
        const auto specFound = phiSpecs_.find(successor->block());
        if (specFound == phiSpecs_.end())
        {
            continue;
        }
        const auto phiFound = phiInstructions_.find(successor->block());
        if (phiFound == phiInstructions_.end() ||
            phiFound->second.size() != specFound->second.size())
        {
            return fail("SSA PHI instruction/spec mapping is incomplete");
        }

        for (std::size_t index = 0; index < specFound->second.size(); ++index)
        {
            const PhiSpec& spec = specFound->second[index];
            mir::MirValue* value = spec.symbol != nullptr
                ? currentSymbolValue(spec.symbol)
                : currentVirtualValue(spec.sourceValue);
            if (value == nullptr)
            {
                return fail("SSA PHI predecessor has no reaching definition");
            }
            if (!addPhiIncomingOnce(phiFound->second[index], outputPredecessor, value))
            {
                return false;
            }
        }
    }
    return true;
}

mir::MirValue* SsaBuilder::resolveValue(const mir::MirValue* sourceValue)
{
    if (sourceValue == nullptr)
    {
        fail("SSA builder cannot resolve null MIR value");
        return nullptr;
    }
    if (isMultiDefinedValue(sourceValue))
    {
        mir::MirValue* value = currentVirtualValue(sourceValue);
        if (value == nullptr)
        {
            fail("multi-defined MIR value has no current SSA version");
        }
        return value;
    }

    const auto found = directValues_.find(sourceValue);
    if (found == directValues_.end() || found->second == nullptr)
    {
        fail("MIR operand has no cloned/reaching SSA value");
        return nullptr;
    }
    return found->second;
}

mir::MirValue* SsaBuilder::currentSymbolValue(const sema::Symbol* symbol)
{
    const auto found = symbolStacks_.find(symbol);
    return found == symbolStacks_.end() || found->second.empty() ? nullptr : found->second.back();
}

mir::MirValue* SsaBuilder::currentVirtualValue(const mir::MirValue* value)
{
    const auto found = valueStacks_.find(value);
    return found == valueStacks_.end() || found->second.empty() ? nullptr : found->second.back();
}

mir::MirValue* SsaBuilder::makeResult(sema::Type* type)
{
    if (type == nullptr)
    {
        fail("SSA result cannot have null semantic type");
        return nullptr;
    }
    return outputArena_->makeValue(type);
}

void SsaBuilder::pushSymbol(
    const sema::Symbol* symbol,
    mir::MirValue* value,
    RenameFrame* frame)
{
    symbolStacks_[symbol].push_back(value);
    if (frame != nullptr)
    {
        frame->symbolPushes.push_back(symbol);
    }
}

void SsaBuilder::pushValue(
    const mir::MirValue* source,
    mir::MirValue* value,
    RenameFrame* frame)
{
    valueStacks_[source].push_back(value);
    if (frame != nullptr)
    {
        frame->valuePushes.push_back(source);
    }
}

void SsaBuilder::popSymbol(const sema::Symbol* symbol)
{
    auto found = symbolStacks_.find(symbol);
    if (found != symbolStacks_.end() && !found->second.empty())
    {
        found->second.pop_back();
    }
}

void SsaBuilder::popValue(const mir::MirValue* source)
{
    auto found = valueStacks_.find(source);
    if (found != valueStacks_.end() && !found->second.empty())
    {
        found->second.pop_back();
    }
}

bool SsaBuilder::isReachableBlock(const mir::MirBlock* block) const noexcept
{
    const cfg::CfgNode* node = cfgFunction_ == nullptr ? nullptr : cfgFunction_->findNode(block);
    return node != nullptr && node->reachable();
}

bool SsaBuilder::isPromotedSymbol(const sema::Symbol* symbol) const noexcept
{
    return symbol != nullptr && promotedSymbols_.contains(symbol);
}

bool SsaBuilder::isMultiDefinedValue(const mir::MirValue* value) const noexcept
{
    return value != nullptr && multiDefinedValues_.contains(value);
}

bool SsaBuilder::addPhiIncomingOnce(
    mir::MirInstruction* phi,
    mir::MirBlock* predecessor,
    mir::MirValue* value)
{
    if (phi == nullptr || phi->opcode() != mir::MirOpcode::Phi || predecessor == nullptr || value == nullptr)
    {
        return fail("cannot add invalid SSA PHI incoming edge");
    }
    for (const mir::MirPhiIncoming& incoming : phi->phiIncoming())
    {
        if (incoming.block == predecessor)
        {
            return incoming.value == value
                ? true
                : fail("same SSA PHI predecessor attempted to provide two different values");
        }
    }
    phi->addPhiIncoming(predecessor, value);
    return true;
}

bool SsaBuilder::fail(std::string message)
{
    if (error_.empty())
    {
        error_ = std::move(message);
    }
    return false;
}
}
