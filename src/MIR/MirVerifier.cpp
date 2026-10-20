#include "nativesharp/MIR/MirVerifier.h"

#include "nativesharp/MIR/MirBlock.h"
#include "nativesharp/MIR/MirFunction.h"
#include "nativesharp/MIR/MirGlobal.h"
#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirModule.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Sema/Symbol.h"
#include "nativesharp/Sema/SymbolKind.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeKind.h"

#include <unordered_map>
#include <utility>

namespace nativesharp::mir
{
namespace
{
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

bool isNumeric(const sema::Type* type) noexcept
{
    return isInteger(type) || isFloating(type);
}

bool isRuntimeStorageSymbol(sema::SymbolKind kind) noexcept
{
    return kind == sema::SymbolKind::Variable || kind == sema::SymbolKind::GlobalVariable ||
        kind == sema::SymbolKind::Parameter;
}

bool isFunctionLocalSymbol(sema::SymbolKind kind) noexcept
{
    return kind == sema::SymbolKind::Variable || kind == sema::SymbolKind::Parameter;
}

bool constantMatchesType(const sema::ConstantValue& value, const sema::Type* type) noexcept
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
            return value.kind() == sema::ConstantKind::SignedInteger ||
                value.kind() == sema::ConstantKind::UnsignedInteger;
        case sema::TypeKind::Float:
        case sema::TypeKind::Double:
            return value.kind() == sema::ConstantKind::Floating;
        case sema::TypeKind::Bool:
            return value.kind() == sema::ConstantKind::Boolean;
        case sema::TypeKind::NullLiteral:
            return value.kind() == sema::ConstantKind::Null;
        case sema::TypeKind::StringLiteral:
            return value.kind() == sema::ConstantKind::String;
        default:
            return false;
    }
}

bool isComparison(MirOpcode opcode) noexcept
{
    return opcode == MirOpcode::CompareEqual || opcode == MirOpcode::CompareNotEqual ||
        opcode == MirOpcode::CompareLess || opcode == MirOpcode::CompareLessEqual ||
        opcode == MirOpcode::CompareGreater || opcode == MirOpcode::CompareGreaterEqual;
}

bool isBinaryArithmetic(MirOpcode opcode) noexcept
{
    return opcode == MirOpcode::Add || opcode == MirOpcode::Subtract || opcode == MirOpcode::Multiply ||
        opcode == MirOpcode::Divide || opcode == MirOpcode::Remainder || opcode == MirOpcode::BitAnd ||
        opcode == MirOpcode::BitOr || opcode == MirOpcode::BitXor || opcode == MirOpcode::ShiftLeft ||
        opcode == MirOpcode::ShiftRight;
}
}

bool MirVerifier::verify(const MirModule* module, MirVerificationMode mode)
{
    error_.clear();
    mode_ = mode;
    moduleSymbols_.clear();
    knownValues_.clear();
    if (module == nullptr)
    {
        return fail("MIR module is null");
    }

    moduleSymbols_.clear();
    for (const MirGlobal* global : module->globals())
    {
        if (global == nullptr || global->symbol() == nullptr ||
            global->symbol()->kind() != sema::SymbolKind::GlobalVariable)
        {
            return fail("MIR global must reference a global-variable symbol");
        }
        if (!moduleSymbols_.insert(global->symbol()).second)
        {
            return fail("MIR module contains duplicate global/function symbol");
        }
        if (global->hasExplicitInitializer() && !global->symbol()->hasConstantValue())
        {
            return fail("MIR global explicit initializer must have semantic constant value");
        }
        if (global->hasExplicitInitializer() &&
            !constantMatchesType(global->initializer(), global->symbol()->type()))
        {
            return fail("MIR global initializer constant kind does not match its semantic type");
        }
    }

    for (const MirFunction* function : module->functions())
    {
        if (function == nullptr || function->symbol() == nullptr ||
            !moduleSymbols_.insert(function->symbol()).second)
        {
            return fail("MIR module contains null or duplicate function symbol");
        }
        if (!verifyFunction(function))
        {
            return false;
        }
    }
    return true;
}

