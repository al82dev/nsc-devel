# T11 - Numeric diagnostics near C calls: findings, no change

Two kinds of diagnostics were reported while compiling a port that calls C functions. Neither comes from a C/C++
boundary conversion; both are NativeSharp's ordinary arithmetic rules. No code was changed: the decision is the
project owner's.

## 1. `binary operator requires compatible numeric operands in Stage 34`

Sites (all are arguments of a C function, but the failing operation is the NativeSharp multiplication inside):

- `MaxParticles * 4 * sizeof(GLfloat)` and `MaxParticles * 4 * sizeof(GLubyte)`, with `const int MaxParticles`
  declared in NativeSharp.
- `ParticlesCount * sizeof(GLfloat) * 4` and `ParticlesCount * sizeof(GLubyte) * 4`, with an `int` local.

Types and provenance:

- `sizeof(T)` is `builtin:nuint` for every type (`IndexAndRangeExpressions.cpp`, `SizeofConstant` binding,
  `result.typeKey = "builtin:nuint"`). It carries an integer constant only for a C/C++ value layout fixed by
  Clang and for builtin integer types (T19-002); `GLfloat` is `float`, so `sizeof(GLfloat)` is a non-constant
  `nuint`, while `sizeof(GLubyte)` is the constant 1.
- `MaxParticles` is a `const` namespace state with an integer constant (`IdentifierAnalyzer.cpp`, namespace state
  identifiers). `MaxParticles * 4` is analyzed by `ProjectMethodBodyModel::analyzeArithmeticBinary`
  (`OperatorExpressionAnalyzer.cpp`), which sets the result type (`int`) and does not set `integerConstant` on the
  result, so the product is no longer a constant expression for the next operator. (Read in the code; not observed
  in a dump.)
- `ParticlesCount` is a runtime `int`.

Rule applied: `ProjectTypeConversion::commonNumericType` (`ProjectTypeConversion.cpp`). For `int` and `nuint` a
common type exists only when the `int` operand is a constant that fits (`constantAdaptsUpward`, C#'s implicit
constant expression conversion); otherwise `integerPromotion` gives `nuint`, which `int` does not reach, and no
widening alternative exists, so the pair is rejected, as C# rejects `int * nuint`. This is a deliberate rule of
NativeSharp's integer arithmetic (IDs 120/126 in that function), not an interop conversion.

Possible decisions, none implemented: fold the integer constants of binary arithmetic so `MaxParticles * 4` stays
a constant; give `sizeof` of a fixed-size builtin floating type its constant; or keep the rule (the source then
writes the conversion it means).

## 2. `compound assignment cannot implicitly convert its right-hand side from 'builtin:double' to 'builtin:float'`

Site: `p->life -= delta;` where `p` points to a NativeSharp struct whose field `life` is `float` and `delta` is a
NativeSharp `double` local (initialized from a C call result, `glfwGetTime()`, but declared `double`).

Both operands are NativeSharp values; no C/C++ declaration is involved in the assignment. NativeSharp rejects the
implicit narrowing on purpose (the message asks for an explicit cast), as C# does for `float -= double`. Not an
interop defect.

## Validation

Documentation only; nothing was built or run for this finding.
