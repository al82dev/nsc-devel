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

LLVMValueRef LlvmLowerer::Implementation::lowerBasicArithmetic(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef left = operand(instruction, 0, error);
        LLVMValueRef right = operand(instruction, 1, error);
        if (left == nullptr || right == nullptr)
        {
            return nullptr;
        }

        const std::string name = valueName(instruction->result());
        const bool floating = isFloating(instruction->result()->type());
        switch (instruction->opcode())
        {
            case mir::MirOpcode::Add:
                return floating ? LLVMBuildFAdd(builder_, left, right, name.c_str())
                                : LLVMBuildAdd(builder_, left, right, name.c_str());
            case mir::MirOpcode::Subtract:
                return floating ? LLVMBuildFSub(builder_, left, right, name.c_str())
                                : LLVMBuildSub(builder_, left, right, name.c_str());
            case mir::MirOpcode::Multiply:
                return floating ? LLVMBuildFMul(builder_, left, right, name.c_str())
                                : LLVMBuildMul(builder_, left, right, name.c_str());
            default:
                error = "internal LLVM arithmetic dispatch error";
                return nullptr;
        }
    }

LLVMValueRef LlvmLowerer::Implementation::lowerDivision(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef left = operand(instruction, 0, error);
        LLVMValueRef right = operand(instruction, 1, error);
        if (left == nullptr || right == nullptr)
        {
            return nullptr;
        }

        sema::Type* type = instruction->result()->type();
        LLVMTypeRef llvmResultType = llvmType(type, error);
        if (llvmResultType == nullptr)
        {
            return nullptr;
        }
        const std::string name = valueName(instruction->result());

        if (isFloating(type))
        {
            LLVMValueRef zero = LLVMConstReal(llvmResultType, 0.0);
            LLVMValueRef valid = LLVMBuildFCmp(builder_, LLVMRealUNE, right, zero, "div.nonzero");
            if (!guardOrTrap(valid, LlvmTrapReason::DivisionByZero, "div.zero", error))
            {
                return nullptr;
            }
            if (instruction->opcode() != mir::MirOpcode::Divide)
            {
                error = "floating remainder reached LLVM lowering";
                return nullptr;
            }
            return LLVMBuildFDiv(builder_, left, right, name.c_str());
        }

        LLVMValueRef zero = LLVMConstInt(llvmResultType, 0, 0);
        LLVMValueRef nonzero = LLVMBuildICmp(builder_, LLVMIntNE, right, zero, "div.nonzero");
        if (!guardOrTrap(nonzero, LlvmTrapReason::DivisionByZero, "div.zero", error))
        {
            return nullptr;
        }

        if (isSignedInteger(type))
        {
            // CAMBIO DE COMPORTAMIENTO DELIBERADO, 2026-09-03. Aqui habia una guarda que ABORTABA
            // con LlvmTrapReason::SignedDivisionOverflow cuando MinValue se dividia entre -1.
            // El doc 110 §2.1 decide que el desbordamiento con signo esta DEFINIDO y envuelve, y
            // §2.5 aplica esa regla a este caso: `MinValue / -1` da `MinValue`, no aborta.
            //
            // Y ADEMAS los dos bajadores no coincidian: este abortaba y el de proyectos --por el
            // que pasan las fixtures-- emitia un `sdiv` desnudo. Ahora los dos llaman a la misma
            // funcion, que esta en src/LLVM/SignedDivisionCorrection.h con el por que completo.
            //
            // LlvmTrapReason::SignedDivisionOverflow se queda en el enum a proposito: quitarlo
            // renumeraria las razones, y los codigos de trampa salen en los diagnosticos.
            LLVMValueRef divisor = correctedSignedDivisor(
                builder_, llvmResultType, left, right, integerWidth(type), "div");
            return instruction->opcode() == mir::MirOpcode::Divide
                ? LLVMBuildSDiv(builder_, left, divisor, name.c_str())
                : LLVMBuildSRem(builder_, left, divisor, name.c_str());
        }

        return instruction->opcode() == mir::MirOpcode::Divide
            ? LLVMBuildUDiv(builder_, left, right, name.c_str())
            : LLVMBuildURem(builder_, left, right, name.c_str());
    }

