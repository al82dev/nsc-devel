#include "nativesharp/MIR/MirOpcode.h"

namespace nativesharp::mir
{
std::string_view mirOpcodeName(MirOpcode opcode) noexcept
{
    switch (opcode)
    {
#define NS_MIR_OPCODE(name, text) case MirOpcode::name: return text
        NS_MIR_OPCODE(Parameter, "param");
        NS_MIR_OPCODE(Undef, "undef");
        NS_MIR_OPCODE(Phi, "phi");
        NS_MIR_OPCODE(Constant, "const");
        NS_MIR_OPCODE(Copy, "copy");
        NS_MIR_OPCODE(LoadSymbol, "load.symbol");
        NS_MIR_OPCODE(StoreSymbol, "store.symbol");
        NS_MIR_OPCODE(AddressOfSymbol, "address.symbol");
        NS_MIR_OPCODE(Load, "load");
        NS_MIR_OPCODE(Store, "store");
        NS_MIR_OPCODE(Convert, "convert");
        NS_MIR_OPCODE(Negate, "neg");
        NS_MIR_OPCODE(BitNot, "bit.not");
        NS_MIR_OPCODE(LogicalNot, "logical.not");
        NS_MIR_OPCODE(Add, "add");
        NS_MIR_OPCODE(Subtract, "sub");
        NS_MIR_OPCODE(Multiply, "mul");
        NS_MIR_OPCODE(Divide, "div");
        NS_MIR_OPCODE(Remainder, "rem");
        NS_MIR_OPCODE(BitAnd, "bit.and");
        NS_MIR_OPCODE(BitOr, "bit.or");
        NS_MIR_OPCODE(BitXor, "bit.xor");
        NS_MIR_OPCODE(ShiftLeft, "shl");
        NS_MIR_OPCODE(ShiftRight, "shr");
        NS_MIR_OPCODE(CompareEqual, "cmp.eq");
        NS_MIR_OPCODE(CompareNotEqual, "cmp.ne");
        NS_MIR_OPCODE(CompareLess, "cmp.lt");
        NS_MIR_OPCODE(CompareLessEqual, "cmp.le");
        NS_MIR_OPCODE(CompareGreater, "cmp.gt");
        NS_MIR_OPCODE(CompareGreaterEqual, "cmp.ge");
        NS_MIR_OPCODE(PointerAdd, "ptr.add");
        NS_MIR_OPCODE(PointerSubtract, "ptr.sub");
        NS_MIR_OPCODE(PointerDifference, "ptr.diff");
        NS_MIR_OPCODE(Call, "call");
        NS_MIR_OPCODE(BoundsCheck, "bounds.check");
        NS_MIR_OPCODE(FieldAddress, "field.address");
        NS_MIR_OPCODE(IndexAddress, "index.address");
        NS_MIR_OPCODE(ExtractField, "extract.field");
        NS_MIR_OPCODE(Allocate, "alloc");
        NS_MIR_OPCODE(AllocateArray, "alloc.array");
        NS_MIR_OPCODE(Deallocate, "free");
        NS_MIR_OPCODE(DeallocateArray, "free.array");
        NS_MIR_OPCODE(StackAllocate, "stack.alloc");
        NS_MIR_OPCODE(Jump, "jump");
        NS_MIR_OPCODE(Branch, "branch");
        NS_MIR_OPCODE(Return, "return");
        NS_MIR_OPCODE(Unreachable, "unreachable");
#undef NS_MIR_OPCODE
    }
    return "invalid";
}

bool isMirTerminator(MirOpcode opcode) noexcept
{
    return opcode == MirOpcode::Jump || opcode == MirOpcode::Branch || opcode == MirOpcode::Return ||
        opcode == MirOpcode::Unreachable;
}
}
