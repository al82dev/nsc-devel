# T3 - C++ record arguments for `const T&` parameters of imported C++ constructors

## Defect

An imported C++ constructor rejected a C++ record argument written without `in` for a `const T&` parameter:
`new glm.mat3(ModelViewMatrix)` with a `glm.mat4` local and GLM's `mat(mat<4, 4, T, Q> const& x)`. The same
record bound a `const T&` parameter of a C++ function (L-020, X2-05).

## Cause

`ProjectConstructionModel::evaluateConstructorArgument`
(`src/Frontend/ConstructionModel/ConstructorCandidateEvaluation.cpp`) bound a value to a `const T&` only when T
is not a record (`constReferenceValue`, X1-04). For a record T the argument was classified against the
parameter's own type key (`mat<4,4,float,0>*`), and `classifyImplicit(mat, mat*)` does not exist, so no
constructor was viable. `ProjectMethodBodyModel::evaluateNamespaceFunctionCandidateArgument` has the record
rules (a prvalue by X2-05, a place of a trivially copyable record by L-020); the constructor path never got them.
`acceptOrdinaryConstructionArgument` (`HirConstructionFrameLowering.cpp`) likewise materialized only non-record
values.

## Change

- `evaluateConstructorArgument`: `constReferenceRecord` binds a C++ record argument passed by value to a
  `const T&` whose referent is a record, as the call path does: a prvalue always (it is materialized for the
  construction, with no copy), and a place only when `ProjectNameLookup::cppTransportValueEligible` holds for the
  record (Item 10.1, trivially copyable and destructible; the bitwise copy materialized for the construction is
  observably the object the constructor reads). The conversion target is the referent type.
- `acceptOrdinaryConstructionArgument`: an argument of such a parameter whose HIR type is the referent (not the
  parameter's pointer type) is stored in a `TemporaryAddress`; the constructor receives its address. The
  temporary-ownership pass of `HirExceptionAndCleanup.cpp` selects a `TemporaryAddress` over a C++ construction or
  constructed result by the node itself, not by its parent, so it should own and destroy such a prvalue as it does
  for a call argument. This was read in the code, not observed in generated code.

## Preserved behavior

- A place of a record that is not trivially copyable still binds only with `in` (X2-04).
- `in`, `ref` and `out` arguments (`pass:` syntax) are not materialized; a pointer argument whose type is the
  parameter's type is not materialized either.
- Non-record `const T&` (X1-04), `T&&` (T19-007), NativeSharp constructors and calls: unchanged.

## `in new T(...)` for a `const T&`

Checked separately: it does not share this cause. `in` is analyzed as taking the address of its operand
(`ProjectMethodBodyModel::analyzeAddressOf`, `IdentifierAnalyzer.cpp`), and a construction is not addressable, so
the argument fails before any candidate is evaluated, with the generic "bit fields have no address" text. That is
the argument-passing analysis, outside this correction, and it is only documented here. With this change the
prvalue binds when written without `in` (`new glm.mat3(new glm.mat4(...))`).

## Validation

- NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`, exit code 0).
- No tests, baselines or probes were run, as requested; the build is not a functional proof. Pending: compiling
  the tutorials that construct a `mat3` from a `mat4`.
