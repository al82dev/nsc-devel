# L1 - Temporaries for `in` parameters

## Limitation

A NativeSharp `in T` parameter receives the address of its argument. The call site had to write `in` (C4), and
`in e` requires `e` to have an address. A temporary value (a call result, a construction, an operator result, an
indexer that returns by value) therefore could not reach an `in` parameter in either spelling:

- `M(f())` reported "argument N of 'M' must be passed with 'in'";
- `M(in f())` reported that the operand has no address.

## Rule

A temporary reaches an `in T` parameter, written with or without `in`. The call stores the value (after the
argument's conversion to `T`) in a slot of the caller for the duration of the call and passes its address; the
callee can only read it, as for any `in` argument.

- A place written without `in` keeps C4: the keyword is still required, because the callee receives the place
  itself.
- `ref` and `out` still require assignable storage.
- The temporary must need nothing at the end of the call (`inTemporaryValueMaterializable`): no ownership
  qualifier or transfer, no owned storage, no lifetime of its own, not a view, not a class reference or string,
  not move-only; a C++ record must be a transport value (trivial copy and destruction).
- The callee's result must not be able to refer to the argument: a reference return, a lifetime-carrying result
  or a pointer result does not receive a temporary.
- Parameters that already materialize temporaries keep their rules: a C++ `const T&` or `T&&` parameter, and the
  operands of user operators.

## Implementation

- `ExpressionResult::inTemporaryOperand`; `analyzeAddressOf` (`IdentifierAnalyzer.cpp`): `in e` with a
  non-addressable `e` that can be materialized yields `e`'s value with `argumentPassKind = In` and that mark.
- `ProjectMethodBodyModel::inTemporaryValueMaterializable` and `inTemporaryArgumentAllowed`
  (`ConversionAndPassKindChecks.cpp`).
- Candidate evaluation (`evaluateNamespaceFunctionCandidateArgument`, `evaluateCallCandidateArgument` in
  `CallAnalyzer.cpp`): such an argument converts to the referent type, like a value for a C++ `const T&`, and its
  ordinal is recorded in `MethodCallCandidate::inTemporaryArgumentOrdinals`.
- `checkArgumentPassKinds`: no keyword diagnostic for such an argument.
- `MethodCallBinding` / `NamespaceFunctionCallBinding::inTemporaryArgumentSyntaxes`, filled from the selected
  candidate.
- HIR (`HirCallFrameLowering.cpp`, bound, ordinary and namespace calls): such an argument is lowered as its value
  (the operand of a written `in`) and wrapped in `TemporaryAddress` of the parameter's referent type, the node C++
  `const T&` arguments already use.

## Preserved behavior

Arguments that are places, `ref`/`out` arguments, C++ reference parameters and operators are unchanged.

## Validation

NSC built in Release with `/clang:-O3`. No tests were run.

## Pending

- Not verified by compiling programs with each call shape (instance, static, namespace function, extension).
- Return statements and other non-call contexts are not affected by this change.
