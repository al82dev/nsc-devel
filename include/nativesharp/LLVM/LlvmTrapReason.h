#pragma once

#include <cstdint>

namespace nativesharp::llvm_backend
{
enum class LlvmTrapReason : std::uint32_t
{
    DivisionByZero = 1,
    SignedDivisionOverflow = 2,
    InvalidShiftCount = 3,
    BoundsViolation = 4,
    FloatingToIntegerOutOfRange = 5,
    AllocationSizeOverflow = 6,
    NullReference = 7,
    InterfaceDispatchFailure = 8,
    InvalidCast = 9,
    AllocationFailure = 10,
    // Reached only if __nativesharp_throw ever RETURNS, which it never does (it ends in
    // RaiseException). Emitted on that impossible edge for two reasons -- see
    // ProjectOopLlvmLowerer::lowerExceptionalTransfer's comment for the full story: it turns a
    // broken runtime invariant into a loud trap instead of letting control fall into whatever
    // happens to be laid out next, and, critically, it occupies REAL BYTES between the end of the
    // throw's protected instruction range and the block placed after it. Without those bytes an
    // enclosing `finally` is silently skipped at -O2.
    ThrowReturned = 11,
    // C1.2 "import identity": a runtime-bound import (one whose C symbol name is claimed by more
    // than one library) whose LoadLibrary/GetProcAddress pair came back null at first call -- the
    // library or the symbol is genuinely absent on this machine. The imported function's own C
    // signature has no error channel to report that through, so it traps. Exit code 128 + 12.
    ImportBindingFailure = 12,
    // C1.3 "callback exception boundary": a NativeSharp exception reached the outer edge of a
    // function that FOREIGN C code called (an `extern C delegate` target, or an `export extern C`
    // entry point). Letting it continue would unwind foreign frames with this compiler's SEH
    // exception code -- frames whose cleanup contract this compiler knows nothing about -- so the
    // boundary stops it here. Exit code 128 + 13. See ProjectOopLlvmLowerer::callbackGuardThunk.
    CallbackExceptionEscape = 13,
    // NativeSharp 3.0 T07 abort-profile panic(). Exit code is 128 + 14 = 142.
    Panic = 14,
    // T07-F06 assert(false). Kept distinct from unreachable() so crash telemetry preserves intent.
    AssertionFailed = 15,
    // T07-F06 explicit unreachable(). Reaching it is a program defect, not an optimizer hint.
    Unreachable = 16,
    // T15-F05: decimal coefficient arithmetic overflowed the 96-bit coefficient. Exit 128 + 17.
    DecimalOverflow = 17
};
}
