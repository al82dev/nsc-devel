# NS304-T02-F05 — Versioned NSMETA default witnesses

## Contract

A precompiled trait or interface default is owned by the producer. A consumer
reconstructs the semantic declaration from versioned NSMETA, binds it to the
producer's physical symbol and does not require the producer source files.

## Causal finding

NSMETA 1.39 already carries trait identity, dispatch order, member flags and the
physical mangled identity for every non-abstract default. Ordinary methods were
reconstructed with a placeholder body and then rebound correctly. Property and
indexer accessors were always emitted as abstract declarations in an interface
context, which discarded their physical identity before binding.

## Correction

Metadata-backed accessors now mirror ordinary methods: an abstract record emits
`;`, while a non-abstract record emits a placeholder body. The established
precompiled-member binding then replaces that local identity with the producer
callee. No metadata schema or ABI version change was needed because NSMETA 1.39
already contains the authoritative flags and mangled names.

## Evidence

- `TL-LANG-306` initially rejected only `get_Value()` and `get_Item(int)`.
- After the correction, `TL-LANG-306` passed in 518878 microseconds; directed
  repetition passed in 521615 microseconds.
- The fixture reads current NSMETA and confirms three non-abstract physical
  witnesses, deletes producer `.ns` and `.nsproj`, then builds and runs the
  consumer from `.lib/.nsmeta` only; observed exit is 42.
- Directed T02 cases `TL-LANG-302` through `TL-LANG-306`: 5/5 `PASS`.
- Nearby guards `TL-LANG-271` through `TL-LANG-278`: 8/8 `PASS`.
- Focused compiler build: 6/6; focused runner build: 2/2.

No complete suite was executed for this item.
