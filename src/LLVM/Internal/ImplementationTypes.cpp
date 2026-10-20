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

LLVMTypeRef LlvmLowerer::Implementation::llvmType(const sema::Type* type, std::string& error)
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

LLVMTypeRef LlvmLowerer::Implementation::llvmFunctionType(
    const sema::Type* type,
    std::string& error)
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

LLVMValueRef LlvmLowerer::Implementation::llvmConstant(
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

std::optional<target::TypeLayout> LlvmLowerer::Implementation::layoutOf(
    const sema::Type* type) const
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


}
