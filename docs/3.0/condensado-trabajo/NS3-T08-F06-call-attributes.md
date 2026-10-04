# NS3-T08-F06 — Tail calls and call attributes

## Status

Implemented on `development`. The directed gate passes 6/6 checks at `-O0` using NSLink. No baseline or broad suite was executed.

## Source contract

NativeSharp 3.0 introduces a strong call-site form:

```nativesharp
must_tail return Next(argument);
```

`must_tail` is not an optimization suggestion. Compilation fails unless the return consists of one statically resolved call that can be emitted as a required Win64 tail call.

The existing declaration modifiers remain hints or policies:

- `inline` requests inlining without changing program meaning;
- `forceinline` is the existing required inlining policy;
- `noinline` preserves an out-of-line boundary;
- none of these modifiers implies `must_tail`, and `must_tail` does not imply inlining.

## Validation

A required tail call is rejected when:

- the returned expression is not a direct function or method call;
- overload resolution does not identify one callable;
- caller and callee differ in parameter carriers, result, receiver channel, calling convention, typed-error contract, lifetime or variadic shape;
- a result conversion would have to execute after the call;
- an RAII/value-drop/using/finally cleanup remains pending;
- the current lowering would require an indirect aggregate return slot;
- the call would lower as an exception `invoke` instead of a call instruction.

Closed generic specializations are validated using their substituted parameter and result types, so generic recursion receives the same ABI check as a non-generic call.

## Intermediate and backend contract

- The parser stores the requirement on the return statement and generic cloning preserves it.
- Semantic analysis reuses `validateMustTail` after overload resolution.
- HIR and MIR carry `must-tail=true` only after validation.
- MIR schema 1.30 publishes the call-site requirement; the compilation cache hashes it.
- LLVM lowering emits `musttail call`, never a plain `tail` hint.
- The call remains immediately followed by the ABI return because pending cleanup and post-call conversions were rejected earlier.

## Files

- `src/Parser/Parser.cpp`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `src/Frontend/ProjectOopHirModel.cpp`
- `src/Frontend/ProjectOopHirModelDumper.cpp`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Frontend/ProjectOopMirModelDumper.cpp`
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`
- `src/LLVM/LlvmCApi.h`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `tests/unit/NativeSharp3T08TailCallTests.cpp`
- current-schema expectations in the focused MIR tests
- `CMakeLists.txt`

## Directed evidence

Target: `nsc_ns3_t08_tail_call_tests`

- MIR publishes `must-tail=true`;
- LLVM contains `musttail call` while preserving independent `inlinehint` and `noinline` attributes;
- ordinary and closed-generic recursion execute through NSLink and return 42;
- a pending destructor is rejected;
- an incompatible Win64 signature is rejected;
- a non-call return is rejected rather than silently degraded.

Result: `PASS: 6 checks` with `-O0` and `A:\nslink-devel\build-debug\bin\nslink.exe`.
