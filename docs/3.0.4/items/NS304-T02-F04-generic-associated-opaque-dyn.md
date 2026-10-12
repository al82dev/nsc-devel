# NS304-T02-F04 — Generic, associated, opaque and dynamic defaults

## Contract

Default trait bodies remain usable when the trait is generic, when its associated
items have defaults, and when the same implementation is consumed through a
generic constraint, `some Trait`, or `dyn Trait`. Dynamic dispatch still rejects
generic methods and traits whose associated identity is not fully defaulted.

## Causal finding

The generic, associated-item, constraint and dynamic-object paths were already
productive. The opaque-result wrapper was the missing consumer: it deliberately
skipped every trait method that had a body and only materialized associated items
provided explicitly by the concrete implementation. Consequently `some Trait`
lost both default bodies and default associated definitions.

## Correction

`ProjectOpaqueResultModel` now clones a default method into the synthetic opaque
wrapper with the established generic substitutions. It likewise clones a trait
associated-type or associated-constant default when the concrete implementation
does not override it. Abstract requirements continue to use the existing
forwarder, so the change does not weaken object-safety or requirement checking.

## Evidence

- Focused compiler build: 6/6 steps with LLVM/Clang 23.
- Focused suite runner build: 3/3 steps.
- `TL-LANG-305`: `PASS`; directed repetition in 452122 microseconds.
- Directed T02 consolidation `TL-LANG-302` through `TL-LANG-305`: 4/4 `PASS`.
- Active nearby guards `TL-LANG-271` through `TL-LANG-278`: 8/8 `PASS`.
- Rebuilt vertical guards: associated items 16/16, opaque results 14/14 and
  trait objects 12/12 `PASS`, with `NSC_LINKER` fixed to the canonical installed
  linker for their child-process contract.

No complete suite was executed for this item.
