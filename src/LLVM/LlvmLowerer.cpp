#include "nativesharp/LLVM/LlvmLowerer.h"

#include "LlvmCApi.h"

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

namespace
{
constexpr const char* kTargetTriple = "x86_64-pc-windows-msvc";
constexpr const char* kTrapFunctionName = "__nativesharp_trap";
constexpr const char* kAllocationFunctionName = "__nativesharp_alloc";
constexpr const char* kDeallocationFunctionName = "__nativesharp_free";

bool isInteger(const sema::Type* type) noexcept
{
    if (type == nullptr)
    {
        return false;
    }

    switch (type->kind())
    {
        case sema::TypeKind::Byte:
        case sema::TypeKind::Char:
        case sema::TypeKind::Short:
        case sema::TypeKind::UShort:
        case sema::TypeKind::Int:
        case sema::TypeKind::UInt:
        case sema::TypeKind::Long:
        case sema::TypeKind::ULong:
        case sema::TypeKind::NInt:
        case sema::TypeKind::NUInt:
        case sema::TypeKind::Enum:
            return true;

        default:
            return false;
    }
}

bool isSignedInteger(const sema::Type* type) noexcept
{
    if (type == nullptr)
    {
        return false;
    }

    switch (type->kind())
    {
        case sema::TypeKind::Short:
        case sema::TypeKind::Int:
        case sema::TypeKind::Long:
        case sema::TypeKind::NInt:
        case sema::TypeKind::Enum:
            return true;

        default:
            return false;
    }
}

bool isFloating(const sema::Type* type) noexcept
{
    return type != nullptr &&
        (type->kind() == sema::TypeKind::Float || type->kind() == sema::TypeKind::Double);
}

bool isPointerLike(const sema::Type* type) noexcept
{
    return type != nullptr &&
        (type->kind() == sema::TypeKind::Pointer || type->kind() == sema::TypeKind::NullLiteral ||
         type->kind() == sema::TypeKind::StringLiteral);
}

unsigned integerWidth(const sema::Type* type) noexcept
{
    if (type == nullptr)
    {
        return 0;
    }

    switch (type->kind())
    {
        case sema::TypeKind::Byte:
        case sema::TypeKind::Char:
            return 8;

        case sema::TypeKind::Short:
        case sema::TypeKind::UShort:
            return 16;

        case sema::TypeKind::Int:
        case sema::TypeKind::UInt:
        case sema::TypeKind::Enum:
            return 32;

        case sema::TypeKind::Long:
        case sema::TypeKind::ULong:
        case sema::TypeKind::NInt:
        case sema::TypeKind::NUInt:
            return 64;

        default:
            return 0;
    }
}

double lowerFloatingBoundary(long double mathematicalBoundary, const sema::Type* sourceType) noexcept
{
    if (sourceType != nullptr && sourceType->kind() == sema::TypeKind::Float)
    {
        float quantized = static_cast<float>(mathematicalBoundary);
        if (static_cast<long double>(quantized) > mathematicalBoundary)
        {
            quantized = std::nextafter(quantized, -std::numeric_limits<float>::infinity());
        }
        return static_cast<double>(quantized);
    }

    double quantized = static_cast<double>(mathematicalBoundary);
    if (static_cast<long double>(quantized) > mathematicalBoundary)
    {
        quantized = std::nextafter(quantized, -std::numeric_limits<double>::infinity());
    }
    return quantized;
}

std::string valueName(const mir::MirValue* value)
{
    if (value == nullptr)
    {
        return "tmp";
    }
    return "v" + std::to_string(value->id());
}

std::string blockName(const mir::MirBlock* block)
{
    if (block == nullptr)
    {
        return "invalid";
    }
    return "bb" + std::to_string(block->id()) + "." + block->name();
}

LLVMIntPredicate integerPredicate(mir::MirOpcode opcode, bool signedOperands)
{
    switch (opcode)
    {
        case mir::MirOpcode::CompareEqual: return LLVMIntEQ;
        case mir::MirOpcode::CompareNotEqual: return LLVMIntNE;
        case mir::MirOpcode::CompareLess: return signedOperands ? LLVMIntSLT : LLVMIntULT;
        case mir::MirOpcode::CompareLessEqual: return signedOperands ? LLVMIntSLE : LLVMIntULE;
        case mir::MirOpcode::CompareGreater: return signedOperands ? LLVMIntSGT : LLVMIntUGT;
        case mir::MirOpcode::CompareGreaterEqual: return signedOperands ? LLVMIntSGE : LLVMIntUGE;
        default: return LLVMIntEQ;
    }
}

LLVMRealPredicate floatingPredicate(mir::MirOpcode opcode)
{
    switch (opcode)
    {
        case mir::MirOpcode::CompareEqual: return LLVMRealOEQ;
        case mir::MirOpcode::CompareNotEqual: return LLVMRealUNE;
        case mir::MirOpcode::CompareLess: return LLVMRealOLT;
        case mir::MirOpcode::CompareLessEqual: return LLVMRealOLE;
        case mir::MirOpcode::CompareGreater: return LLVMRealOGT;
        case mir::MirOpcode::CompareGreaterEqual: return LLVMRealOGE;
        default: return LLVMRealOEQ;
    }
}
}

