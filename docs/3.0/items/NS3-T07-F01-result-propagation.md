# NS3-T07-F01 — Result protocol and propagation

Status: implemented on `development`
Platform: Windows x64
Validation profile: directed O0/NSLink only; no baseline

## Delivered vertical

Postfix `?` is now a real language operation in edition 3. It accepts a structurally compatible,
two-case `variant` whose cases are exactly `Ok(T)` and `Error(E)`. It evaluates the carrier once,
branches through the compiler-owned public variant protocol, yields the `Ok` payload on the normal
edge, and returns an `Error` carrier from the current callable on the other edge.

The source and destination error types must be identical or connected by one declared
`explicit operator`. The compiler never invents a numeric, structural, implicit, or runtime error
conversion. A named move-only carrier must be consumed as `move(result)?`; a temporary result can
be consumed directly.

## Compiler path

- Parser: distinguishes postfix `?` from the existing ternary `?:` at the current nesting level.
- Semantic model: freezes both variant contracts, tags, extractors, return factory, optional
  explicit conversion, ownership transfer, and the structured-return cleanup path.
- HIR: `result-propagation` contains the single carrier operand and all selected protocol members.
- MIR: schema 1.23 publishes the source/return carriers, error types, tags, callees, conversion, and
  `continuation=return`; it contains no synthetic throw or exception edge.
- LLVM: spills the carrier once, calls `IsOk`, extracts only the active payload, converts the error
  when required, constructs the destination `Error`, and reuses the ordinary structured-return
  lowering so `defer`, owned locals, catch state, and other lexical cleanup are preserved.
- Code partitioning: every protocol callee is an explicit dependency of the code unit that uses it.

The generated variant factories and extractors now carry compiler-only transfer markers. These
allow their generated `move(...)` operations to copy trivial payloads while still moving and
clearing payloads/carriers that require drop. User-written `move(...)` rules were not relaxed.

## Files changed

- `include/nativesharp/AST/AstNodeKind.h`
- `include/nativesharp/Parser/Parser.h`
- `src/AST/AstNodeKind.cpp`
- `src/Parser/Parser.cpp`
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`
- `src/Frontend/ProjectMethodBodyModel.cpp`
- `include/nativesharp/Frontend/ProjectOopHirModel.h`
- `src/Frontend/ProjectOopHirModel.cpp`
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
- `src/Frontend/ProjectOopMirModel.cpp`
- `src/Frontend/ProjectOopMirModelDumper.cpp`
- `src/LLVM/ProjectOopMirCacheIdentity.cpp`
- `src/LLVM/ProjectOopCodePartition.cpp`
- `src/LLVM/ProjectOopLlvmLowerer.cpp`
- `tests/unit/NativeSharp3T07ErrorTests.cpp`
- `CMakeLists.txt`

## Directed evidence

- Exact compiler target `nsc`: built successfully.
- Exact feature target `nsc_ns3_t07_error_tests`: built successfully.
- Exact feature executable: `PASS: 11 checks`.
- Productive fixture: O0 emission through NSLink and process exit 42.
- Covered in that fixture: Ok continuation, Error early return, declared explicit conversion,
  `defer` on both paths, single evaluation, ternary preservation, and MIR 1.23 publication.

No CTest suite, matrix, baseline, O1/O2/O3 build, installation, deploy, or write outside `A:` was
performed.