LLVMValueRef LlvmLowerer::Implementation::lowerBitwise(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef left = operand(instruction, 0, error);
        LLVMValueRef right = operand(instruction, 1, error);
        if (left == nullptr || right == nullptr)
        {
            return nullptr;
        }
        const std::string name = valueName(instruction->result());
        switch (instruction->opcode())
        {
            case mir::MirOpcode::BitAnd: return LLVMBuildAnd(builder_, left, right, name.c_str());
            case mir::MirOpcode::BitOr: return LLVMBuildOr(builder_, left, right, name.c_str());
            case mir::MirOpcode::BitXor: return LLVMBuildXor(builder_, left, right, name.c_str());
            default: error = "internal LLVM bitwise dispatch error"; return nullptr;
        }
    }

LLVMValueRef LlvmLowerer::Implementation::lowerShift(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef left = operand(instruction, 0, error);
        LLVMValueRef count = operand(instruction, 1, error);
        if (left == nullptr || count == nullptr)
        {
            return nullptr;
        }

        const sema::Type* countType = instruction->operands()[1]->type();
        const unsigned resultWidth = integerWidth(instruction->result()->type());
        LLVMTypeRef countLlvmType = llvmType(countType, error);
        if (countLlvmType == nullptr || resultWidth == 0)
        {
            return nullptr;
        }

        LLVMValueRef valid = nullptr;
        LLVMValueRef upper = LLVMConstInt(countLlvmType, resultWidth, 0);
        LLVMValueRef belowUpper = LLVMBuildICmp(builder_, LLVMIntULT, count, upper, "shift.below_width");
        if (isSignedInteger(countType))
        {
            LLVMValueRef zero = LLVMConstInt(countLlvmType, 0, 0);
            LLVMValueRef nonnegative = LLVMBuildICmp(builder_, LLVMIntSGE, count, zero, "shift.nonnegative");
            valid = LLVMBuildAnd(builder_, nonnegative, belowUpper, "shift.valid");
        }
        else
        {
            valid = belowUpper;
        }
        if (!guardOrTrap(valid, LlvmTrapReason::InvalidShiftCount, "shift.range", error))
        {
            return nullptr;
        }

        LLVMValueRef normalized = castInteger(
            count,
            countType,
            instruction->result()->type(),
            false,
            "shift.count",
            error);
        if (normalized == nullptr)
        {
            return nullptr;
        }

        const std::string name = valueName(instruction->result());
        if (instruction->opcode() == mir::MirOpcode::ShiftLeft)
        {
            return LLVMBuildShl(builder_, left, normalized, name.c_str());
        }
        return isSignedInteger(instruction->result()->type())
            ? LLVMBuildAShr(builder_, left, normalized, name.c_str())
            : LLVMBuildLShr(builder_, left, normalized, name.c_str());
    }

LLVMValueRef LlvmLowerer::Implementation::lowerComparison(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef left = operand(instruction, 0, error);
        LLVMValueRef right = operand(instruction, 1, error);
        if (left == nullptr || right == nullptr)
        {
            return nullptr;
        }
        const sema::Type* type = instruction->operands()[0]->type();
        const std::string name = valueName(instruction->result());
        if (isFloating(type))
        {
            return LLVMBuildFCmp(builder_, floatingPredicate(instruction->opcode()), left, right, name.c_str());
        }
        return LLVMBuildICmp(
            builder_,
            integerPredicate(instruction->opcode(), isSignedInteger(type)),
            left,
            right,
            name.c_str());
    }

