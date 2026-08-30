#include "nativesharp/Passes/OptimizationBuilder.h"

#include "nativesharp/MIR/MirArena.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/SSA/SsaModule.h"

namespace nativesharp::passes
{
OptimizationBuilder::OptimizationBuilder(OptimizationArena& arena) : arena_(arena) {}

OptimizationModule* OptimizationBuilder::build(const ssa::SsaModule* source)
{
    error_.clear();
    blocks_.clear();
    values_.clear();

    if (source == nullptr || source->module() == nullptr)
    {
        fail("optimization builder requires a verified SSA module");
        return nullptr;
    }

    mir::MirModule* output = arena_.mirArena().makeModule();
    if (!cloneModule(source->module(), output))
    {
        return nullptr;
    }
    return arena_.makeModule(source, output);
}

const std::string& OptimizationBuilder::error() const noexcept { return error_; }

bool OptimizationBuilder::cloneModule(
    const mir::MirModule* source,
    mir::MirModule* output)
{
    if (source == nullptr || output == nullptr)
    {
        return fail("optimization builder cannot clone null MIR module");
    }

    for (const mir::MirGlobal* global : source->globals())
    {
        if (global == nullptr || global->symbol() == nullptr)
        {
            return fail("optimization builder encountered invalid global");
        }
        mir::MirGlobal* copy = arena_.mirArena().makeGlobal(global->symbol());
        if (global->hasExplicitInitializer())
        {
            copy->setExplicitInitializer(global->initializer());
        }
        output->addGlobal(copy);
    }

    for (const mir::MirFunction* function : source->functions())
    {
        if (!cloneFunction(function, output))
        {
            return false;
        }
    }
    return true;
}

bool OptimizationBuilder::cloneFunction(
    const mir::MirFunction* source,
    mir::MirModule* output)
{
    if (source == nullptr || source->symbol() == nullptr || output == nullptr)
    {
        return fail("optimization builder encountered invalid function");
    }

    blocks_.clear();
    values_.clear();

    mir::MirFunction* copy = arena_.mirArena().makeFunction(source->symbol());
    copy->setDefined(source->defined());
    output->addFunction(copy);

    if (!source->defined())
    {
        return true;
    }

    return cloneDefinedFunction(source, copy);
}

bool OptimizationBuilder::cloneDefinedFunction(
    const mir::MirFunction* source,
    mir::MirFunction* output)
{
    for (sema::Symbol* local : source->locals())
    {
        output->addLocal(local);
    }

    for (const mir::MirBlock* sourceBlock : source->blocks())
    {
        if (sourceBlock == nullptr)
        {
            return fail("optimization builder encountered null source block");
        }
        mir::MirBlock* outputBlock = arena_.mirArena().makeBlock(sourceBlock->name());
        blocks_.emplace(sourceBlock, outputBlock);
        output->addBlock(outputBlock);
    }

    for (const mir::MirBlock* sourceBlock : source->blocks())
    {
        for (const mir::MirInstruction* instruction : sourceBlock->instructions())
        {
            if (instruction != nullptr && instruction->result() != nullptr)
            {
                mir::MirValue* value = arena_.mirArena().makeValue(instruction->result()->type());
                values_.emplace(instruction->result(), value);
            }
        }
    }

    for (const mir::MirValue* parameter : source->parameters())
    {
        const auto found = values_.find(parameter);
        if (found == values_.end())
        {
            return fail("optimization builder cannot map SSA parameter value");
        }
        output->addParameter(found->second);
    }

    for (const mir::MirBlock* sourceBlock : source->blocks())
    {
        mir::MirBlock* outputBlock = blocks_.at(sourceBlock);
        for (const mir::MirInstruction* sourceInstruction : sourceBlock->instructions())
        {
            if (sourceInstruction == nullptr)
            {
                return fail("optimization builder encountered null SSA instruction");
            }
            mir::MirInstruction* outputInstruction =
                arena_.mirArena().makeInstruction(sourceInstruction->opcode(), sourceInstruction->range());
            if (!cloneInstruction(sourceInstruction, outputInstruction))
            {
                return false;
            }
            outputBlock->addInstruction(outputInstruction);
        }
    }

    return true;
}

bool OptimizationBuilder::cloneInstruction(
    const mir::MirInstruction* source,
    mir::MirInstruction* output)
{
    if (source->result() != nullptr)
    {
        const auto result = values_.find(source->result());
        if (result == values_.end())
        {
            return fail("optimization builder cannot map instruction result");
        }
        output->setResult(result->second);
    }

    for (const mir::MirValue* operand : source->operands())
    {
        const auto found = values_.find(operand);
        if (found == values_.end())
        {
            return fail("optimization builder cannot map instruction operand");
        }
        output->addOperand(found->second);
    }

    for (const mir::MirPhiIncoming& incoming : source->phiIncoming())
    {
        const auto block = blocks_.find(incoming.block);
        const auto value = values_.find(incoming.value);
        if (block == blocks_.end() || value == values_.end())
        {
            return fail("optimization builder cannot map PHI incoming pair");
        }
        output->addPhiIncoming(block->second, value->second);
    }

    output->setSymbol(source->symbol());
    output->setAuxiliaryType(source->auxiliaryType());
    if (source->hasConstant())
    {
        output->setConstant(source->constant());
    }
    if (source->hasConversion())
    {
        output->setConversionKind(source->conversionKind());
    }
    if (source->hasImmediate())
    {
        output->setImmediate(source->immediate());
    }
    if (source->hasOffset())
    {
        output->setOffset(source->offset());
    }
    if (source->firstTarget() != nullptr)
    {
        const auto target = blocks_.find(source->firstTarget());
        if (target == blocks_.end())
        {
            return fail("optimization builder cannot map first block target");
        }
        output->setFirstTarget(target->second);
    }
    if (source->secondTarget() != nullptr)
    {
        const auto target = blocks_.find(source->secondTarget());
        if (target == blocks_.end())
        {
            return fail("optimization builder cannot map second block target");
        }
        output->setSecondTarget(target->second);
    }

    return true;
}

bool OptimizationBuilder::fail(std::string message)
{
    error_ = std::move(message);
    return false;
}
}
