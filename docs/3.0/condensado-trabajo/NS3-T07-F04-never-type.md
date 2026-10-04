# NS3-T07-F04 — Diverging expressions and `never`

Status: implemented on `development`
Platform: Windows x64
Validation profile: directed O0/NSLink only; no baseline

## Delivered contract

`builtin:never` is NativeSharp 3's bottom type. A divergent expression converts implicitly to
every result type with the dedicated `Never` conversion kind. The conversion participates in
overload resolution, explicit generic instantiations, conditional inference, local inference,
returns, arguments, assignments, boolean conditions, and match arms. It never manufactures a
runtime value.

`panic()` is the current productive source of `never`. `while (true)`, `do ... while (true)`,
`for (;;)` and `for (; true; ...)` are proven divergent when they have no reachable `break`.
Unreachable source is still bound and checked in a disposable flow probe, so divergence cannot
hide invalid names, conversions, ownership violations, or definite-assignment errors.

## Inference and overloads

- `condition ? panic() : value` has the type and ownership of the reachable value branch.
- Two divergent conditional arms produce `builtin:never`.
- A `never` argument is viable for every parameter type. Normal target-specificity rules still
  choose a strictly better target; unrelated equally good targets remain ambiguous.
- Explicit generic arguments establish the destination type before the bottom conversion.
- An inferred `var` initialized by `panic()` has semantic type `builtin:never`, but no storage or
  ABI layout is created.

The present `match` statement accepts divergent arms and preserves exhaustiveness. The future
value-producing `match` expression remains owned by NS3-T03-F03; F04 does not claim that separate
syntax as implemented.

## Flow and LLVM

HIR erases the physical `never -> T` conversion and preserves the divergent node. LLVM emits the
trap and `unreachable`, then excludes that path from conditional/coalescing PHIs, returns, stores,
calls, unary/binary operations and structured exits. A `never` local receives no `alloca`, debug
lifetime, drop flag, store, or placeholder bit. Proven infinite loops expose only explicit break
states to the semantic successor.

## Files changed

- `include/nativesharp/Frontend/ProjectTypeConversion.h` and
  `src/Frontend/ProjectTypeConversion.cpp`: stable `Never` conversion kind and bottom conversion.
- `src/Frontend/ProjectMethodBodyModel.cpp`: bottom-aware conditions/conditional inference,
  divergent panic flow, and proven infinite-loop successors.
- `src/Frontend/ProjectOopHirModel.cpp`: semantic-only erasure of bottom conversions.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: divergence-aware control/value lowering and no-layout
  locals.
- `tests/unit/NativeSharp3T07ErrorTests.cpp`: production classifier, inference, overload,
  constrained generic, match, loop, dead-code, MIR, LLVM and O0/NSLink evidence.
- T07, language, roadmap, master and root tracking documentation.

## Directed evidence

- Exact feature target `nsc_ns3_t07_error_tests`: built successfully.
- Exact feature executable: `PASS: 34 checks`.
- Productive O0 executable linked by NSLink exits with 42.
- LLVM extraction for `NeverLocal` contains the native trap and `unreachable`, with no `alloca`
  or fabricated boolean store.
- An overload set with unrelated parameter types rejects `panic()` as ambiguous.
- Code after `panic()` remains semantically checked and reports use of an unassigned local.

No CTest suite, matrix, baseline, O1/O2/O3 build, installation, deploy, remote operation, or write
to `C:` was performed.
