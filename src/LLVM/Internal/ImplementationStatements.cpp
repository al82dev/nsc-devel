#include "nativesharp/LLVM/LlvmLowerer.h"

#include "../LlvmCApi.h"
#include "../SignedDivisionCorrection.h"

#include "nativesharp/LLVM/LlvmModule.h"
#include "nativesharp/LLVM/LlvmTrapReason.h"
#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Passes/OptimizationModule.h"
#include "nativesharp/Passes/OptimizationVerifier.h"
#include "nativesharp/Sema/ConstantValue.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeKind.h"
#include "nativesharp/Target/TargetDataLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

#include "Implementation.h"
#include "ImplementationSupport.h"

using namespace llvm_lowerer_detail;

bool LlvmLowerer::Implementation::lowerFunctions(std::string& error)
    {
        for (const mir::MirFunction* function : sourceModule_->functions())
        {
            if (!function->defined())
            {
                continue;
            }
            if (!lowerFunction(function, error))
            {
                return false;
            }
        }
        return true;
    }

bool LlvmLowerer::Implementation::lowerFunction(
    const mir::MirFunction* function,
    std::string& error)
    {
        currentMirFunction_ = function;
        currentFunction_ = functionValues_.at(function->symbol());
        valueMap_.clear();
        blockEntry_.clear();
        blockExit_.clear();
        localStorage_.clear();
        deferredPhis_.clear();

        if (function->entryBlock() == nullptr)
        {
            error = "defined MIR function '" + function->symbol()->name() + "' has no entry block";
            return false;
        }

        const mir::MirBlock* entry = function->entryBlock();
        if (!entry->instructions().empty() && entry->instructions().front()->opcode() == mir::MirOpcode::Phi)
        {
            error = "LLVM lowering does not permit a phi in the MIR entry block";
            return false;
        }

        LLVMBasicBlockRef prologue = LLVMAppendBasicBlockInContext(context_, currentFunction_, "nsc.prologue");
        if (prologue == nullptr)
        {
            error = "LLVM failed to create function prologue";
            return false;
        }

        for (const mir::MirBlock* block : function->blocks())
        {
            LLVMBasicBlockRef llvmBlock = LLVMAppendBasicBlockInContext(
                context_, currentFunction_, blockName(block).c_str());
            if (llvmBlock == nullptr)
            {
                error = "LLVM failed to create a basic block in function '" + function->symbol()->name() + "'";
                return false;
            }
            blockEntry_[block] = llvmBlock;
        }

        LLVMPositionBuilderAtEnd(builder_, prologue);

        for (sema::Symbol* local : function->locals())
        {
            LLVMTypeRef type = llvmType(local->type(), error);
            if (type == nullptr)
            {
                return false;
            }
            std::string name = local->name() + ".addr";
            LLVMValueRef storage = LLVMBuildAlloca(builder_, type, name.c_str());
            if (storage == nullptr)
            {
                error = "LLVM failed to allocate local '" + local->name() + "'";
                return false;
            }
            localStorage_[local] = storage;
        }
        LLVMBuildBr(builder_, blockEntry_.at(entry));

        for (const mir::MirBlock* block : function->blocks())
        {
            LLVMPositionBuilderAtEnd(builder_, blockEntry_.at(block));
            for (const mir::MirInstruction* instruction : block->instructions())
            {
                if (!lowerInstruction(instruction, error))
                {
                    if (error.empty())
                    {
                        error = "failed to lower MIR instruction in function '" + function->symbol()->name() + "'";
                    }
                    return false;
                }
            }
            blockExit_[block] = LLVMGetInsertBlock(builder_);
        }

        for (const DeferredPhi& deferred : deferredPhis_)
        {
            if (!completePhi(deferred, error))
            {
                return false;
            }
        }
        return true;
    }

