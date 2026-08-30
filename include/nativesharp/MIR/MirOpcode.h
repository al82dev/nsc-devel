#pragma once

#include <string_view>

namespace nativesharp::mir
{
enum class MirOpcode
{
    Parameter,
    Undef,
    Phi,
    Constant,
    Copy,
    LoadSymbol,
    StoreSymbol,
    AddressOfSymbol,
    Load,
    Store,
    Convert,
    Negate,
    BitNot,
    LogicalNot,
    Add,
    Subtract,
    Multiply,
    Divide,
    Remainder,
    BitAnd,
    BitOr,
    BitXor,
    ShiftLeft,
    ShiftRight,
    CompareEqual,
    CompareNotEqual,
    CompareLess,
    CompareLessEqual,
    CompareGreater,
    CompareGreaterEqual,
    PointerAdd,
    PointerSubtract,
    PointerDifference,
    Call,
    BoundsCheck,
    FieldAddress,
    IndexAddress,
    ExtractField,
    Allocate,
    AllocateArray,
    Deallocate,
    DeallocateArray,
    StackAllocate,
    Jump,
    Branch,
    Return,
    Unreachable
};

std::string_view mirOpcodeName(MirOpcode opcode) noexcept;
bool isMirTerminator(MirOpcode opcode) noexcept;
}
