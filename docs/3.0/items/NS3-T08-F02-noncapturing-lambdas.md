# NS3-T08-F02 — Non-capturing lambdas

Status: implemented on `development`

Platform: Windows x64

Validation profile: directed O0/NSLink only; no baseline

## Delivered contract

- A contextual non-capturing lambda converts to a raw `fn*` with the target signature and calling
  convention.
- An unconverted expression-bodied lambda with explicitly typed parameters receives a distinct
  anonymous nominal type for each source expression.
- The nominal value is zero-size/alignment-one, allocates no heap or delegate cell, and direct
  invocation lowers to its generated static body.
- It converts later only to a compatible raw `fn*`; an incompatible ABI or managed `fn&` is rejected.
- Overload selection, generic contextual inference, compile-time preprocessing in the body, static
  `fn*` storage, direct code generation, and `sizeof == 0` are exercised.

The normative bilingual contract is in `docs/3.0/language/noncapturing-lambdas.md`.

## Implementation changes

- `CallableSemantics` owns strict anonymous-lambda identity parsing and stable generated names.
- method-body binding can infer a standalone lambda signature, rejects captures and ambiguous
  untyped/block-bodied standalone forms, and records direct-call identity.
- layout reports `{size=0, alignment=1}` for the anonymous nominal value.
- type conversion admits only signature-compatible raw function pointers.
- HIR/MIR bypass delegate construction for direct invocation and publish a `direct-static-call`.
- LLVM represents the nominal value as an empty struct and materializes the existing guarded thunk
  only when conversion to a raw pointer is requested.

## Files changed

- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `include/nativesharp/Language/CallableSemantics.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `src/Frontend/ProjectObjectLayoutModel.cpp`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Frontend/ProjectTypeConversion.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `src/Language/CallableSemantics.cpp`
- `tests/unit/NativeSharp3T08CallableTests.cpp`
- T08 roadmap and closure documents.

## New files and why

- `docs/3.0/language/noncapturing-lambdas.md`: normative ES/EN source and ABI contract.
- `docs/3.0/items/NS3-T08-F02-noncapturing-lambdas.md`: atomic implementation record and evidence.

No runtime component, library, generated source tree, or persistent temporary directory was created.

## Concrete improvement

Previously every lambda required a target delegate type, so `var f = (int x) => ...` was rejected and
even a non-capturing callable followed delegate construction semantics. Edition 3 now preserves the
zero-cost nominal lambda until conversion is actually requested, emits a direct static call for local
invocation, and exposes no hidden allocation.

## Directed evidence

- Exact target `nsc_ns3_t08_callable_tests`: built successfully.
- Exact feature executable: `PASS: 22 checks`.
- The O0 program linked by NSLink executes direct invocation, later `fn*` conversion, a C callback,
  overload selection, generic contextual inference, `#run`, static field storage, and zero size.
- MIR contains `direct-static-call` and the generated anonymous-lambda callee.
- Negative compilation rejects captured and untyped standalone anonymous lambdas.
- Pure conversion checks reject incompatible calling convention and `fn&` carrier.
- Exact `nsc` target builds successfully with the static host-runtime contract.

No CTest suite, baseline, O1/O2/O3 build, installation, deploy, remote operation, or write to `C:`
was performed.

## Deliberate boundaries

Static locals are not part of F02: edition 4 owns their once-init, reentrancy, destruction, and
module-lifetime semantics. F02 validates static storage through a typed static field. Generic-lambda
syntax remains T08-F04; F02 covers a regular lambda receiving context from a generic method.
