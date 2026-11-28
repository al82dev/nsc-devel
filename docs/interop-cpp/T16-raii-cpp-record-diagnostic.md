# T16 - Diagnostic for an ownership qualifier on a C++ record local

## Defect

`raii T x = new T();` with `T` an imported C++ record reported that a raii local requires a NativeSharp class or
interface reference, a managed delegate, a native string or a native array buffer. The text lists what may be
owned but not why a C++ record is not one of them.

## Change

`validateVariableLifetimeQualifiers` (`StatementAnalyzer.cpp`): when the local's type is an imported C++ record,
the text says that a local of C++ record type holds the object itself, that the end of its scope destroys it, and
that the ownership qualifier should be removed.

## Preserved behavior

Only the text changes; the same declarations are rejected.

## Validation

NSC built in Release with `/clang:-O3`. No tests were run.
