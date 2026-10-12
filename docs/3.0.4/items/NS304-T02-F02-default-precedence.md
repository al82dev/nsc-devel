# NS304-T02-F02 — Default implementation precedence

## Status

`CLOSED` on 2026-10-12.

## Contract

Interface-table construction selects an explicit implementation first, a nominal member second,
and the most-specific inherited default last. The same default-selection rule applies to `trait`
and ordinary `interface` declarations.

This item defines precedence only. Diamond ambiguity, sealed traits, coherence conflicts,
cross-module conflicts, generic associated items, metadata transport, and final ABI evolution
remain owned by T02-F03 through T02-F06.

## Causal correction

The existing plan already searched an implementing class before falling back to a default, so
explicit implementation blocks and nominal class members retained their priority. Two separate
branches still restricted default replacement to declarations spelled `trait`: one when merging an
inherited candidate and another when the current interface redeclared the same contract. An
ordinary derived interface therefore retained its base default even when its own body was more
specific.

Both replacement branches now use declaration shape and derivation order instead of the spelling
of the declaring abstraction. No parser, dispatch, or ABI rule was weakened.

## Focused evidence

- Compiler closure: 6/6 Ninja steps from the existing LLVM/Clang 23 tree, without CMake
  regeneration.
- Active runner closure: 3/3 Ninja steps.
- `TL-LANG-303`: `PASS` in 392,739 microseconds. The linked executable independently checks
  explicit implementation, nominal member, and most-specific ordinary-interface default; each
  path produces 42.
- Previous default-body guard `TL-LANG-302`: `PASS` in 419,338 microseconds.
- Trait-model guards `TL-LANG-271` through `TL-LANG-278`: 8/8 `PASS`.

No full test battery was executed.