const std::string& MirVerifier::error() const noexcept { return error_; }

bool MirVerifier::verifyFunction(const MirFunction* function)
{
    knownValues_.clear();
    if (function == nullptr || function->symbol() == nullptr ||
        function->symbol()->kind() != sema::SymbolKind::Function ||
        function->symbol()->type()->kind() != sema::TypeKind::Function)
    {
        return fail("MIR function must reference a semantic function symbol");
    }
    if (!function->defined())
    {
        return function->blocks().empty() && function->parameters().empty() && function->locals().empty()
            ? true
            : fail("MIR prototype cannot contain blocks, locals or runtime parameter values");
    }
    if (function->blocks().empty() || function->entryBlock() == nullptr)
    {
        return fail("defined MIR function must contain an entry block");
    }

    std::unordered_set<const sema::Symbol*> localSymbols;
    for (const sema::Symbol* local : function->locals())
    {
        if (local == nullptr || !isFunctionLocalSymbol(local->kind()) ||
            !localSymbols.insert(local).second)
        {
            return fail("MIR function locals must be unique local-variable or parameter symbols");
        }
    }

    const std::vector<sema::Type*>& parameterTypes = function->symbol()->type()->parameterTypes();
    if (function->parameters().size() != parameterTypes.size())
    {
        return fail("defined MIR function parameter values do not match semantic signature");
    }
    for (std::size_t index = 0; index < parameterTypes.size(); ++index)
    {
        if (function->parameters()[index] == nullptr || function->parameters()[index]->type() != parameterTypes[index])
        {
            return fail("MIR parameter value type does not match semantic function signature");
        }
    }

    std::unordered_set<const MirBlock*> blockSet;
    std::unordered_map<const MirValue*, std::size_t> definitions;
    std::vector<std::size_t> parameterDefinitions(parameterTypes.size(), 0);
    for (const MirBlock* block : function->blocks())
    {
        if (block == nullptr || !blockSet.insert(block).second)
        {
            return fail("MIR function contains null or duplicate block pointer");
        }
        for (const MirInstruction* instruction : block->instructions())
        {
            if (instruction != nullptr && instruction->result() != nullptr)
            {
                knownValues_.insert(instruction->result());
                ++definitions[instruction->result()];
            }
            if (instruction != nullptr && instruction->opcode() == MirOpcode::Parameter &&
                instruction->hasImmediate() && instruction->immediate() < parameterDefinitions.size())
            {
                ++parameterDefinitions[instruction->immediate()];
            }
        }
    }
    for (const MirValue* parameter : function->parameters())
    {
        knownValues_.insert(parameter);
    }

    for (const MirBlock* block : function->blocks())
    {
        if (!verifyBlock(block, function))
        {
            return false;
        }
    }

    for (std::size_t index = 0; index < function->parameters().size(); ++index)
    {
        const MirValue* parameter = function->parameters()[index];
        const auto definition = definitions.find(parameter);
        if (parameterDefinitions[index] != 1 || definition == definitions.end() || definition->second != 1)
        {
            return fail("each MIR parameter value must have exactly one positional param definition");
        }
    }

    for (const auto& pair : definitions)
    {
        if (pair.first == nullptr || pair.first->type() == nullptr || pair.second == 0)
        {
            return fail("MIR virtual value has invalid definition information");
        }
        if (mode_ == MirVerificationMode::Ssa && pair.second != 1)
        {
            return fail("SSA MIR virtual value must have exactly one definition");
        }
    }
    return true;
}

