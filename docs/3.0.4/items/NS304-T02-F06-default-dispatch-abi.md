# NS304-T02-F06 — Default dispatch ABI closure

## Contract

A default selected for a concrete type has one producer-owned physical identity.
Direct and constrained calls use that identity statically; `dyn Trait` places the
same identity in the interface table. DLL exports, NSMETA ordering, MIR, LLVM and
debug artifacts must agree, and changing slot order is an evolution break.

## Causal findings and corrections

The coherent interface plan already selected the correct default for dynamic
dispatch. Static lookup on the same concrete type stopped after nominal/base
lookup and never consulted that plan. It now uses the selected implementation
ordinals as a fallback, so precedence remains centralized.

The DLL emitted default interface functions but two publication filters admitted
only class/struct instance methods. Non-abstract interface methods are now
consumer-callable and exported; abstract requirements remain excluded.

## Evidence

- `TL-LANG-307` builds a debug DLL with two default slots and a separate consumer.
- Reversing the ordered NSMETA slots is detected as a source or ABI contract break.
- MIR contains the two static callees and one two-slot interface binding.
- LLVM contains both imported declarations and the concrete interface table.
- The producer PDB is non-empty.
- Static constraint dispatch and `dyn` dispatch each produce 21; process exit is 42.
- Initial focused run exposed the missing static lookup and unpublished DLL symbols.
- Corrected run passed in 782796 microseconds; consolidation passed in 760726
  microseconds.
- Complete T02 directed set `TL-LANG-302` through `TL-LANG-307`: 6/6 `PASS`;
  nearby guards `TL-LANG-271` through `TL-LANG-278`: 8/8 `PASS`.

No complete suite was executed for this item.