class LlvmLowerer::Implementation final
{
public:
    explicit Implementation(std::span<sema::Symbol* const> globalSymbols)
        : globalSymbols_(globalSymbols.begin(), globalSymbols.end()),
          targetLayout_(target::TargetDataLayout::WindowsX64())
    {
    }

    std::unique_ptr<LlvmModule> lower(const passes::OptimizationModule* optimizedModule, std::string& error)
    {
        reset();
        error.clear();

        if (optimizedModule == nullptr || optimizedModule->module() == nullptr)
        {
            error = "LLVM lowering requires a non-null optimized MIR/SSA module";
            return nullptr;
        }

        passes::OptimizationVerifier optimizationVerifier;
        if (!optimizationVerifier.verify(optimizedModule))
        {
            error = "optimized SSA precondition failed before LLVM lowering: " + optimizationVerifier.error();
            return nullptr;
        }

        sourceModule_ = optimizedModule->module();
        if (!initializeModule(error) || !declareStructs(error) || !defineStructBodies(error) ||
            !declareGlobals(error) || !declareFunctions(error) || !declareRuntimeFunctions(error) ||
            !lowerGlobals(error) || !lowerFunctions(error))
        {
            disposeState();
            return nullptr;
        }

        LLVMDisposeBuilder(builder_);
        builder_ = nullptr;

        auto result = std::unique_ptr<LlvmModule>(new LlvmModule(context_, module_));
        context_ = nullptr;
        module_ = nullptr;

        std::string verificationError;
        if (!result->verify(verificationError))
        {
            error = "LLVM verifier rejected lowered module: " + verificationError;
            return nullptr;
        }
        return result;
    }

private:
    struct FieldInfo
    {
        LLVMTypeRef structType = nullptr;
        unsigned index = 0;
    };

    struct DeferredPhi
    {
        const mir::MirInstruction* instruction = nullptr;
        LLVMValueRef llvmPhi = nullptr;
    };

    void reset()
    {
        disposeState();
        sourceModule_ = nullptr;
        currentFunction_ = nullptr;
        currentMirFunction_ = nullptr;
        trapFunction_ = nullptr;
        trapFunctionType_ = nullptr;
        allocationFunction_ = nullptr;
        allocationFunctionType_ = nullptr;
        deallocationFunction_ = nullptr;
        deallocationFunctionType_ = nullptr;
        typeMap_.clear();
        structSymbols_.clear();
        fieldInfo_.clear();
        globalStorage_.clear();
        functionValues_.clear();
        functionTypes_.clear();
        valueMap_.clear();
        blockEntry_.clear();
        blockExit_.clear();
        localStorage_.clear();
        deferredPhis_.clear();
        trapSequence_ = 0;
        stringSequence_ = 0;
    }

    void disposeState() noexcept
    {
        if (builder_ != nullptr)
        {
            LLVMDisposeBuilder(builder_);
            builder_ = nullptr;
        }
        if (module_ != nullptr)
        {
            LLVMDisposeModule(module_);
            module_ = nullptr;
        }
        if (context_ != nullptr)
        {
            LLVMContextDispose(context_);
            context_ = nullptr;
        }
    }