bool MirVerifier::verifyBlock(const MirBlock* block, const MirFunction* function)
{
    if (block->instructions().empty())
    {
        return fail("MIR basic block cannot be empty");
    }
    bool sawTerminator = false;
    bool sawNonPhi = false;
    for (std::size_t index = 0; index < block->instructions().size(); ++index)
    {
        const MirInstruction* instruction = block->instructions()[index];
        if (instruction == nullptr)
        {
            return fail("MIR block contains null instruction");
        }
        const bool terminator = isMirTerminator(instruction->opcode());
        if (mode_ == MirVerificationMode::Ssa)
        {
            if (instruction->opcode() == MirOpcode::Phi && sawNonPhi)
            {
                return fail("SSA PHI instructions must form a contiguous block prefix");
            }
            if (instruction->opcode() != MirOpcode::Phi)
            {
                sawNonPhi = true;
            }
        }
        if (sawTerminator || (terminator && index + 1 != block->instructions().size()))
        {
            return fail("MIR terminator must be the final instruction of its block");
        }
        if (instruction->opcode() == MirOpcode::Parameter && block != function->entryBlock())
        {
            return fail("param instruction must appear in the function entry block");
        }
        if (!verifyInstruction(instruction, function))
        {
            return false;
        }
        sawTerminator = terminator;
    }
    return sawTerminator ? true : fail("every MIR block must end in exactly one terminator");
}

bool MirVerifier::verifyUndefInstruction(
    const MirInstruction* instruction,
    const MirFunction* function)
{
    if (mode_ != MirVerificationMode::Ssa)
        return fail("pre-SSA MIR cannot contain SSA-only undef instructions");
    if (instruction->result() == nullptr || !instruction->operands().empty() ||
        !instruction->phiIncoming().empty())
        return fail("SSA undef requires exactly a typed result and no operands");
    if (instruction->symbol() != nullptr &&
        (!verifyStorageSymbol(instruction->symbol(), function) ||
         instruction->symbol()->type() != instruction->result()->type()))
        return fail("symbol-associated SSA undef must match in-scope storage type");
    return true;
}

bool MirVerifier::verifyPhiInstruction(
    const MirInstruction* instruction,
    const MirFunction* function)
{
    if (mode_ != MirVerificationMode::Ssa)
        return fail("pre-SSA MIR cannot contain SSA-only phi instructions");
    if (instruction->result() == nullptr || !instruction->operands().empty() ||
        instruction->phiIncoming().empty())
        return fail("SSA phi requires typed result and one or more incoming pairs");
    if (instruction->symbol() != nullptr &&
        (!verifyStorageSymbol(instruction->symbol(), function) ||
         instruction->symbol()->type() != instruction->result()->type()))
        return fail("symbol-associated SSA phi must match in-scope storage type");
    for (const MirPhiIncoming& incoming : instruction->phiIncoming())
    {
        if (incoming.block == nullptr || incoming.value == nullptr ||
            !verifyTarget(incoming.block, function) || !knownValues_.contains(incoming.value) ||
            incoming.value->type() != instruction->result()->type())
            return fail("SSA phi incoming pair must reference in-function block and same-typed value");
    }
    return true;
}

bool MirVerifier::verifyParameterInstruction(
    const MirInstruction* instruction,
    const MirFunction* function)
{
    if (instruction->result() == nullptr || instruction->symbol() == nullptr ||
        instruction->symbol()->kind() != sema::SymbolKind::Parameter ||
        !verifyStorageSymbol(instruction->symbol(), function) ||
        instruction->result()->type() != instruction->symbol()->type() ||
        !instruction->hasImmediate() || !instruction->operands().empty() ||
        instruction->immediate() >= function->parameters().size())
        return fail("param requires typed result, parameter symbol and valid positional index");
    return function->parameters()[instruction->immediate()] == instruction->result()
        ? true
        : fail("param result must be the function parameter value at its positional index");
}

