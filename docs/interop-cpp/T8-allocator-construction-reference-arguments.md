# T8 - Reference arguments of a C++ constructor in `new (allocator) T(...)`

## Defect

For an allocator construction, `acceptAllocatorConstructionChild` (`HirConstructionFrameLowering.cpp`) only
wrapped each argument in its selected conversion. The analysis binds a value to a C++ constructor's `const T&`
(X1-04, T3) or `T&&` (T19-007) parameter, whose LLVM parameter is a pointer, so the placement construction would
have passed the value where the constructor expects its address. The ordinary construction path materializes it.

## Cause

The materialization of `acceptOrdinaryConstructionArgument` (a `TemporaryAddress` around the argument) was written
inline in the ordinary path only.

## Change

- `ProjectOopHirModel::materializeCppConstructionReferenceArgument`: the ordinary path's rule moved into one
  function, unchanged: a `T&&` value, a non-record `const T&` value, or a record whose HIR type is the referent of a
  `const T&`, written without `pass:` (`ref`/`in`/`out`), is stored in a `TemporaryAddress` of the referent type. A
  parameter pack is never materialized.
- `acceptOrdinaryConstructionArgument` calls it where the inline code was.
- `acceptAllocatorConstructionChild` calls it after the conversion, so `PlacementConstruction` passes the address.

Code generation: MIR lowers `PlacementConstruction` to `PlacementConstruct` with the constructor's `calleeName`
from `functionForMember`, as `ValueConstruct`; `invokePlacementConstructor` calls it with the storage first and the
ordered arguments, the same shape `lowerValueConstruction` uses for a C++ constructor.

Lifetime: the temporary-ownership pass of `HirExceptionAndCleanup.cpp` indexes every node of the function
(`indexHirParents`) and selects a `TemporaryAddress` over a C++ construction or constructed result by the node
itself, so a non-trivial prvalue argument is owned and destroyed at the end of the full expression as in the
ordinary path. A scalar or trivially copyable value is a stack temporary of the function. Read in the code; not
observed in generated code.

## Preserved behavior

- Ordinary constructions: identical rule and node.
- Allocator constructions of NativeSharp types, and arguments that need no materialization (by-value parameters,
  pointers, `ref`/`in`/`out`): unchanged.
- The allocator path still does not add the ordinary path's owned-transfer and fresh-borrow wrappers, as before.

## Pending verification

- No tests, baselines or probes were run, as requested. Pending: an allocator construction of a C++ record with a
  `const T&` parameter; whether such constructions are otherwise supported end to end was not checked.

## Validation

- NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`, exit code 0). The build is not a functional
  proof.
