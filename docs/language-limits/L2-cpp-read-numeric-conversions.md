# L2 - Numeric conversions of values read from C/C++ storage

## Limitation

A value returned by a C/C++ function converts to a NativeSharp arithmetic type as C does (`ForeignNumeric`,
`classifyForeignCallNumericResult`): `float f = cFunctionReturningDouble();` is accepted. A value read from C/C++
storage was not: `float f = cRecord->doubleField;`, `int n = cRecord->unsignedCount;` or an element of a C array
were rejected with NativeSharp's ordinary rules, although the value comes from the same C declarations.

## Rule

A read of C/C++ storage converts like a C/C++ call result: a field of an imported record, an element of C/C++
storage, an imported global or enumerator, and the pointee of a C pointer. The analysis already marks those
values with `cppCOrigin` (T08-D11/D12), and the expression binding preserves it for HIR.

Values computed by NativeSharp (a local, a NativeSharp field, arithmetic on C values) keep NativeSharp's
conversions; `cppCOrigin` is not propagated by NativeSharp operators.

## Implementation

`cppForeignCallResult || cppCOrigin` gates the C conversion in:

- `ProjectMethodBodyModel::classifyImportedFlexibleConversion` (`ConversionAndPassKindChecks.cpp`), used by local
  initializers, assignments, arguments and returns in the body analysis;
- `ProjectOopHirModel::classifyExpressionConversion` (`HirConversion.cpp`) and `convertConditionalBranch`
  (`HirExpressionMachine.cpp`), which build the conversion nodes;
- constructor argument evaluation (`ConstructorCandidateEvaluation.cpp`).

## Preserved behavior

Conversions between NativeSharp values, and C/C++ call results, are unchanged. A conversion that NativeSharp
already allows is still chosen first (the C conversion is only tried when none exists).

## Validation

NSC built in Release with `/clang:-O3`. No tests were run.

## Pending

Not verified by compiling programs. Overload resolution may now find a NativeSharp candidate viable for a C/C++
storage argument where it was not before (rank 5, as for call results).
