#pragma once

#include "../LlvmCApi.h"

namespace nativesharp::llvm_backend
{
using namespace llvm_api;

// A cleanup region (a RAII local's scope-exit, a `using`, an owned-parameter destroy, or a
// `finally` body) can be reached by more than one exceptional unwind chain within the same
// method -- for example a local declared before two sibling try blocks is unwound both by
// "no catch in try #1 matched" and, independently, by "try #2 itself needs to unwind past
// it". Before this cache existed, emitExceptionalCleanupChain() lowered that SAME MIR region
// fresh for every chain that passed through it, producing two independent `cleanuppad within
// none` instructions for one logical scope.
//
// ACCURACY NOTE (external audit item "comentarios contradictorios sobre la causa raíz",
// 2026-08-29): this comment used to assert that the duplicate pads were "what
// BUGS_ENCONTRADOS/bug_try_catch_reuso_objeto_tras_catch.md's crash came from" -- i.e. that
// this cache was the fix for bug B-001. That claim was never checked by running anything, and
// it is FALSE on two counts, both established by real Wine execution: (1) the cache was already
// in the built binary while B-001's access violation still reproduced identically, and (2)
// disabling the cache outright (always lowering a fresh cleanuppad per chain) changes NOTHING
// about the EH bugs known today -- it was measured against both the B-006 double-destruction
// and the B-007 nested-catch repro, with byte-identical results. Emitting one pad per logical
// scope is still the right thing to do (it is what the source's scope nesting actually means,
// and duplicate "within none" pads for one scope are simply wrong IR), so the cache stays --
// but as correctness hygiene, NOT as the fix for any known bug. B-001's actual fix is the
// per-function mem2reg legalization in src/LLVM/LlvmOptimizer.cpp. Caching by ordinal makes sure
// each cleanup region gets exactly one cleanuppad, reused by every chain that needs it,
// matching its real (unique, static) position in the source's scope nesting. `parentPad` is
// stored alongside the block because the same ordinal COULD legitimately need two different
// funclet parents if it were ever reached from within two different enclosing funclets (a
// cleanup call made from inside a catch handler's own body would need a different parent
// than the same ordinal reached from top-level code) -- the cache is only reused when the
// parent matches, so a genuinely different context still falls back to lowering fresh rather
// than reusing an incompatible pad.
struct CachedCleanup final
{
    LLVMValueRef parentPad = nullptr;
    LLVMBasicBlockRef next = nullptr;
    LLVMBasicBlockRef block = nullptr;
};
}