LLVMValueRef LlvmLowerer::Implementation::lowerConversion(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef source = operand(instruction, 0, error);
        if (source == nullptr)
        {
            return nullptr;
        }
        const sema::Type* sourceType = instruction->operands()[0]->type();
        const sema::Type* destinationType = instruction->result()->type();
        const std::string name = valueName(instruction->result());

        if (sourceType == destinationType)
        {
            return source;
        }
        if (isInteger(sourceType) && isInteger(destinationType))
        {
            return castInteger(source, sourceType, destinationType, isSignedInteger(sourceType), name, error);
        }
        if (isInteger(sourceType) && isFloating(destinationType))
        {
            LLVMTypeRef destination = llvmType(destinationType, error);
            if (destination == nullptr)
            {
                return nullptr;
            }
            return isSignedInteger(sourceType)
                ? LLVMBuildSIToFP(builder_, source, destination, name.c_str())
                : LLVMBuildUIToFP(builder_, source, destination, name.c_str());
        }
        if (isFloating(sourceType) && isInteger(destinationType))
        {
            return lowerFloatingToInteger(source, sourceType, destinationType, name, error);
        }
        if (isFloating(sourceType) && isFloating(destinationType))
        {
            LLVMTypeRef destination = llvmType(destinationType, error);
            if (destination == nullptr)
            {
                return nullptr;
            }
            return sourceType->kind() == sema::TypeKind::Float
                ? LLVMBuildFPExt(builder_, source, destination, name.c_str())
                : LLVMBuildFPTrunc(builder_, source, destination, name.c_str());
        }
        if (isPointerLike(sourceType) && isPointerLike(destinationType))
        {
            LLVMTypeRef destination = llvmType(destinationType, error);
            if (destination == nullptr)
            {
                return nullptr;
            }
            return LLVMBuildPointerCast(builder_, source, destination, name.c_str());
        }
        if (isPointerLike(sourceType) && isInteger(destinationType))
        {
            LLVMTypeRef destination = llvmType(destinationType, error);
            return destination != nullptr ? LLVMBuildPtrToInt(builder_, source, destination, name.c_str()) : nullptr;
        }
        if (isInteger(sourceType) && destinationType->kind() == sema::TypeKind::Pointer)
        {
            LLVMTypeRef destination = llvmType(destinationType, error);
            return destination != nullptr ? LLVMBuildIntToPtr(builder_, source, destination, name.c_str()) : nullptr;
        }

        error = "unsupported LLVM conversion from '" + sourceType->displayName() + "' to '" +
            destinationType->displayName() + "'";
        return nullptr;
    }

LLVMValueRef LlvmLowerer::Implementation::lowerFloatingToInteger(
        LLVMValueRef source,
        const sema::Type* sourceType,
        const sema::Type* destinationType,
        const std::string& name,
        std::string& error)
    {
        LLVMTypeRef sourceLlvmType = llvmType(sourceType, error);
        LLVMTypeRef destinationLlvmType = llvmType(destinationType, error);
        if (sourceLlvmType == nullptr || destinationLlvmType == nullptr)
        {
            return nullptr;
        }

        const unsigned width = integerWidth(destinationType);
        if (width == 0)
        {
            error = "floating-to-integer conversion has non-integer destination";
            return nullptr;
        }

        double lowerBoundary = 0.0;
        double upperBoundary = 0.0;
        if (isSignedInteger(destinationType))
        {
            const long double minimum = -std::ldexp(static_cast<long double>(1.0), static_cast<int>(width - 1));
            const long double minimumMinusOne = minimum - static_cast<long double>(1.0);
            const long double maximumPlusOne = std::ldexp(static_cast<long double>(1.0), static_cast<int>(width - 1));
            lowerBoundary = lowerFloatingBoundary(minimumMinusOne, sourceType);
            upperBoundary = static_cast<double>(maximumPlusOne);
        }
        else
        {
            lowerBoundary = -1.0;
            const long double maximumPlusOne = std::ldexp(static_cast<long double>(1.0), static_cast<int>(width));
            upperBoundary = static_cast<double>(maximumPlusOne);
        }

        LLVMValueRef lower = LLVMConstReal(sourceLlvmType, lowerBoundary);
        LLVMValueRef upper = LLVMConstReal(sourceLlvmType, upperBoundary);
        LLVMValueRef aboveLower = LLVMBuildFCmp(builder_, LLVMRealOGT, source, lower, "fptoi.above_lower");
        LLVMValueRef belowUpper = LLVMBuildFCmp(builder_, LLVMRealOLT, source, upper, "fptoi.below_upper");
        LLVMValueRef valid = LLVMBuildAnd(builder_, aboveLower, belowUpper, "fptoi.valid");
        if (!guardOrTrap(valid, LlvmTrapReason::FloatingToIntegerOutOfRange, "fptoi.range", error))
        {
            return nullptr;
        }

        return isSignedInteger(destinationType)
            ? LLVMBuildFPToSI(builder_, source, destinationLlvmType, name.c_str())
            : LLVMBuildFPToUI(builder_, source, destinationLlvmType, name.c_str());
    }

