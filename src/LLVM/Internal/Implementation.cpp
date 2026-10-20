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

namespace llvm_lowerer_detail
{
const char* const kTargetTriple = "x86_64-pc-windows-msvc";
const char* const kTrapFunctionName = "__nativesharp_trap";
const char* const kAllocationFunctionName = "__nativesharp_alloc";
const char* const kDeallocationFunctionName = "__nativesharp_free";

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

LlvmLowerer::Implementation::Implementation(std::span<sema::Symbol* const> globalSymbols)
        : globalSymbols_(globalSymbols.begin(), globalSymbols.end()),
          targetLayout_(target::TargetDataLayout::WindowsX64())
    {
    }

std::unique_ptr<LlvmModule> LlvmLowerer::Implementation::lower(
    const passes::OptimizationModule* optimizedModule,
    std::string& error)
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

void LlvmLowerer::Implementation::reset()
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

void LlvmLowerer::Implementation::disposeState() noexcept
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

bool LlvmLowerer::Implementation::initializeModule(std::string& error)
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

bool LlvmLowerer::Implementation::declareStructs(std::string& error)
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

bool LlvmLowerer::Implementation::defineStructBodies(std::string& error)
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

bool LlvmLowerer::Implementation::declareGlobals(std::string& error)
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

bool LlvmLowerer::Implementation::declareFunctions(std::string& error)
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

bool LlvmLowerer::Implementation::declareRuntimeFunctions(std::string& error)
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

bool LlvmLowerer::Implementation::lowerGlobals(std::string& error)
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

}
