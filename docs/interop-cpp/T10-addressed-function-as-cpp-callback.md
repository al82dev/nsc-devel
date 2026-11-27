# T10 - `&F` as a C/C++ callback argument

## Defect

Passing the address of a NativeSharp function where a C function pointer is expected, through a C function
pointer call (`glDebugMessageCallbackARB(&DebugOutputCallback, null)`: a GLEW entry point is a variable of type
`PFNGLDEBUGMESSAGECALLBACKARBPROC` whose first parameter is the callback type `GLDEBUGPROCARB`), failed in HIR:

```text
Stage 34 method-group expression can only appear as call target
```

## Causes

1. HIR classification (`ProjectOopHirModel::iterativeStorageKind`,
   `src/Frontend/HIR/ExpressionStatementMachine/HirExpressionClassifier.cpp`). Since b87dd2dba the analysis lets
   `&F` yield the method group of F where a function-pointer or delegate parameter can receive it
   (`analyzeUnary`), and the call binds it with `bindMethodGroupToDelegate` on the `&` syntax: a
   `MethodDelegateBinding` of kind `Create`, the same binding `F` written without `&` gets. The iterative
   classifier, however, classified every `&` in a value context as `StorageUnary`, the address of storage, before
   any delegate binding was looked at (`iterativeDelegateAndCallKind` only takes delegate creations with a
   receiver). The operand `F` was then lowered as a name and reached `lowerBoundNameExpression`, which rejects a
   method group.
2. Analysis of C function pointer and delegate calls (`ProjectMethodBodyModel` call argument analysis in
   `CallAnalyzer.cpp`, L-036). A method group argument was bound only after
   `methodGroupExactlyMatchesDelegate` found exactly one match; with none (or several) the argument stayed a method
   group with no diagnostic (`allArgumentsValid = false` only), so an incompatible callback also ended in the same
   HIR error instead of a signature diagnostic. Direct calls (`bindNamespaceFunctionMethodGroupArgument`) and
   method calls already reported it.

## Change

- `iterativeStorageKind`: `&` whose syntax has a delegate binding of kind `Create` is not `StorageUnary`; the
  expression is lowered by `lowerExpressionImpl`, which builds the `DelegateCreate` node from the binding
  (`lowerDelegateCreationExpressionImplCase`, `finishDelegateCreation`) exactly as for `F` without `&`.
- Call argument analysis: for a call through a delegate or C function pointer, a method group argument for a
  parameter is always passed to `bindMethodGroupToDelegate`, which selects the method whose parameter types,
  return type, pass kinds, volatile qualifiers, ownership and target validity (`validateDelegateMethodTarget`) the
  parameter accepts, and reports "no method in the method group exactly matches delegate signature ...", an
  ambiguous conversion, or a parameter that is not a function type.

Code generation is the existing one for a method group bound to a structural C function type: `DelegateCreate`
with the selected callable, lowered to the callback thunk of that convention (Item 7.1, T08-F02); no new lowering.

## Preserved behavior

- `&x` of a local, field, element or global has no delegate binding and keeps `StorageUnary`.
- `F` without `&`, delegate creations with receivers and direct calls are unchanged.
- No method group becomes a pointer without a delegate or function-pointer parameter: the analysis only lets `&F`
  be a method group where such a parameter exists, and the binding checks the signature.
- NativeSharp calls keep their rules.

## Pending verification

- No tests, baselines or probes were run, as requested. Pending: the GLEW `glDebugMessageCallbackARB(&F, null)`
  case, a direct C call with `&F`, and the new diagnostics for an incompatible or ambiguous callback through a C
  function pointer.

## Validation

- NSC built in Release with `/clang:-O3` (`ninja -C build-clang nsc`, exit code 0). The build is not a functional
  proof.