bool MirVerifier::verifySimpleStorageInstruction(
    const MirInstruction* instruction,
    const MirFunction* function)
{
    switch (instruction->opcode())
    {
        case MirOpcode::Constant:
            return instruction->result() != nullptr && instruction->hasConstant() &&
                    instruction->operands().empty() &&
                    constantMatchesType(instruction->constant(), instruction->result()->type())
                ? true
                : fail("const requires result and constant payload compatible with its type");
        case MirOpcode::Copy:
            return instruction->result() != nullptr && instruction->operands().size() == 1 &&
                    instruction->result()->type() == instruction->operands()[0]->type()
                ? true
                : fail("copy requires one operand with the same result type");
        case MirOpcode::LoadSymbol:
            return instruction->result() != nullptr && instruction->symbol() != nullptr &&
                    verifyStorageSymbol(instruction->symbol(), function) &&
                    instruction->result()->type() == instruction->symbol()->type() &&
                    instruction->operands().empty()
                ? true
                : fail("load.symbol requires in-scope MIR storage symbol and matching result type");
        case MirOpcode::StoreSymbol:
            return instruction->result() == nullptr && instruction->symbol() != nullptr &&
                    verifyStorageSymbol(instruction->symbol(), function) &&
                    instruction->operands().size() == 1 &&
                    instruction->operands()[0]->type() == instruction->symbol()->type()
                ? true
                : fail("store.symbol requires one value exactly matching in-scope storage type");
        case MirOpcode::AddressOfSymbol:
            return instruction->result() != nullptr && instruction->symbol() != nullptr &&
                    verifyStorageSymbol(instruction->symbol(), function) &&
                    instruction->result()->type()->kind() == sema::TypeKind::Pointer &&
                    instruction->result()->type()->elementType() == instruction->symbol()->type() &&
                    instruction->operands().empty()
                ? true
                : fail("address.symbol requires in-scope pointer-to-storage result");
        default:
            return false;
    }
}

bool MirVerifier::verifyMemoryInstruction(const MirInstruction* instruction)
{
    switch (instruction->opcode())
    {
        case MirOpcode::Load:
            return instruction->result() != nullptr && instruction->operands().size() == 1 &&
                    instruction->operands()[0]->type()->kind() == sema::TypeKind::Pointer &&
                    instruction->operands()[0]->type()->elementType() == instruction->result()->type()
                ? true
                : fail("load requires pointer-to-result operand");
        case MirOpcode::Store:
            return instruction->result() == nullptr && instruction->operands().size() == 2 &&
                    instruction->operands()[0]->type()->kind() == sema::TypeKind::Pointer &&
                    instruction->operands()[0]->type()->elementType() == instruction->operands()[1]->type()
                ? true
                : fail("store requires pointer-to-T and T operands");
        case MirOpcode::Convert:
            if (instruction->result() == nullptr || instruction->operands().size() != 1 ||
                !instruction->hasConversion())
                return fail("convert requires result, source value and conversion kind");
            if (instruction->conversionKind() == sema::ConversionKind::Identity ||
                instruction->conversionKind() == sema::ConversionKind::ToVoid ||
                instruction->result()->type()->kind() == sema::TypeKind::Void)
                return fail("convert cannot encode identity or to-void conversion");
            return true;
        default:
            return false;
    }
}

bool MirVerifier::verifyUnaryInstruction(const MirInstruction* instruction)
{
    switch (instruction->opcode())
    {
        case MirOpcode::Negate:
            return instruction->result() != nullptr && instruction->operands().size() == 1 &&
                    isNumeric(instruction->result()->type()) &&
                    instruction->result()->type() == instruction->operands()[0]->type()
                ? true : fail("neg requires one numeric operand matching its result type");
        case MirOpcode::BitNot:
            return instruction->result() != nullptr && instruction->operands().size() == 1 &&
                    isInteger(instruction->result()->type()) &&
                    instruction->result()->type() == instruction->operands()[0]->type()
                ? true : fail("bit.not requires one integer operand matching its result type");
        case MirOpcode::LogicalNot:
            return instruction->result() != nullptr && instruction->operands().size() == 1 &&
                    instruction->result()->type()->kind() == sema::TypeKind::Bool &&
                    instruction->operands()[0]->type()->kind() == sema::TypeKind::Bool
                ? true : fail("logical.not requires bool operand and bool result");
        default:
            return false;
    }
}