    bool initializeModule(std::string& error)
    {
        context_ = LLVMContextCreate();
        if (context_ == nullptr)
        {
            error = "LLVMContextCreate returned null";
            return false;
        }

        module_ = LLVMModuleCreateWithNameInContext("nativesharp", context_);
        if (module_ == nullptr)
        {
            error = "LLVMModuleCreateWithNameInContext returned null";
            return false;
        }

        LLVMSetTarget(module_, kTargetTriple);
        builder_ = LLVMCreateBuilderInContext(context_);
        if (builder_ == nullptr)
        {
            error = "LLVMCreateBuilderInContext returned null";
            return false;
        }
        return true;
    }

    bool declareStructs(std::string& error)
    {
        for (sema::Symbol* symbol : globalSymbols_)
        {
            if (symbol == nullptr || symbol->kind() != sema::SymbolKind::Struct)
            {
                continue;
            }
            if (symbol->type() == nullptr || symbol->type()->kind() != sema::TypeKind::Struct)
            {
                error = "semantic struct symbol '" + symbol->name() + "' has no struct type";
                return false;
            }

            LLVMTypeRef structType = LLVMStructCreateNamed(context_, symbol->name().c_str());
            if (structType == nullptr)
            {
                error = "LLVM failed to create struct type '" + symbol->name() + "'";
                return false;
            }
            typeMap_[symbol->type()] = structType;
            structSymbols_[symbol->type()] = symbol;
        }
        return true;
    }

    bool defineStructBodies(std::string& error)
    {
        for (const auto& [type, symbol] : structSymbols_)
        {
            LLVMTypeRef structType = typeMap_.at(type);
            std::vector<LLVMTypeRef> fields;
            fields.reserve(symbol->members().size());

            for (std::size_t index = 0; index < symbol->members().size(); ++index)
            {
                sema::Symbol* field = symbol->members()[index];
                if (field == nullptr || field->kind() != sema::SymbolKind::Field || field->type() == nullptr)
                {
                    error = "struct '" + symbol->name() + "' contains an invalid semantic field";
                    return false;
                }
                LLVMTypeRef fieldType = llvmType(field->type(), error);
                if (fieldType == nullptr)
                {
                    return false;
                }
                fields.push_back(fieldType);
                fieldInfo_[field] = FieldInfo{structType, static_cast<unsigned>(index)};
            }
            LLVMStructSetBody(
                structType,
                fields.empty() ? nullptr : fields.data(),
                static_cast<unsigned>(fields.size()),
                0);
        }
        return true;
    }

    bool declareGlobals(std::string& error)
    {
        for (const mir::MirGlobal* global : sourceModule_->globals())
        {
            if (global == nullptr || global->symbol() == nullptr)
            {
                error = "optimized module contains a null global";
                return false;
            }
            LLVMTypeRef type = llvmType(global->symbol()->type(), error);
            if (type == nullptr)
            {
                return false;
            }
            LLVMValueRef value = LLVMAddGlobal(module_, type, global->symbol()->name().c_str());
            if (value == nullptr)
            {
                error = "LLVM failed to declare global '" + global->symbol()->name() + "'";
                return false;
            }
            globalStorage_[global->symbol()] = value;
        }
        return true;
    }

    bool declareFunctions(std::string& error)
    {
        for (const mir::MirFunction* function : sourceModule_->functions())
        {
            if (function == nullptr || function->symbol() == nullptr)
            {
                error = "optimized module contains a null function";
                return false;
            }
            LLVMTypeRef type = llvmFunctionType(function->symbol()->type(), error);
            if (type == nullptr)
            {
                return false;
            }
            LLVMValueRef value = LLVMAddFunction(module_, function->symbol()->name().c_str(), type);
            if (value == nullptr)
            {
                error = "LLVM failed to declare function '" + function->symbol()->name() + "'";
                return false;
            }
            functionValues_[function->symbol()] = value;
            functionTypes_[function->symbol()] = type;
        }
        return true;
    }

