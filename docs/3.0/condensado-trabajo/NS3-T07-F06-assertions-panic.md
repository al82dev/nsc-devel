# NS3-T07-F06 — Assertions and panic profiles

Status: implemented on `development`
Platform: Windows x64
Validation profile: directed O0/NSLink only; no baseline

## Delivered contract

NativeSharp 3 recognizes the contextual intrinsics `assert`, `unreachable`, `assume`, and
`panic`. They are represented explicitly through semantic binding, HIR, MIR 1.28, and LLVM; none
is rewritten as a user function call or a managed exception.

- `assert(bool)` returns `void`. A false condition terminates through trap reason 15.
- `unreachable()` returns `never` and terminates through trap reason 16.
- `assume(bool)` returns `void`, is legal only in an explicit `unsafe` context, and lowers to
  `llvm.assume`. A false premise is undefined behavior rather than a checked assertion.
- `panic()` and `panic(string)` return `never`. Other arities and payload types are rejected.

## Panic profiles

`<PanicProfile>abort</PanicProfile>` is the default. It invokes the optional handler and then the
stable panic trap (reason 14, process exit 142). It does not run lexical cleanup.

`<PanicProfile>unwind-native</PanicProfile>` raises the dedicated NativeSharp Win64 SEH code. MIR
publishes the panic nonlocal exit and its cleanup path; LLVM emits native exceptional cleanup for
active `defer`, RAII, and finally regions. Typed language catches do not consume panic. The
top-level entry boundary and generated foreign-call boundaries use the dedicated runtime filter so
panic cannot escape silently into foreign code.

The runtime contract is ABI 1.2 / symbol set 4 and publishes
`__nativesharp_panic_eh_filter`. The compiler-generated entry point only declares that symbol;
the static Windows runtime library owns its single definition.

## Custom handler

A project may declare:

```xml
<PanicHandler>Diagnostics.OnPanic</PanicHandler>
```

The name must resolve to a non-throwing namespace function with the exact signature:

```nativesharp
void OnPanic(string message, string file, int line, int column)
```

The handler runs before either abort or native unwind. It receives the optional payload (null when
absent) and the compiler source path/position. Returning continues with the selected profile; the
handler may terminate explicitly. Its physical function is added to every LLVM code partition
that contains a call to `panic`, preventing partition-dependent missing declarations.

## Project-system integrity

`PanicProfile` and `PanicHandler` survive graph normalization, runtime-library injection,
source-preferred rewiring, and precompiled-closure expansion. Invalid profiles, duplicate
properties, missing handlers, and invalid handler signatures fail before object emission.

## Files changed

- Project loading/model/graph files: parse and preserve both panic properties.
- Parser, method-body model, HIR, and MIR files: contextual syntax, typing, divergence, cleanup,
  payload, profile, handler resolution, and MIR 1.28 publication.
- LLVM lowering and code partitioning: checks, traps, `llvm.assume`, handler calls, native panic
  transfer, cleanup, and cross-partition declarations.
- Runtime builder and ABI contract: panic SEH filter, top-level boundary, ABI 1.2/symbol set 4.
- `tests/unit/NativeSharp3T07ErrorTests.cpp`: productive execution, MIR, and rejection evidence.

## Directed evidence

- Exact target `nsc_ns3_t07_error_tests`: built successfully.
- Exact feature executable: `PASS: 54 checks`.
- O0 programs linked by NSLink prove successful assertions, assertion failure, native-unwind
  cleanup, and custom-handler payload/source delivery.
- Negative cases cover unsafe enforcement, condition/payload types, intrinsic arity, unknown
  profiles, and invalid handler signatures.

No CTest suite, baseline, O1/O2/O3 build, installation, deploy, remote operation, or write to `C:`
was performed.