bool MirVerifier::verifyArithmeticInstruction(const MirInstruction* instruction)
{
    switch (instruction->opcode())
    {
        case MirOpcode::Add:
        case MirOpcode::Subtract:
        case MirOpcode::Multiply:
        case MirOpcode::Divide:
            if (!isBinaryArithmetic(instruction->opcode()) || instruction->result() == nullptr ||
                instruction->operands().size() != 2 || !isNumeric(instruction->result()->type()) ||
                instruction->operands()[0]->type() != instruction->result()->type() ||
                instruction->operands()[1]->type() != instruction->result()->type())
                return fail("numeric MIR arithmetic requires two operands matching the result type");
            return true;
        case MirOpcode::Remainder:
        case MirOpcode::BitAnd:
        case MirOpcode::BitOr:
        case MirOpcode::BitXor:
            if (instruction->result() == nullptr || instruction->operands().size() != 2 ||
                !isInteger(instruction->result()->type()) ||
                instruction->operands()[0]->type() != instruction->result()->type() ||
                instruction->operands()[1]->type() != instruction->result()->type())
                return fail("integer MIR operation requires two operands matching the result type");
            return true;
        case MirOpcode::ShiftLeft:
        case MirOpcode::ShiftRight:
            if (instruction->result() == nullptr || instruction->operands().size() != 2 ||
                !isInteger(instruction->result()->type()) ||
                instruction->operands()[0]->type() != instruction->result()->type() ||
                !isInteger(instruction->operands()[1]->type()))
                return fail("shift requires integer left/result type and integer count operand");
            return true;
        default:
            return false;
    }
}

bool MirVerifier::verifyComparisonInstruction(const MirInstruction* instruction)
{
    if (!isComparison(instruction->opcode()) || instruction->result() == nullptr ||
        instruction->result()->type()->kind() != sema::TypeKind::Bool ||
        instruction->operands().size() != 2 ||
        instruction->operands()[0]->type() != instruction->operands()[1]->type())
        return fail("comparison requires two same-typed operands and bool result");
    const sema::Type* operandType = instruction->operands()[0]->type();
    if (instruction->opcode() == MirOpcode::CompareEqual ||
        instruction->opcode() == MirOpcode::CompareNotEqual)
    {
        return isNumeric(operandType) || operandType->kind() == sema::TypeKind::Bool ||
                operandType->kind() == sema::TypeKind::Enum ||
                operandType->kind() == sema::TypeKind::Pointer
            ? true
            : fail("equality comparison requires numeric, bool, enum or pointer operands");
    }
    return isNumeric(operandType) ? true : fail("relational comparison requires numeric operands");
}

bool MirVerifier::verifyPointerInstruction(const MirInstruction* instruction)
{
    if (instruction->opcode() == MirOpcode::PointerDifference)
    {
        return instruction->result() != nullptr &&
                instruction->result()->type()->kind() == sema::TypeKind::NInt &&
                instruction->operands().size() == 2 &&
                instruction->operands()[0]->type()->kind() == sema::TypeKind::Pointer &&
                instruction->operands()[0]->type()->elementType() != nullptr &&
                instruction->operands()[0]->type()->elementType()->kind() != sema::TypeKind::Void &&
                instruction->operands()[0]->type()->elementType()->isComplete() &&
                instruction->operands()[0]->type() == instruction->operands()[1]->type()
            ? true
            : fail("pointer difference requires matching complete non-void pointers and nint result");
    }
    if (instruction->result() == nullptr ||
        instruction->result()->type()->kind() != sema::TypeKind::Pointer ||
        instruction->result()->type()->elementType() == nullptr ||
        instruction->result()->type()->elementType()->kind() == sema::TypeKind::Void ||
        !instruction->result()->type()->elementType()->isComplete() ||
        instruction->operands().empty() ||
        instruction->operands()[0]->type() != instruction->result()->type())
        return fail("pointer arithmetic requires matching pointer to complete non-void element type");
    if (instruction->hasImmediate())
    {
        return instruction->operands().size() == 1
            ? true
            : fail("immediate pointer step cannot also have an index operand");
    }
    return instruction->operands().size() == 2 && isInteger(instruction->operands()[1]->type())
        ? true
        : fail("pointer arithmetic requires integer index operand");
}

