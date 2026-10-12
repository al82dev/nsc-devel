# NS304-T02-F01 — Default trait and interface bodies

## Status

`CLOSED` on 2026-10-12.

## Contract

NativeSharp 3.0.4 accepts productive default bodies for instance methods, property accessors,
and indexer accessors declared by both `trait` and `interface`. A bodyless accessor remains an
abstract requirement. An explicitly `abstract` member cannot declare a body.

This item does not decide implementation precedence, diamond conflicts, generic associated
items, metadata transport, or the final DLL/vtable ABI. Those remain owned by T02-F02 through
T02-F06.

## Causal correction

The parser previously had three independent deferrals: ordinary interface methods were born
abstract before their body was known; interface-context property/indexer accessors were always
marked abstract; and all accessor bodies reported the deferred-language diagnostic. The semantic
interface model then prohibited ordinary interface bodies and selected a default implementation
only when the declaration used the `trait` spelling.

The corrected vertical assigns abstractness from the actual declaration shape (`;` versus a
block), validates the same requirement/default invariant for both declaration spellings, and lets
the existing interface-table plan select any declared default body. HIR, MIR, and LLVM already
consume that plan without a trait-only branch.

## Focused evidence

- Compiler closure: 7/7 Ninja steps from the existing LLVM/Clang 23 tree, without CMake
  regeneration.
- Active runner closure: 5/5 Ninja steps.
- `TL-LANG-302`: `PASS` twice; the second run completed in 413,062 microseconds.
- Parser/interface guards `TP-PAR-183` through `TP-PAR-196`: 14/14 `PASS`.
- Trait guards `TL-LANG-271` through `TL-LANG-278`: 8/8 `PASS`.
- Historical trait-declaration unit executable: 14/14 checks `PASS` with the canonical NSLink
  and Windows runtime paths from the active suite configuration.

No full test battery was executed.