bool LlvmLowerer::Implementation::lowerInstruction(
    const mir::MirInstruction* instruction,
    std::string& error)
{
    if (instruction == nullptr)
    {
        error = "optimized module contains a null MIR instruction";
        return false;
    }

    LLVMValueRef result = nullptr;
    const std::string name = valueName(instruction->result());

    switch (instruction->opcode())
    {
        case mir::MirOpcode::Parameter:
        case mir::MirOpcode::Undef:
        case mir::MirOpcode::Phi:
        case mir::MirOpcode::Constant:
        case mir::MirOpcode::Copy:
            return lowerInstructionGroup01(instruction, result, name, error);

        case mir::MirOpcode::LoadSymbol:
        case mir::MirOpcode::StoreSymbol:
        case mir::MirOpcode::AddressOfSymbol:
            return lowerInstructionGroup02(instruction, result, name, error);

        case mir::MirOpcode::Load:
        case mir::MirOpcode::Store:
        case mir::MirOpcode::Convert:
        case mir::MirOpcode::Negate:
            return lowerInstructionGroup03(instruction, result, name, error);

        case mir::MirOpcode::BitNot:
        case mir::MirOpcode::LogicalNot:
        case mir::MirOpcode::Add:
        case mir::MirOpcode::Subtract:
        case mir::MirOpcode::Multiply:
        case mir::MirOpcode::Divide:
        case mir::MirOpcode::Remainder:
        case mir::MirOpcode::BitAnd:
        case mir::MirOpcode::BitOr:
        case mir::MirOpcode::BitXor:
        case mir::MirOpcode::ShiftLeft:
        case mir::MirOpcode::ShiftRight:
        case mir::MirOpcode::CompareEqual:
        case mir::MirOpcode::CompareNotEqual:
        case mir::MirOpcode::CompareLess:
        case mir::MirOpcode::CompareLessEqual:
        case mir::MirOpcode::CompareGreater:
        case mir::MirOpcode::CompareGreaterEqual:
        case mir::MirOpcode::PointerAdd:
        case mir::MirOpcode::PointerSubtract:
        case mir::MirOpcode::PointerDifference:
            return lowerInstructionGroup03Remainder(instruction, result, name, error);

        case mir::MirOpcode::Call:
        case mir::MirOpcode::BoundsCheck:
        case mir::MirOpcode::FieldAddress:
        case mir::MirOpcode::IndexAddress:
        case mir::MirOpcode::ExtractField:
        case mir::MirOpcode::Allocate:
        case mir::MirOpcode::AllocateArray:
        case mir::MirOpcode::StackAllocate:
        case mir::MirOpcode::Deallocate:
        case mir::MirOpcode::DeallocateArray:
            return lowerInstructionGroup04(instruction, result, name, error);

        case mir::MirOpcode::Jump:
        case mir::MirOpcode::Branch:
        case mir::MirOpcode::Return:
        case mir::MirOpcode::Unreachable:
            return lowerInstructionGroup05(instruction, result, name, error);

    }

    return finishInstruction(instruction, result, error);
}

bool LlvmLowerer::Implementation::lowerInstructionGroup01(
    const mir::MirInstruction* instruction,
    LLVMValueRef& result,
    const std::string& name,
    std::string& error)
{
    switch (instruction->opcode())
    {
        case mir::MirOpcode::Parameter:
        {
            if (!instruction->hasImmediate())
            {
                return failInstruction(instruction, "parameter has no parameter index", error);
            }
            result = LLVMGetParam(currentFunction_, static_cast<unsigned>(instruction->immediate()));
            if (result != nullptr && instruction->symbol() != nullptr)
            {
                LLVMSetValueName2(
                    result,
                    instruction->symbol()->name().c_str(),
                    instruction->symbol()->name().size());
            }
            break;
        }

        case mir::MirOpcode::Undef:
        {
            LLVMTypeRef type = llvmType(instruction->result()->type(), error);
            result = type != nullptr ? LLVMGetUndef(type) : nullptr;
            break;
        }

        case mir::MirOpcode::Phi:
        {
            LLVMTypeRef type = llvmType(instruction->result()->type(), error);
            if (type == nullptr)
            {
                return false;
            }
            result = LLVMBuildPhi(builder_, type, name.c_str());
            deferredPhis_.push_back(DeferredPhi{instruction, result});
            break;
        }

        case mir::MirOpcode::Constant:
            result = llvmConstant(instruction->constant(), instruction->result()->type(), error, true);
            break;

        case mir::MirOpcode::Copy:
            result = operand(instruction, 0, error);
            break;
        default:
            break;
    }

    return finishInstruction(instruction, result, error);
}

bool LlvmLowerer::Implementation::lowerInstructionGroup02(
    const mir::MirInstruction* instruction,
    LLVMValueRef& result,
    const std::string& name,
    std::string& error)
{
    switch (instruction->opcode())
    {
        case mir::MirOpcode::LoadSymbol:
        {
            LLVMValueRef storage = symbolStorage(instruction->symbol());
            LLVMTypeRef type = llvmType(instruction->result()->type(), error);
            if (storage == nullptr || type == nullptr)
            {
                if (error.empty())
                {
                    return failInstruction(instruction, "load.symbol references unknown storage", error);
                }
                return false;
            }
            result = LLVMBuildLoad2(builder_, type, storage, name.c_str());
            break;
        }

        case mir::MirOpcode::StoreSymbol:
        {
            LLVMValueRef storage = symbolStorage(instruction->symbol());
            LLVMValueRef value = operand(instruction, 0, error);
            if (storage == nullptr || value == nullptr)
            {
                if (error.empty())
                {
                    return failInstruction(instruction, "store.symbol references unknown storage/value", error);
                }
                return false;
            }
            LLVMBuildStore(builder_, value, storage);
            return true;
        }

        case mir::MirOpcode::AddressOfSymbol:
            result = symbolStorage(instruction->symbol());
            if (result == nullptr)
            {
                return failInstruction(instruction, "address.symbol references unknown storage", error);
            }
            break;
        default:
            break;
    }

    return finishInstruction(instruction, result, error);
}

