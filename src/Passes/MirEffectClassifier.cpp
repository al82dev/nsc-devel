#include "nativesharp/Passes/MirEffectClassifier.h"

#include "nativesharp/MIR/MirInstruction.h"
#include "nativesharp/MIR/MirOpcode.h"
#include "nativesharp/MIR/MirValue.h"
#include "nativesharp/Sema/Type.h"
#include "nativesharp/Sema/TypeKind.h"

namespace nativesharp::passes
{
namespace
{
bool conversionMayTrap(const mir::MirInstruction* instruction) noexcept
{
    if (instruction == nullptr || instruction->opcode() != mir::MirOpcode::Convert ||
        instruction->result() == nullptr || instruction->operands().size() != 1 ||
        instruction->operands()[0] == nullptr)
    {
        return false;
    }

    const sema::TypeKind source = instruction->operands()[0]->type()->kind();
    const sema::TypeKind destination = instruction->result()->type()->kind();
    const bool sourceFloating = source == sema::TypeKind::Float || source == sema::TypeKind::Double;

    switch (destination)
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
            return sourceFloating;
        default:
            return false;
    }
}

bool classifyValueEffect(
    const mir::MirInstruction* instruction,
    MirEffectInfo& result) noexcept
{
    using mir::MirOpcode;
    switch (instruction->opcode())
    {
        case MirOpcode::Parameter:
            result.structural = true;
            return true;
        case MirOpcode::Undef:
        case MirOpcode::Phi:
        case MirOpcode::Constant:
        case MirOpcode::Copy:
        case MirOpcode::Negate:
        case MirOpcode::BitNot:
        case MirOpcode::LogicalNot:
        case MirOpcode::Add:
        case MirOpcode::Subtract:
        case MirOpcode::Multiply:
        case MirOpcode::BitAnd:
        case MirOpcode::BitOr:
        case MirOpcode::BitXor:
        case MirOpcode::CompareEqual:
        case MirOpcode::CompareNotEqual:
        case MirOpcode::CompareLess:
        case MirOpcode::CompareLessEqual:
        case MirOpcode::CompareGreater:
        case MirOpcode::CompareGreaterEqual:
        case MirOpcode::PointerAdd:
        case MirOpcode::PointerSubtract:
        case MirOpcode::PointerDifference:
        case MirOpcode::AddressOfSymbol:
        case MirOpcode::FieldAddress:
        case MirOpcode::IndexAddress:
        case MirOpcode::ExtractField:
            return true;
        case MirOpcode::Convert:
            result.mayTrap = conversionMayTrap(instruction);
            return true;
        case MirOpcode::Divide:
        case MirOpcode::Remainder:
        case MirOpcode::ShiftLeft:
        case MirOpcode::ShiftRight:
        case MirOpcode::BoundsCheck:
            result.mayTrap = true;
            return true;
        default:
            return false;
    }
}

bool classifyMemoryEffect(
    const mir::MirInstruction* instruction,
    MirEffectInfo& result) noexcept
{
    using mir::MirOpcode;
    switch (instruction->opcode())
    {
        case MirOpcode::LoadSymbol:
        case MirOpcode::Load:
            result.readsMemory = true;
            return true;
        case MirOpcode::StoreSymbol:
        case MirOpcode::Store:
            result.hasSideEffects = true;
            result.writesMemory = true;
            return true;
        case MirOpcode::Call:
            result.hasSideEffects = true;
            return true;
        case MirOpcode::Allocate:
        case MirOpcode::AllocateArray:
        case MirOpcode::StackAllocate:
            result.hasSideEffects = true;
            result.mayTrap = instruction->opcode() == MirOpcode::AllocateArray ||
                instruction->opcode() == MirOpcode::StackAllocate;
            return true;
        case MirOpcode::Deallocate:
        case MirOpcode::DeallocateArray:
            result.hasSideEffects = true;
            result.writesMemory = true;
            return true;
        case MirOpcode::Jump:
        case MirOpcode::Branch:
        case MirOpcode::Return:
        case MirOpcode::Unreachable:
            result.hasSideEffects = true;
            result.structural = true;
            return true;
        default:
            return false;
    }
}
}

MirEffectInfo MirEffectClassifier::classify(const mir::MirInstruction* instruction) noexcept
{
    MirEffectInfo result;
    if (instruction == nullptr)
    {
        result.structural = true;
        return result;
    }

    if (classifyValueEffect(instruction, result) ||
        classifyMemoryEffect(instruction, result))
    {
        return result;
    }

    result.structural = true;
    return result;
}

bool MirEffectClassifier::removableWhenUnused(
    const mir::MirInstruction* instruction) noexcept
{
    if (instruction == nullptr || instruction->result() == nullptr)
    {
        return false;
    }

    const MirEffectInfo effect = classify(instruction);
    return !effect.hasSideEffects && !effect.mayTrap && !effect.readsMemory &&
        !effect.writesMemory && !effect.structural;
}
}