LLVMValueRef LlvmLowerer::Implementation::castInteger(
        LLVMValueRef value,
        const sema::Type* sourceType,
        const sema::Type* destinationType,
        bool signExtend,
        const std::string& name,
        std::string& error)
    {
        const unsigned sourceWidth = integerWidth(sourceType);
        const unsigned destinationWidth = integerWidth(destinationType);
        LLVMTypeRef destination = llvmType(destinationType, error);
        if (destination == nullptr || sourceWidth == 0 || destinationWidth == 0)
        {
            return nullptr;
        }
        if (sourceWidth == destinationWidth)
        {
            return value;
        }
        if (sourceWidth > destinationWidth)
        {
            return LLVMBuildTrunc(builder_, value, destination, name.c_str());
        }
        return signExtend
            ? LLVMBuildSExt(builder_, value, destination, name.c_str())
            : LLVMBuildZExt(builder_, value, destination, name.c_str());
    }

LLVMValueRef LlvmLowerer::Implementation::lowerPointerArithmetic(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef pointer = operand(instruction, 0, error);
        if (pointer == nullptr)
        {
            return nullptr;
        }
        sema::Type* pointerType = instruction->result()->type();
        LLVMTypeRef elementType = llvmType(pointerType->elementType(), error);
        if (elementType == nullptr)
        {
            return nullptr;
        }

        LLVMValueRef index = nullptr;
        if (instruction->hasImmediate())
        {
            const std::int64_t step = instruction->opcode() == mir::MirOpcode::PointerSubtract
                ? -static_cast<std::int64_t>(instruction->immediate())
                : static_cast<std::int64_t>(instruction->immediate());
            index = LLVMConstInt(LLVMInt64TypeInContext(context_), static_cast<std::uint64_t>(step), 1);
        }
        else
        {
            index = operand(instruction, 1, error);
            if (index == nullptr)
            {
                return nullptr;
            }
            if (instruction->opcode() == mir::MirOpcode::PointerSubtract)
            {
                index = LLVMBuildNeg(builder_, index, "ptr.neg_index");
            }
        }

        LLVMValueRef indices[] = {index};
        return LLVMBuildGEP2(
            builder_, elementType, pointer, indices, 1, valueName(instruction->result()).c_str());
    }

LLVMValueRef LlvmLowerer::Implementation::lowerPointerDifference(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef left = operand(instruction, 0, error);
        LLVMValueRef right = operand(instruction, 1, error);
        if (left == nullptr || right == nullptr)
        {
            return nullptr;
        }
        sema::Type* element = instruction->operands()[0]->type()->elementType();
        LLVMTypeRef elementType = llvmType(element, error);
        if (elementType == nullptr)
        {
            return nullptr;
        }
        return LLVMBuildPtrDiff2(
            builder_, elementType, left, right, valueName(instruction->result()).c_str());
    }

LLVMValueRef LlvmLowerer::Implementation::lowerCall(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        auto function = functionValues_.find(instruction->symbol());
        auto type = functionTypes_.find(instruction->symbol());
        if (function == functionValues_.end() || type == functionTypes_.end())
        {
            failInstruction(instruction, "call references undeclared function", error);
            return nullptr;
        }

        std::vector<LLVMValueRef> arguments;
        arguments.reserve(instruction->operands().size());
        for (std::size_t index = 0; index < instruction->operands().size(); ++index)
        {
            LLVMValueRef argument = operand(instruction, index, error);
            if (argument == nullptr)
            {
                return nullptr;
            }
            arguments.push_back(argument);
        }
        const std::string callName = instruction->result() != nullptr
            ? valueName(instruction->result())
            : std::string();
        return LLVMBuildCall2(
            builder_,
            type->second,
            function->second,
            arguments.empty() ? nullptr : arguments.data(),
            static_cast<unsigned>(arguments.size()),
            callName.c_str());
    }

bool LlvmLowerer::Implementation::lowerBoundsCheck(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef index = operand(instruction, 0, error);
        const sema::Type* indexType = instruction->operands()[0]->type();
        LLVMTypeRef llvmIndexType = llvmType(indexType, error);
        if (index == nullptr || llvmIndexType == nullptr)
        {
            return false;
        }

        LLVMValueRef upper = LLVMConstInt(llvmIndexType, instruction->immediate(), 0);
        LLVMValueRef below = LLVMBuildICmp(builder_, LLVMIntULT, index, upper, "bounds.below_upper");
        LLVMValueRef valid = below;
        if (isSignedInteger(indexType))
        {
            LLVMValueRef zero = LLVMConstInt(llvmIndexType, 0, 0);
            LLVMValueRef nonnegative = LLVMBuildICmp(builder_, LLVMIntSGE, index, zero, "bounds.nonnegative");
            valid = LLVMBuildAnd(builder_, nonnegative, below, "bounds.valid");
        }
        return guardOrTrap(valid, LlvmTrapReason::BoundsViolation, "bounds", error);
    }