bool MirVerifier::verifyCallInstruction(const MirInstruction* instruction)
{
    if (instruction->symbol() == nullptr ||
        instruction->symbol()->kind() != sema::SymbolKind::Function ||
        instruction->symbol()->type()->kind() != sema::TypeKind::Function ||
        !moduleSymbols_.contains(instruction->symbol()))
        return fail("call requires resolved function symbol");
    const std::vector<sema::Type*>& parameters = instruction->symbol()->type()->parameterTypes();
    if (parameters.size() != instruction->operands().size())
        return fail("call argument count does not match function type");
    for (std::size_t index = 0; index < parameters.size(); ++index)
    {
        if (instruction->operands()[index]->type() != parameters[index])
            return fail("call argument type does not match function parameter type");
    }
    sema::Type* returnType = instruction->symbol()->type()->returnType();
    if (returnType->kind() == sema::TypeKind::Void)
        return instruction->result() == nullptr ? true : fail("void call cannot produce MIR result");
    return instruction->result() != nullptr && instruction->result()->type() == returnType
        ? true
        : fail("non-void call result must match function return type");
}

bool MirVerifier::verifyAggregateInstruction(const MirInstruction* instruction)
{
    switch (instruction->opcode())
    {
        case MirOpcode::BoundsCheck:
            return instruction->result() == nullptr && instruction->operands().size() == 1 &&
                    isInteger(instruction->operands()[0]->type()) && instruction->hasImmediate() &&
                    instruction->immediate() > 0
                ? true : fail("bounds.check requires integer index and positive fixed upper bound");
        case MirOpcode::FieldAddress:
            return instruction->result() != nullptr &&
                    instruction->result()->type()->kind() == sema::TypeKind::Pointer &&
                    instruction->symbol() != nullptr &&
                    instruction->symbol()->kind() == sema::SymbolKind::Field &&
                    instruction->result()->type()->elementType() == instruction->symbol()->type() &&
                    instruction->symbol()->hasFieldOffset() && instruction->hasOffset() &&
                    instruction->offset() == instruction->symbol()->fieldOffset() &&
                    instruction->operands().size() == 1 &&
                    instruction->operands()[0]->type()->kind() == sema::TypeKind::Pointer &&
                    instruction->operands()[0]->type()->elementType()->kind() == sema::TypeKind::Struct
                ? true
                : fail("field.address requires pointer-to-struct base, field, offset and pointer result");
        case MirOpcode::IndexAddress:
        {
            if (instruction->result() == nullptr ||
                instruction->result()->type()->kind() != sema::TypeKind::Pointer ||
                instruction->auxiliaryType() == nullptr || instruction->operands().size() != 2 ||
                !isInteger(instruction->operands()[1]->type()))
                return fail("index.address requires base, integer index, base type and pointer result");
            const sema::Type* baseType = instruction->auxiliaryType();
            const sema::Type* loweredBaseType = instruction->operands()[0]->type();
            if (baseType->kind() == sema::TypeKind::Array)
                return loweredBaseType->kind() == sema::TypeKind::Pointer &&
                        loweredBaseType->elementType() == baseType &&
                        instruction->result()->type()->elementType() == baseType->elementType()
                    ? true : fail("array index.address base/result relationship is invalid");
            if (baseType->kind() == sema::TypeKind::Pointer)
                return loweredBaseType == baseType && instruction->result()->type() == baseType
                    ? true : fail("pointer index.address must preserve the pointer type");
            if (baseType->kind() == sema::TypeKind::StringLiteral)
                return loweredBaseType == baseType &&
                        instruction->result()->type()->elementType() == baseType->elementType()
                    ? true : fail("string index.address must produce pointer to the string element type");
            return fail("index.address base type must be fixed array, pointer or string literal");
        }
        case MirOpcode::ExtractField:
            return instruction->result() != nullptr && instruction->symbol() != nullptr &&
                    instruction->symbol()->kind() == sema::SymbolKind::Field &&
                    instruction->result()->type() == instruction->symbol()->type() &&
                    instruction->symbol()->hasFieldOffset() && instruction->hasOffset() &&
                    instruction->offset() == instruction->symbol()->fieldOffset() &&
                    instruction->operands().size() == 1 &&
                    instruction->operands()[0]->type()->kind() == sema::TypeKind::Struct
                ? true : fail("extract.field requires struct value, field symbol and offset");
        default:
            return false;
    }
}

