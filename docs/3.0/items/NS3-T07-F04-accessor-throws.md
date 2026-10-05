# NS3-T07-F04 — Typed throws on property and indexer accessors

Status: implemented on `development`
Platform: Windows x64
Validation profile: directed O0/NSLink only; no baseline

## Contract

Edition 3 properties and indexers can publish independent checked-error contracts for their
getter and setter:

```nativesharp
public int this[int index]
{
    get throws RangeError { return Read(index); }
    set throws RangeError { Write(index, value); }
}
```

An access must be covered by an active `catch` or by a compatible `throws E` on its enclosing
callable. The getter and setter remain ordinary synthesized member functions after parsing, so
they reuse the existing type validation, inheritance rules, NS3071 analysis, native EH lowering,
HIR/MIR contracts, ABI identity and method-body pipeline.

The accessor contract is edition-3-only. `throws` remains contextual in edition 2.

## Metadata closure

NSMETA already records `declaredErrorTypeKey` for every method, including synthesized accessors.
The public-source materializer now emits that contract after `get` or `set`; a library consumer
therefore reconstructs the same checked API instead of silently losing the error declaration.

## Files changed

- `src/Parser/Parser.cpp`: parses `throws E` after property and indexer accessor keywords.
- `src/Driver/Driver.cpp`: preserves accessor throws while reconstructing public source from
  NSMETA.
- `tests/unit/NativeSharp3T07ErrorTests.cpp`: covers property/indexer get and set execution,
  NS3071 rejection, metadata round-trip and a metadata consumer.
- `docs/3.0/language/typed-throws.md`: documents the bilingual user syntax.

No AST shape, metadata version, runtime helper, linker feature or managed dependency was added.

## Directed evidence

- Exact target `nsc_ns3_t07_error_tests`: built successfully.
- Exact executable: `PASS: 58 checks`.
- Productive paths: property and indexer get/set execute through O0 and NSLink.
- Negative paths: an unhandled property get and indexer set report NS3071.
- NSMETA 1.38: accessor `declaredErrorTypeKey` survives producer, materializer and consumer.

No CTest suite, matrix, baseline, O1/O2/O3, install, deploy, remote operation or write to `C:` was
performed.