    bool declareRuntimeFunctions(std::string& error)
    {
        for (sema::Symbol* symbol : globalSymbols_)
        {
            if (symbol == nullptr || symbol->kind() != sema::SymbolKind::Function)
            {
                continue;
            }
            if (symbol->name() == kTrapFunctionName || symbol->name() == kAllocationFunctionName ||
                symbol->name() == kDeallocationFunctionName)
            {
                error = "reserved backend function name is already declared: " + symbol->name();
                return false;
            }
        }

        LLVMTypeRef i32 = LLVMInt32TypeInContext(context_);
        LLVMTypeRef i64 = LLVMInt64TypeInContext(context_);
        LLVMTypeRef pointer = LLVMPointerTypeInContext(context_, 0);
        LLVMTypeRef voidType = LLVMVoidTypeInContext(context_);

        LLVMTypeRef trapParameters[] = {i32};
        trapFunctionType_ = LLVMFunctionType(voidType, trapParameters, 1, 0);
        trapFunction_ = LLVMAddFunction(module_, kTrapFunctionName, trapFunctionType_);

        LLVMTypeRef allocationParameters[] = {i64};
        allocationFunctionType_ = LLVMFunctionType(pointer, allocationParameters, 1, 0);
        allocationFunction_ = LLVMAddFunction(module_, kAllocationFunctionName, allocationFunctionType_);

        LLVMTypeRef deallocationParameters[] = {pointer};
        deallocationFunctionType_ = LLVMFunctionType(voidType, deallocationParameters, 1, 0);
        deallocationFunction_ = LLVMAddFunction(module_, kDeallocationFunctionName, deallocationFunctionType_);

        if (trapFunction_ == nullptr || allocationFunction_ == nullptr || deallocationFunction_ == nullptr)
        {
            error = "LLVM failed to declare NativeSharp runtime hooks";
            return false;
        }
        return true;
    }

    bool lowerGlobals(std::string& error)
    {
        for (const mir::MirGlobal* global : sourceModule_->globals())
        {
            LLVMValueRef storage = globalStorage_.at(global->symbol());
            LLVMValueRef initializer = nullptr;
            if (global->hasExplicitInitializer())
            {
                initializer = llvmConstant(global->initializer(), global->symbol()->type(), error, false);
            }
            else
            {
                LLVMTypeRef type = llvmType(global->symbol()->type(), error);
                if (type != nullptr)
                {
                    initializer = LLVMConstNull(type);
                }
            }
            if (initializer == nullptr)
            {
                if (error.empty())
                {
                    error = "failed to lower initializer for global '" + global->symbol()->name() + "'";
                }
                return false;
            }
            LLVMSetInitializer(storage, initializer);
        }
        return true;
    }

    bool lowerFunctions(std::string& error)
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

    bool lowerFunction(const mir::MirFunction* function, std::string& error)
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

