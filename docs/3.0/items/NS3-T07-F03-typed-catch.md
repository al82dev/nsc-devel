# NS3-T07-F03 — Typed try, catch filters, and finally

Status: implemented on `development`
Platform: Windows x64
Validation profile: directed O0/NSLink only; no baseline

## Delivered vertical

Edition 3 treats a `try` region as a closed set of the errors declared by operations in its
protected block. Catch clauses are checked in source order against that set. A typed clause, an
optional binding, a catch-all, and an edition-3 contextual filter are supported:

```nativesharp
try { Parse(); }
catch (ParseError problem) when (problem.IsRecoverable) { Recover(problem); }
catch (Error problem) { Report(problem); }
finally { Close(); }
```

The filter syntax is `when (bool-expression)`. Its catch binding is in scope. A filter can decline
the error, so only an unfiltered clause or literal-`true` filter contributes guaranteed coverage.
A literal-`false` filter is rejected as unreachable. A non-boolean filter is rejected causally.

## Declared-error coverage

- A clause may match only an error declared by a call/throw in its protected region.
- Matching uses NativeSharp class inheritance, not CLR or managed RTTI.
- A catch of the declared type or one of its bases fully covers that error.
- A derived catch can select a runtime subtype but cannot by itself cover a base error contract.
- Filtered clauses record possible matches but do not prove exhaustiveness.
- A later clause is unreachable only when an earlier unfiltered clause handles the same or a base
  type, or when an earlier unfiltered catch-all handles every NativeSharp error.
- Errors not fully covered continue through an enclosing typed try or the callable's `throws E`
  contract. Otherwise NS3071 is emitted.

This analysis is edition-3-only and does not retrofit checked-error rules onto edition 2.

## Cleanup and Windows x64 lowering

`finally`, `defer`, RAII destruction, owned-local cleanup, return, rethrow, and unmatched
propagation share the existing cleanup-region model. The directed runtime fixture proves:

- a filter can accept or decline an error while preserving source order;
- a declined error reaches the next clause;
- rethrow preserves the original native exception for an outer typed catch;
- nested try/catch/finally executes the finally block exactly once on return and rethrow;
- owned objects are destroyed once in reverse construction order during unwind;
- return from inside a finally block remains forbidden.

LLVM uses one Windows x64 catchpad for NativeSharp exceptions, then ordinary parent-side blocks
perform class-descriptor tests and filter expressions in source order. Filter dispatch itself has
no allocator call and carries no CLR machinery. Throwing still uses the native exception-object
contract established by earlier EH work; this item adds no managed runtime dependency.

## IR and cache contract

HIR and MIR catch regions publish the type/wildcard pattern, filter presence, declared error types
that can reach the clause, and cumulative exhaustivity. MIR advances to schema 1.25. These fields
participate in the compilation cache identity so a filter or coverage change cannot reuse an
incompatible cached object.

## Files changed

- `src/Parser/Parser.cpp`: contextual `when (expression)` after a catch declaration.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` and
  `src/Frontend/ProjectMethodBodyModel.cpp`: declared-error collection, ordered reachability,
  filter typing/scope, exhaustive coverage, and propagation to enclosing regions.
- HIR/MIR headers, builders, dumpers, and `ProjectOopMirCacheIdentity.cpp`: versioned catch
  contract and cache identity.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: native source-ordered type tests and runtime filters.
- `tests/unit/NativeSharp3T07ErrorTests.cpp`: positive runtime/IR and causal negative cases.
- T07, language, roadmap, master, and root tracking documentation.

## New files and purpose

- `docs/3.0/items/NS3-T07-F03-typed-catch.md`: atomic implementation record and evidence.
- `docs/3.0/language/typed-catch-finally.md`: bilingual user-facing language contract.

No runtime library, linker component, helper executable, generated source, or dependency was
added.

## Directed evidence

- Exact feature target `nsc_ns3_t07_error_tests`: built successfully.
- Exact feature executable: `PASS: 28 checks`.
- Productive fixture: O0 emission through NSLink and process exit 42.
- LLVM evidence: ordered native RTTI/filter blocks in `Nested`, with no allocator call or CLR text.
- Negative coverage: unrelated type pattern, non-exhaustive derived pattern, non-boolean filter,
  constant-false filter, and return from finally.
- Exact compiler target `nsc`: built successfully after the directed feature validation.

No CTest suite, matrix, baseline, O1/O2/O3 build, installation, deploy, remote operation, or write
to `C:` was performed.
