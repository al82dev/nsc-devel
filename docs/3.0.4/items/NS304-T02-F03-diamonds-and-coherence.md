# NS304-T02-F03 — Diamonds and sealed coherence

## Status

`CLOSED` on 2026-10-12.

## Contract

Unrelated inherited defaults for the same signature are ambiguous. A derived trait or interface
must redeclare the member to select or replace the default; a strictly more-derived inherited
default dominates its base without a false conflict.

NativeSharp sealed coherence is an ownership rule, not a `sealed trait` modifier. A module may
implement a local or imported trait for its own non-static class, but may not attach an impl to a
foreign class. A base-list conformance and an explicit impl for the same trait/type pair conflict.

## Causal correction

The productive impl pipeline already merged local impl blocks before member and interface
planning, rejected duplicate pairs, rejected foreign targets, and transported owner metadata
across a separate library boundary. The inherited-default ambiguity pass, however, ran only for
declarations spelled `trait`. Ordinary interfaces enabled by T02-F01 could therefore inherit two
unrelated bodies and silently keep source-order selection.

The same dominance and redeclaration analysis now covers both declaration spellings. Existing
ownership, metadata, dispatch, and syntax rules remain unchanged.

## Focused evidence

- Compiler closure: 6/6 Ninja steps from the existing LLVM/Clang 23 tree, without CMake
  regeneration.
- Active runner closure: 4/4 Ninja steps.
- `TL-LANG-304`: initial `PASS` in 933,173 microseconds; directed consolidation `PASS` in
  913,717 microseconds.
- The case rejects an unresolved ordinary-interface diamond, executes its derived redeclaration
  with exit 42, rejects base-list plus impl duplication, builds a producer library, executes an
  imported-trait/local-class impl with exit 42, and rejects an impl for the producer-owned class.
- Directed T02 cases `TL-LANG-302` through `TL-LANG-304`: 3/3 `PASS`.
- Trait and coherence guards `TL-LANG-271` through `TL-LANG-278`: 8/8 `PASS`.

No full test battery was executed.