bool LlvmLowerer::Implementation::lowerInstructionGroup03(
    const mir::MirInstruction* instruction,
    LLVMValueRef& result,
    const std::string& name,
    std::string& error)
{
    switch (instruction->opcode())
    {
        case mir::MirOpcode::Load:
        {
            LLVMValueRef pointer = operand(instruction, 0, error);
            LLVMTypeRef type = llvmType(instruction->result()->type(), error);
            if (pointer == nullptr || type == nullptr)
            {
                return false;
            }
            result = LLVMBuildLoad2(builder_, type, pointer, name.c_str());
            break;
        }

        case mir::MirOpcode::Store:
        {
            LLVMValueRef pointer = operand(instruction, 0, error);
            LLVMValueRef value = operand(instruction, 1, error);
            if (pointer == nullptr || value == nullptr)
            {
                return false;
            }
            LLVMBuildStore(builder_, value, pointer);
            return true;
        }

        case mir::MirOpcode::Convert:
            result = lowerConversion(instruction, error);
            break;

        case mir::MirOpcode::Negate:
        {
            LLVMValueRef value = operand(instruction, 0, error);
            if (value == nullptr)
            {
                return false;
            }
            result = isFloating(instruction->result()->type())
                ? LLVMBuildFNeg(builder_, value, name.c_str())
                : LLVMBuildNeg(builder_, value, name.c_str());
            break;
        }
        default:
            break;
    }

    return finishInstruction(instruction, result, error);
}

bool LlvmLowerer::Implementation::lowerInstructionGroup03Remainder(
    const mir::MirInstruction* instruction,
    LLVMValueRef& result,
    const std::string& name,
    std::string& error)
{
    switch (instruction->opcode())
    {

        case mir::MirOpcode::BitNot:
        case mir::MirOpcode::LogicalNot:
        {
            LLVMValueRef value = operand(instruction, 0, error);
            if (value == nullptr)
            {
                return false;
            }
            result = LLVMBuildNot(builder_, value, name.c_str());
            break;
        }

        case mir::MirOpcode::Add:
        case mir::MirOpcode::Subtract:
        case mir::MirOpcode::Multiply:
            result = lowerBasicArithmetic(instruction, error);
            break;

        case mir::MirOpcode::Divide:
        case mir::MirOpcode::Remainder:
            result = lowerDivision(instruction, error);
            break;

        case mir::MirOpcode::BitAnd:
        case mir::MirOpcode::BitOr:
        case mir::MirOpcode::BitXor:
            result = lowerBitwise(instruction, error);
            break;

        case mir::MirOpcode::ShiftLeft:
        case mir::MirOpcode::ShiftRight:
            result = lowerShift(instruction, error);
            break;

        case mir::MirOpcode::CompareEqual:
        case mir::MirOpcode::CompareNotEqual:
        case mir::MirOpcode::CompareLess:
        case mir::MirOpcode::CompareLessEqual:
        case mir::MirOpcode::CompareGreater:
        case mir::MirOpcode::CompareGreaterEqual:
            result = lowerComparison(instruction, error);
            break;

        case mir::MirOpcode::PointerAdd:
        case mir::MirOpcode::PointerSubtract:
            result = lowerPointerArithmetic(instruction, error);
            break;

        case mir::MirOpcode::PointerDifference:
            result = lowerPointerDifference(instruction, error);
            break;
        default:
            break;
    }

    return finishInstruction(instruction, result, error);
}