LLVMValueRef LlvmLowerer::Implementation::lowerFieldAddress(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef base = operand(instruction, 0, error);
        auto field = fieldInfo_.find(instruction->symbol());
        if (base == nullptr || field == fieldInfo_.end())
        {
            if (error.empty())
            {
                failInstruction(instruction, "field.address has no semantic struct-field mapping", error);
            }
            return nullptr;
        }
        return LLVMBuildStructGEP2(
            builder_,
            field->second.structType,
            base,
            field->second.index,
            valueName(instruction->result()).c_str());
    }

LLVMValueRef LlvmLowerer::Implementation::lowerIndexAddress(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef base = operand(instruction, 0, error);
        LLVMValueRef index = operand(instruction, 1, error);
        if (base == nullptr || index == nullptr || instruction->auxiliaryType() == nullptr)
        {
            return nullptr;
        }

        const sema::Type* baseType = instruction->auxiliaryType();
        if (baseType->kind() == sema::TypeKind::Array)
        {
            LLVMTypeRef arrayType = llvmType(baseType, error);
            if (arrayType == nullptr)
            {
                return nullptr;
            }
            LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
            LLVMValueRef indices[] = {zero, index};
            return LLVMBuildGEP2(
                builder_, arrayType, base, indices, 2, valueName(instruction->result()).c_str());
        }

        const sema::Type* elementType = nullptr;
        if (baseType->kind() == sema::TypeKind::Pointer)
        {
            elementType = baseType->elementType();
        }
        else if (baseType->kind() == sema::TypeKind::StringLiteral)
        {
            elementType = baseType->elementType();
        }
        if (elementType == nullptr)
        {
            failInstruction(instruction, "index.address has unsupported base type", error);
            return nullptr;
        }
        LLVMTypeRef llvmElement = llvmType(elementType, error);
        if (llvmElement == nullptr)
        {
            return nullptr;
        }
        LLVMValueRef indices[] = {index};
        return LLVMBuildGEP2(
            builder_, llvmElement, base, indices, 1, valueName(instruction->result()).c_str());
    }

LLVMValueRef LlvmLowerer::Implementation::lowerExtractField(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef aggregate = operand(instruction, 0, error);
        auto field = fieldInfo_.find(instruction->symbol());
        if (aggregate == nullptr || field == fieldInfo_.end())
        {
            if (error.empty())
            {
                failInstruction(instruction, "extract.field has no semantic field mapping", error);
            }
            return nullptr;
        }
        return LLVMBuildExtractValue(
            builder_, aggregate, field->second.index, valueName(instruction->result()).c_str());
    }

LLVMValueRef LlvmLowerer::Implementation::lowerCountedAllocation(
    const mir::MirInstruction* instruction,
    std::string& error)
    {
        LLVMValueRef count = operand(instruction, 0, error);
        const sema::Type* countType = instruction->operands()[0]->type();
        if (count == nullptr)
        {
            return nullptr;
        }

        const unsigned countWidth = integerWidth(countType);
        if (countWidth == 0)
        {
            error = "allocation count has non-integer type";
            return nullptr;
        }
        LLVMValueRef count64 = count;
        if (countWidth < 64)
        {
            count64 = LLVMBuildZExt(
                builder_, count, LLVMInt64TypeInContext(context_), "alloc.count64");
        }

        LLVMValueRef valid = nullptr;
        if (isSignedInteger(countType))
        {
            LLVMTypeRef sourceType = llvmType(countType, error);
            if (sourceType == nullptr)
            {
                return nullptr;
            }
            LLVMValueRef zero = LLVMConstInt(sourceType, 0, 0);
            valid = LLVMBuildICmp(builder_, LLVMIntSGE, count, zero, "alloc.nonnegative");
        }
        else
        {
            valid = LLVMConstInt(LLVMInt1TypeInContext(context_), 1, 0);
        }

        const std::optional<target::TypeLayout> layout = layoutOf(instruction->auxiliaryType());
        if (!layout.has_value() || layout->size == 0)
        {
            error = "cannot determine allocation size for type '" + instruction->auxiliaryType()->displayName() + "'";
            return nullptr;
        }

        const std::uint64_t maximumCount = std::numeric_limits<std::uint64_t>::max() / layout->size;
        LLVMValueRef maximum = LLVMConstInt(LLVMInt64TypeInContext(context_), maximumCount, 0);
        LLVMValueRef fits = LLVMBuildICmp(builder_, LLVMIntULE, count64, maximum, "alloc.size_fits");
        valid = LLVMBuildAnd(builder_, valid, fits, "alloc.valid");
        if (!guardOrTrap(valid, LlvmTrapReason::AllocationSizeOverflow, "alloc.size", error))
        {
            return nullptr;
        }

        LLVMTypeRef elementType = llvmType(instruction->auxiliaryType(), error);
        if (elementType == nullptr)
        {
            return nullptr;
        }
        if (instruction->opcode() == mir::MirOpcode::AllocateArray)
        {
            LLVMValueRef elementSize = LLVMConstInt(LLVMInt64TypeInContext(context_), layout->size, 0);
            LLVMValueRef byteCount = LLVMBuildMul(builder_, count64, elementSize, "alloc.bytes");
            LLVMValueRef arguments[] = {byteCount};
            return LLVMBuildCall2(
                builder_,
                allocationFunctionType_,
                allocationFunction_,
                arguments,
                1,
                valueName(instruction->result()).c_str());
        }
        return LLVMBuildArrayAlloca(
            builder_, elementType, count64, valueName(instruction->result()).c_str());
    }