    bool lowerInstruction(const mir::MirInstruction* instruction, std::string& error)
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
        }

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

    LLVMValueRef lowerBasicArithmetic(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerDivision(const mir::MirInstruction* instruction, std::string& error)
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
            const unsigned width = integerWidth(type);
            const std::uint64_t minimumBits = width == 64
                ? (std::uint64_t{1} << 63)
                : (std::uint64_t{1} << (width - 1));
            const std::uint64_t minusOneBits = width == 64
                ? std::numeric_limits<std::uint64_t>::max()
                : ((std::uint64_t{1} << width) - 1);
            LLVMValueRef minimum = LLVMConstInt(llvmResultType, minimumBits, 0);
            LLVMValueRef minusOne = LLVMConstInt(llvmResultType, minusOneBits, 0);
            LLVMValueRef isMinimum = LLVMBuildICmp(builder_, LLVMIntEQ, left, minimum, "div.is_min");
            LLVMValueRef isMinusOne = LLVMBuildICmp(builder_, LLVMIntEQ, right, minusOne, "div.is_minus_one");
            LLVMValueRef overflow = LLVMBuildAnd(builder_, isMinimum, isMinusOne, "div.overflow");
            LLVMValueRef noOverflow = LLVMBuildNot(builder_, overflow, "div.no_overflow");
            if (!guardOrTrap(noOverflow, LlvmTrapReason::SignedDivisionOverflow, "div.overflow", error))
            {
                return nullptr;
            }
            return instruction->opcode() == mir::MirOpcode::Divide
                ? LLVMBuildSDiv(builder_, left, right, name.c_str())
                : LLVMBuildSRem(builder_, left, right, name.c_str());
        }

        return instruction->opcode() == mir::MirOpcode::Divide
            ? LLVMBuildUDiv(builder_, left, right, name.c_str())
            : LLVMBuildURem(builder_, left, right, name.c_str());
    }

    LLVMValueRef lowerBitwise(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerShift(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerComparison(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerConversion(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerFloatingToInteger(
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

    LLVMValueRef castInteger(
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

    LLVMValueRef lowerPointerArithmetic(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerPointerDifference(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerCall(const mir::MirInstruction* instruction, std::string& error)
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

    bool lowerBoundsCheck(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerFieldAddress(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerIndexAddress(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerExtractField(const mir::MirInstruction* instruction, std::string& error)
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

    LLVMValueRef lowerCountedAllocation(const mir::MirInstruction* instruction, std::string& error)
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

    bool guardOrTrap(
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

    bool completePhi(const DeferredPhi& deferred, std::string& error)
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

    LLVMTypeRef llvmType(const sema::Type* type, std::string& error)
    {
        if (type == nullptr)
        {
            error = "cannot lower null semantic type to LLVM";
            return nullptr;
        }
        auto existing = typeMap_.find(type);
        if (existing != typeMap_.end())
        {
            return existing->second;
        }

        LLVMTypeRef result = nullptr;
        switch (type->kind())
        {
            case sema::TypeKind::Void: result = LLVMVoidTypeInContext(context_); break;
            case sema::TypeKind::Bool: result = LLVMInt1TypeInContext(context_); break;
            case sema::TypeKind::Byte:
            case sema::TypeKind::Char: result = LLVMInt8TypeInContext(context_); break;
            case sema::TypeKind::Short:
            case sema::TypeKind::UShort: result = LLVMInt16TypeInContext(context_); break;
            case sema::TypeKind::Int:
            case sema::TypeKind::UInt:
            case sema::TypeKind::Enum: result = LLVMInt32TypeInContext(context_); break;
            case sema::TypeKind::Long:
            case sema::TypeKind::ULong:
            case sema::TypeKind::NInt:
            case sema::TypeKind::NUInt: result = LLVMInt64TypeInContext(context_); break;
            case sema::TypeKind::Float: result = LLVMFloatTypeInContext(context_); break;
            case sema::TypeKind::Double: result = LLVMDoubleTypeInContext(context_); break;
            case sema::TypeKind::Pointer:
            case sema::TypeKind::NullLiteral:
            case sema::TypeKind::StringLiteral:
                result = LLVMPointerTypeInContext(context_, 0);
                break;

            case sema::TypeKind::Array:
            {
                LLVMTypeRef element = llvmType(type->elementType(), error);
                if (element != nullptr)
                {
                    result = LLVMArrayType2(element, type->count());
                }
                break;
            }

            case sema::TypeKind::Struct:
                error = "struct type '" + type->displayName() + "' is missing semantic struct-symbol metadata";
                return nullptr;

            case sema::TypeKind::Function:
                result = llvmFunctionType(type, error);
                break;

            case sema::TypeKind::Error:
                error = "error semantic type reached LLVM lowering";
                return nullptr;
        }

        if (result == nullptr)
        {
            if (error.empty())
            {
                error = "LLVM failed to lower semantic type '" + type->displayName() + "'";
            }
            return nullptr;
        }
        typeMap_[type] = result;
        return result;
    }

    LLVMTypeRef llvmFunctionType(const sema::Type* type, std::string& error)
    {
        if (type == nullptr || type->kind() != sema::TypeKind::Function || type->returnType() == nullptr)
        {
            error = "function symbol has invalid semantic function type";
            return nullptr;
        }
        auto existing = typeMap_.find(type);
        if (existing != typeMap_.end())
        {
            return existing->second;
        }

        LLVMTypeRef returnType = llvmType(type->returnType(), error);
        if (returnType == nullptr)
        {
            return nullptr;
        }
        std::vector<LLVMTypeRef> parameters;
        parameters.reserve(type->parameterTypes().size());
        for (sema::Type* parameter : type->parameterTypes())
        {
            LLVMTypeRef lowered = llvmType(parameter, error);
            if (lowered == nullptr)
            {
                return nullptr;
            }
            parameters.push_back(lowered);
        }
        LLVMTypeRef result = LLVMFunctionType(
            returnType,
            parameters.empty() ? nullptr : parameters.data(),
            static_cast<unsigned>(parameters.size()),
            0);
        if (result == nullptr)
        {
            error = "LLVM failed to create function type '" + type->displayName() + "'";
            return nullptr;
        }
        typeMap_[type] = result;
        return result;
    }

    LLVMValueRef llvmConstant(
        const sema::ConstantValue& constant,
        const sema::Type* type,
        std::string& error,
        bool allowStringInstruction)
    {
        LLVMTypeRef llvmConstantType = llvmType(type, error);
        if (llvmConstantType == nullptr)
        {
            return nullptr;
        }

        switch (constant.kind())
        {
            case sema::ConstantKind::SignedInteger:
                return LLVMConstInt(
                    llvmConstantType,
                    static_cast<std::uint64_t>(constant.signedInteger()),
                    1);

            case sema::ConstantKind::UnsignedInteger:
                return LLVMConstInt(llvmConstantType, constant.unsignedInteger(), 0);

            case sema::ConstantKind::Floating:
                return LLVMConstReal(llvmConstantType, constant.floating());

            case sema::ConstantKind::Boolean:
                return LLVMConstInt(llvmConstantType, constant.boolean() ? 1 : 0, 0);

            case sema::ConstantKind::Null:
                return LLVMConstNull(llvmConstantType);

            case sema::ConstantKind::String:
            {
                if (!allowStringInstruction)
                {
                    error = "string literal is not supported as a global constant initializer";
                    return nullptr;
                }
                LLVMTypeRef characterType = LLVMInt8TypeInContext(context_);
                LLVMTypeRef arrayType = LLVMArrayType2(characterType, constant.string().size() + 1U);
                LLVMValueRef bytes = LLVMConstStringInContext2(
                    context_, constant.string().data(), constant.string().size(), 0);
                const std::string globalName = "nsc.str." + std::to_string(stringSequence_++);
                LLVMValueRef global = LLVMAddGlobal(module_, arrayType, globalName.c_str());
                if (bytes == nullptr || global == nullptr)
                {
                    error = "LLVM failed to materialize string literal storage";
                    return nullptr;
                }
                LLVMSetInitializer(global, bytes);
                LLVMSetGlobalConstant(global, 1);
                LLVMValueRef zero = LLVMConstInt(LLVMInt64TypeInContext(context_), 0, 0);
                LLVMValueRef indices[] = {zero, zero};
                return LLVMConstInBoundsGEP2(arrayType, global, indices, 2);
            }

            case sema::ConstantKind::None:
                error = "constant instruction has ConstantKind::None";
                return nullptr;
        }
        error = "unknown semantic constant kind reached LLVM lowering";
        return nullptr;
    }

    std::optional<target::TypeLayout> layoutOf(const sema::Type* type) const
    {
        if (type == nullptr)
        {
            return std::nullopt;
        }

        switch (type->kind())
        {
            case sema::TypeKind::Bool: return targetLayout_.layoutOf(target::BuiltinLayoutKind::Bool);
            case sema::TypeKind::Byte: return targetLayout_.layoutOf(target::BuiltinLayoutKind::Byte);
            case sema::TypeKind::Char: return targetLayout_.layoutOf(target::BuiltinLayoutKind::Char);
            case sema::TypeKind::Short: return targetLayout_.layoutOf(target::BuiltinLayoutKind::Short);
            case sema::TypeKind::UShort: return targetLayout_.layoutOf(target::BuiltinLayoutKind::UShort);
            case sema::TypeKind::Int: return targetLayout_.layoutOf(target::BuiltinLayoutKind::Int);
            case sema::TypeKind::UInt: return targetLayout_.layoutOf(target::BuiltinLayoutKind::UInt);
            case sema::TypeKind::Long: return targetLayout_.layoutOf(target::BuiltinLayoutKind::Long);
            case sema::TypeKind::ULong: return targetLayout_.layoutOf(target::BuiltinLayoutKind::ULong);
            case sema::TypeKind::NInt: return targetLayout_.layoutOf(target::BuiltinLayoutKind::NInt);
            case sema::TypeKind::NUInt: return targetLayout_.layoutOf(target::BuiltinLayoutKind::NUInt);
            case sema::TypeKind::Float: return targetLayout_.layoutOf(target::BuiltinLayoutKind::Float);
            case sema::TypeKind::Double: return targetLayout_.layoutOf(target::BuiltinLayoutKind::Double);
            case sema::TypeKind::Enum: return targetLayout_.layoutOf(target::BuiltinLayoutKind::Enum);
            case sema::TypeKind::Pointer:
            case sema::TypeKind::NullLiteral:
            case sema::TypeKind::StringLiteral:
                return targetLayout_.layoutOf(target::BuiltinLayoutKind::Pointer);

            case sema::TypeKind::Array:
            {
                const std::optional<target::TypeLayout> element = layoutOf(type->elementType());
                if (!element.has_value())
                {
                    return std::nullopt;
                }
                const std::optional<target::ArrayLayout> array = targetLayout_.layoutArray(*element, type->count());
                if (!array.has_value())
                {
                    return std::nullopt;
                }
                return target::TypeLayout{array->size, array->alignment};
            }

            case sema::TypeKind::Struct:
            {
                auto symbol = structSymbols_.find(type);
                if (symbol == structSymbols_.end())
                {
                    return std::nullopt;
                }
                std::vector<target::TypeLayout> fields;
                fields.reserve(symbol->second->members().size());
                for (const sema::Symbol* field : symbol->second->members())
                {
                    const std::optional<target::TypeLayout> fieldLayout = layoutOf(field->type());
                    if (!fieldLayout.has_value())
                    {
                        return std::nullopt;
                    }
                    fields.push_back(*fieldLayout);
                }
                const std::optional<target::StructLayout> layout = targetLayout_.layoutStruct(fields);
                if (!layout.has_value())
                {
                    return std::nullopt;
                }
                return target::TypeLayout{layout->size, layout->alignment};
            }

            case sema::TypeKind::Void:
            case sema::TypeKind::Function:
            case sema::TypeKind::Error:
                return std::nullopt;
        }
        return std::nullopt;
    }


    LLVMValueRef operand(const mir::MirInstruction* instruction, std::size_t index, std::string& error)
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

    LLVMValueRef symbolStorage(const sema::Symbol* symbol) const noexcept
    {
        auto local = localStorage_.find(symbol);
        if (local != localStorage_.end())
        {
            return local->second;
        }
        auto global = globalStorage_.find(symbol);
        return global != globalStorage_.end() ? global->second : nullptr;
    }

    bool failInstruction(
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

    std::vector<sema::Symbol*> globalSymbols_;
    target::TargetDataLayout targetLayout_;
    const mir::MirModule* sourceModule_ = nullptr;
    LLVMContextRef context_ = nullptr;
    LLVMModuleRef module_ = nullptr;
    LLVMBuilderRef builder_ = nullptr;
    LLVMValueRef currentFunction_ = nullptr;
    const mir::MirFunction* currentMirFunction_ = nullptr;
    LLVMValueRef trapFunction_ = nullptr;
    LLVMTypeRef trapFunctionType_ = nullptr;
    LLVMValueRef allocationFunction_ = nullptr;
    LLVMTypeRef allocationFunctionType_ = nullptr;
    LLVMValueRef deallocationFunction_ = nullptr;
    LLVMTypeRef deallocationFunctionType_ = nullptr;
    std::unordered_map<const sema::Type*, LLVMTypeRef> typeMap_;
    std::unordered_map<const sema::Type*, sema::Symbol*> structSymbols_;
    std::unordered_map<const sema::Symbol*, FieldInfo> fieldInfo_;
    std::unordered_map<const sema::Symbol*, LLVMValueRef> globalStorage_;
    std::unordered_map<const sema::Symbol*, LLVMValueRef> functionValues_;
    std::unordered_map<const sema::Symbol*, LLVMTypeRef> functionTypes_;
    std::unordered_map<const mir::MirValue*, LLVMValueRef> valueMap_;
    std::unordered_map<const mir::MirBlock*, LLVMBasicBlockRef> blockEntry_;
    std::unordered_map<const mir::MirBlock*, LLVMBasicBlockRef> blockExit_;
    std::unordered_map<const sema::Symbol*, LLVMValueRef> localStorage_;
    std::vector<DeferredPhi> deferredPhis_;
    std::uint64_t trapSequence_ = 0;
    std::uint64_t stringSequence_ = 0;
};

LlvmLowerer::LlvmLowerer(std::span<sema::Symbol* const> globalSymbols) noexcept
    : implementation_(std::make_unique<Implementation>(globalSymbols))
{
}

LlvmLowerer::~LlvmLowerer() = default;

std::unique_ptr<LlvmModule> LlvmLowerer::lower(const passes::OptimizationModule* optimizedModule)
{
    return implementation_->lower(optimizedModule, error_);
}

const std::string& LlvmLowerer::error() const noexcept
{
    return error_;
}
}