bool LlvmLowerer::Implementation::lowerInstructionGroup04(
    const mir::MirInstruction* instruction,
    LLVMValueRef& result,
    const std::string& name,
    std::string& error)
{
    switch (instruction->opcode())
    {
        case mir::MirOpcode::Call:
            result = lowerCall(instruction, error);
            if (instruction->result() == nullptr)
            {
                return result != nullptr || error.empty();
            }
            break;

        case mir::MirOpcode::BoundsCheck:
            return lowerBoundsCheck(instruction, error);

        case mir::MirOpcode::FieldAddress:
            result = lowerFieldAddress(instruction, error);
            break;

        case mir::MirOpcode::IndexAddress:
            result = lowerIndexAddress(instruction, error);
            break;

        case mir::MirOpcode::ExtractField:
            result = lowerExtractField(instruction, error);
            break;

        case mir::MirOpcode::Allocate:
        {
            const std::optional<target::TypeLayout> layout = layoutOf(instruction->auxiliaryType());
            if (!layout.has_value() || layout->size == 0)
            {
                error = "cannot determine allocation size for type '" +
                    instruction->auxiliaryType()->displayName() + "'";
                return false;
            }
            LLVMValueRef size = LLVMConstInt(LLVMInt64TypeInContext(context_), layout->size, 0);
            LLVMValueRef arguments[] = {size};
            result = LLVMBuildCall2(
                builder_, allocationFunctionType_, allocationFunction_, arguments, 1, name.c_str());
            break;
        }

        case mir::MirOpcode::AllocateArray:
        case mir::MirOpcode::StackAllocate:
            result = lowerCountedAllocation(instruction, error);
            break;

        case mir::MirOpcode::Deallocate:
        case mir::MirOpcode::DeallocateArray:
        {
            LLVMValueRef pointer = operand(instruction, 0, error);
            if (pointer == nullptr)
            {
                return false;
            }
            LLVMValueRef arguments[] = {pointer};
            LLVMBuildCall2(
                builder_, deallocationFunctionType_, deallocationFunction_, arguments, 1, "");
            return true;
        }
        default:
            break;
    }

    return finishInstruction(instruction, result, error);
}

bool LlvmLowerer::Implementation::lowerInstructionGroup05(
    const mir::MirInstruction* instruction,
    LLVMValueRef& result,
    const std::string& name,
    std::string& error)
{
    (void)name;
    switch (instruction->opcode())
    {
        case mir::MirOpcode::Jump:
        {
            auto target = blockEntry_.find(instruction->firstTarget());
            if (target == blockEntry_.end())
            {
                return failInstruction(instruction, "jump target is not in current function", error);
            }
            LLVMBuildBr(builder_, target->second);
            return true;
        }

        case mir::MirOpcode::Branch:
        {
            LLVMValueRef condition = operand(instruction, 0, error);
            auto trueTarget = blockEntry_.find(instruction->firstTarget());
            auto falseTarget = blockEntry_.find(instruction->secondTarget());
            if (condition == nullptr || trueTarget == blockEntry_.end() || falseTarget == blockEntry_.end())
            {
                return failInstruction(instruction, "branch has unresolved condition or target", error);
            }
            LLVMBuildCondBr(builder_, condition, trueTarget->second, falseTarget->second);
            return true;
        }

        case mir::MirOpcode::Return:
            if (instruction->operands().empty())
            {
                LLVMBuildRetVoid(builder_);
            }
            else
            {
                LLVMValueRef value = operand(instruction, 0, error);
                if (value == nullptr)
                {
                    return false;
                }
                LLVMBuildRet(builder_, value);
            }
            return true;

        case mir::MirOpcode::Unreachable:
            LLVMBuildUnreachable(builder_);
            return true;
        default:
            break;
    }

    return finishInstruction(instruction, result, error);
}

bool LlvmLowerer::Implementation::finishInstruction(
    const mir::MirInstruction* instruction,
    LLVMValueRef result,
    std::string& error)
{
    if (instruction->result() != nullptr)
    {
        if (result == nullptr)
        {
            if (error.empty())
            {
                return failInstruction(instruction, "LLVM builder returned null result", error);
            }
            return false;
        }
        valueMap_[instruction->result()] = result;
    }
    return error.empty();
}

bool LlvmLowerer::Implementation::completePhi(const DeferredPhi& deferred, std::string& error)
    {
        if (deferred.instruction == nullptr || deferred.llvmPhi == nullptr)
        {
            error = "deferred LLVM phi record is invalid";
            return false;
        }

        std::vector<LLVMValueRef> values;
        std::vector<LLVMBasicBlockRef> blocks;
        values.reserve(deferred.instruction->phiIncoming().size());
        blocks.reserve(deferred.instruction->phiIncoming().size());
        for (const mir::MirPhiIncoming& incoming : deferred.instruction->phiIncoming())
        {
            auto value = valueMap_.find(incoming.value);
            auto block = blockExit_.find(incoming.block);
            if (value == valueMap_.end() || block == blockExit_.end())
            {
                error = "phi incoming value/block was not lowered before LLVM completion";
                return false;
            }
            values.push_back(value->second);
            blocks.push_back(block->second);
        }
        LLVMAddIncoming(
            deferred.llvmPhi,
            values.empty() ? nullptr : values.data(),
            blocks.empty() ? nullptr : blocks.data(),
            static_cast<unsigned>(values.size()));
        return true;
    }

}