bool MirVerifier::verifyAllocationInstruction(const MirInstruction* instruction)
{
    switch (instruction->opcode())
    {
        case MirOpcode::Allocate:
            return instruction->result() != nullptr &&
                    instruction->result()->type()->kind() == sema::TypeKind::Pointer &&
                    instruction->auxiliaryType() == instruction->result()->type()->elementType() &&
                    instruction->auxiliaryType() != nullptr &&
                    instruction->auxiliaryType()->kind() != sema::TypeKind::Void &&
                    instruction->auxiliaryType()->isComplete() && instruction->operands().empty()
                ? true : fail("alloc requires pointer result and allocated element type");
        case MirOpcode::AllocateArray:
            return instruction->result() != nullptr &&
                    instruction->result()->type()->kind() == sema::TypeKind::Pointer &&
                    instruction->auxiliaryType() == instruction->result()->type()->elementType() &&
                    instruction->auxiliaryType() != nullptr &&
                    instruction->auxiliaryType()->kind() != sema::TypeKind::Void &&
                    instruction->auxiliaryType()->isComplete() && instruction->operands().size() == 1 &&
                    isInteger(instruction->operands()[0]->type())
                ? true : fail("alloc.array requires pointer result, element type and integer count");
        case MirOpcode::Deallocate:
        case MirOpcode::DeallocateArray:
            return instruction->result() == nullptr && instruction->operands().size() == 1 &&
                    (instruction->operands()[0]->type()->kind() == sema::TypeKind::Pointer ||
                     instruction->operands()[0]->type()->kind() == sema::TypeKind::NullLiteral)
                ? true : fail("free requires pointer or null operand");
        case MirOpcode::StackAllocate:
            return instruction->result() != nullptr &&
                    instruction->result()->type()->kind() == sema::TypeKind::Pointer &&
                    instruction->auxiliaryType() == instruction->result()->type()->elementType() &&
                    instruction->auxiliaryType() != nullptr &&
                    instruction->auxiliaryType()->kind() != sema::TypeKind::Void &&
                    instruction->auxiliaryType()->isComplete() && instruction->operands().size() == 1 &&
                    isInteger(instruction->operands()[0]->type())
                ? true : fail("stack.alloc requires pointer result, element type and integer count");
        default:
            return false;
    }
}

bool MirVerifier::verifyTerminatorInstruction(
    const MirInstruction* instruction,
    const MirFunction* function)
{
    switch (instruction->opcode())
    {
        case MirOpcode::Jump:
            return instruction->result() == nullptr && instruction->operands().empty() &&
                    instruction->firstTarget() != nullptr && instruction->secondTarget() == nullptr &&
                    verifyTarget(instruction->firstTarget(), function)
                ? true : fail("jump requires exactly one in-function target");
        case MirOpcode::Branch:
            return instruction->result() == nullptr && instruction->operands().size() == 1 &&
                    instruction->operands()[0]->type()->kind() == sema::TypeKind::Bool &&
                    instruction->firstTarget() != nullptr && instruction->secondTarget() != nullptr &&
                    verifyTarget(instruction->firstTarget(), function) &&
                    verifyTarget(instruction->secondTarget(), function)
                ? true : fail("branch requires bool condition and two in-function targets");
        case MirOpcode::Return:
        {
            sema::Type* returnType = function->symbol()->type()->returnType();
            if (returnType->kind() == sema::TypeKind::Void)
                return instruction->operands().empty() && instruction->result() == nullptr
                    ? true : fail("void MIR return cannot contain value");
            return instruction->result() == nullptr && instruction->operands().size() == 1 &&
                    instruction->operands()[0]->type() == returnType
                ? true : fail("MIR return value must match function return type");
        }
        case MirOpcode::Unreachable:
            return instruction->result() == nullptr && instruction->operands().empty() &&
                    instruction->firstTarget() == nullptr && instruction->secondTarget() == nullptr
                ? true : fail("unreachable terminator cannot have values or targets");
        default:
            return false;
    }
}