bool LlvmLowerer::Implementation::guardOrTrap(
        LLVMValueRef validCondition,
        LlvmTrapReason reason,
        const std::string& stem,
        std::string& error)
    {
        if (validCondition == nullptr || currentFunction_ == nullptr || trapFunction_ == nullptr)
        {
            error = "cannot create NativeSharp trap guard without condition/function/trap hook";
            return false;
        }

        const std::string suffix = std::to_string(trapSequence_++);
        LLVMBasicBlockRef okBlock = LLVMAppendBasicBlockInContext(
            context_, currentFunction_, ("nsc." + stem + ".ok." + suffix).c_str());
        LLVMBasicBlockRef trapBlock = LLVMAppendBasicBlockInContext(
            context_, currentFunction_, ("nsc." + stem + ".trap." + suffix).c_str());
        if (okBlock == nullptr || trapBlock == nullptr)
        {
            error = "LLVM failed to create NativeSharp trap-control-flow blocks";
            return false;
        }

        LLVMBuildCondBr(builder_, validCondition, okBlock, trapBlock);
        LLVMPositionBuilderAtEnd(builder_, trapBlock);
        LLVMValueRef code = LLVMConstInt(
            LLVMInt32TypeInContext(context_), static_cast<std::uint32_t>(reason), 0);
        LLVMValueRef arguments[] = {code};
        LLVMBuildCall2(builder_, trapFunctionType_, trapFunction_, arguments, 1, "");
        LLVMBuildUnreachable(builder_);
        LLVMPositionBuilderAtEnd(builder_, okBlock);
        return true;
    }

LLVMValueRef LlvmLowerer::Implementation::operand(
    const mir::MirInstruction* instruction,
    std::size_t index,
    std::string& error)
    {
        if (instruction == nullptr || index >= instruction->operands().size())
        {
            error = "LLVM lowering requested a missing MIR operand";
            return nullptr;
        }
        auto value = valueMap_.find(instruction->operands()[index]);
        if (value == valueMap_.end())
        {
            error = "MIR operand %v" + std::to_string(instruction->operands()[index]->id()) +
                " has no previously lowered LLVM value";
            return nullptr;
        }
        return value->second;
    }

LLVMValueRef LlvmLowerer::Implementation::symbolStorage(const sema::Symbol* symbol) const noexcept
    {
        auto local = localStorage_.find(symbol);
        if (local != localStorage_.end())
        {
            return local->second;
        }
        auto global = globalStorage_.find(symbol);
        return global != globalStorage_.end() ? global->second : nullptr;
    }

bool LlvmLowerer::Implementation::failInstruction(
        const mir::MirInstruction* instruction,
        const std::string& message,
        std::string& error) const
    {
        std::ostringstream stream;
        stream << "LLVM lowering of MIR opcode '" << mir::mirOpcodeName(instruction->opcode()) << "' failed: "
               << message;
        error = stream.str();
        return false;
    }

}