bool MirVerifier::verifyInstruction(const MirInstruction* instruction, const MirFunction* function)
{
    for (const MirValue* operand : instruction->operands())
    {
        if (!verifyValue(operand) || !knownValues_.contains(operand))
            return fail("MIR instruction references an unknown or untyped virtual value");
    }
    if (instruction->result() != nullptr && !verifyValue(instruction->result()))
        return fail("MIR instruction result has no semantic type");
    if (instruction->opcode() != MirOpcode::Phi && !instruction->phiIncoming().empty())
        return fail("only SSA PHI instructions may contain PHI incoming pairs");

    switch (instruction->opcode())
    {
        case MirOpcode::Undef:
            return verifyUndefInstruction(instruction, function);
        case MirOpcode::Phi:
            return verifyPhiInstruction(instruction, function);
        case MirOpcode::Parameter:
            return verifyParameterInstruction(instruction, function);
        case MirOpcode::Constant:
        case MirOpcode::Copy:
        case MirOpcode::LoadSymbol:
        case MirOpcode::StoreSymbol:
        case MirOpcode::AddressOfSymbol:
            return verifySimpleStorageInstruction(instruction, function);
        case MirOpcode::Load:
        case MirOpcode::Store:
        case MirOpcode::Convert:
            return verifyMemoryInstruction(instruction);
        case MirOpcode::Negate:
        case MirOpcode::BitNot:
        case MirOpcode::LogicalNot:
            return verifyUnaryInstruction(instruction);
        case MirOpcode::Add:
        case MirOpcode::Subtract:
        case MirOpcode::Multiply:
        case MirOpcode::Divide:
        case MirOpcode::Remainder:
        case MirOpcode::BitAnd:
        case MirOpcode::BitOr:
        case MirOpcode::BitXor:
        case MirOpcode::ShiftLeft:
        case MirOpcode::ShiftRight:
            return verifyArithmeticInstruction(instruction);
        case MirOpcode::CompareEqual:
        case MirOpcode::CompareNotEqual:
        case MirOpcode::CompareLess:
        case MirOpcode::CompareLessEqual:
        case MirOpcode::CompareGreater:
        case MirOpcode::CompareGreaterEqual:
            return verifyComparisonInstruction(instruction);
        case MirOpcode::PointerAdd:
        case MirOpcode::PointerSubtract:
        case MirOpcode::PointerDifference:
            return verifyPointerInstruction(instruction);
        case MirOpcode::Call:
            return verifyCallInstruction(instruction);
        case MirOpcode::BoundsCheck:
        case MirOpcode::FieldAddress:
        case MirOpcode::IndexAddress:
        case MirOpcode::ExtractField:
            return verifyAggregateInstruction(instruction);
        case MirOpcode::Allocate:
        case MirOpcode::AllocateArray:
        case MirOpcode::Deallocate:
        case MirOpcode::DeallocateArray:
        case MirOpcode::StackAllocate:
            return verifyAllocationInstruction(instruction);
        case MirOpcode::Jump:
        case MirOpcode::Branch:
        case MirOpcode::Return:
        case MirOpcode::Unreachable:
            return verifyTerminatorInstruction(instruction, function);
    }
    return fail("unknown MIR opcode");
}

bool MirVerifier::verifyStorageSymbol(
    const sema::Symbol* symbol,
    const MirFunction* function) const noexcept
{
    if (symbol == nullptr || function == nullptr || !isRuntimeStorageSymbol(symbol->kind()))
    {
        return false;
    }
    if (symbol->kind() == sema::SymbolKind::GlobalVariable)
    {
        return moduleSymbols_.contains(symbol);
    }
    for (const sema::Symbol* local : function->locals())
    {
        if (local == symbol)
        {
            return true;
        }
    }
    return false;
}

bool MirVerifier::verifyTarget(const MirBlock* target, const MirFunction* function)
{
    if (target == nullptr)
    {
        return false;
    }
    for (const MirBlock* block : function->blocks())
    {
        if (block == target)
        {
            return true;
        }
    }
    return false;
}

bool MirVerifier::verifyValue(const MirValue* value) const noexcept
{
    return value != nullptr && value->type() != nullptr;
}

bool MirVerifier::fail(std::string message)
{
    if (error_.empty())
    {
        error_ = std::move(message);
    }
    return false;
}
}
