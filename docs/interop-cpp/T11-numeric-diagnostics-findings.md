# T11 - Numeric diagnostics near C calls: findings, no change

Two kinds of diagnostics were reported while compiling a port that calls C functions. Neither comes from a C/C++
boundary conversion; both are NativeSharp's ordinary arithmetic rules. No code was changed: the decision is the
project owner's.

## 1. `binary operator requires compatible numeric operands in Stage 34`

Six sites, all arguments of a C function; the failing operation is a NativeSharp multiplication inside each:

| Line | Expression | Left operand of the failing `*` | Right operand |
|---|---|---|---|
| 91, 159 | `MaxParticles * 4 * sizeof(GLfloat)` | `MaxParticles * 4`: `int`, no constant | `sizeof(GLfloat)`: `nuint`, no constant |
| 95, 162 | `MaxParticles * 4 * sizeof(GLubyte)` | `MaxParticles * 4`: `int`, no constant | `sizeof(GLubyte)`: `nuint`, constant 1 |
| 160 | `ParticlesCount * sizeof(GLfloat) * 4` | `ParticlesCount`: runtime `int` | `sizeof(GLfloat)`: `nuint`, no constant |
| 163 | `ParticlesCount * sizeof(GLubyte) * 4` | `ParticlesCount`: runtime `int` | `sizeof(GLubyte)`: `nuint`, constant 1 |

Types and provenance:

- `sizeof(T)` is `builtin:nuint` for every type (`IndexAndRangeExpressions.cpp`, `SizeofConstant` binding,
  `result.typeKey = "builtin:nuint"`). It carries an integer constant only for a C/C++ value layout fixed by
  Clang and for builtin integer types (T19-002). `GLfloat` is `float`, a builtin floating type, so
  `sizeof(GLfloat)` is a non-constant `nuint`; `GLubyte` is a builtin integer, so `sizeof(GLubyte)` is the
  constant 1.
- `MaxParticles` is a NativeSharp `const int` namespace state with an integer constant (`IdentifierAnalyzer.cpp`).
  `MaxParticles * 4` is analyzed by `ProjectMethodBodyModel::analyzeArithmeticBinary`
  (`OperatorExpressionAnalyzer.cpp`), which sets the result type (`int`) but never sets `integerConstant` on the
  result, so the product is no longer a constant for the next operator. The compiler does have a constant
  evaluator (`ProjectIntegerEvaluator`, used through `foldIntegerConstant` for constant declarations, case labels
  and similar positions), but expression analysis does not feed it into `ExpressionResult::integerConstant`.
  (Read in the code; not observed in a dump.)
- `ParticlesCount` is a runtime `int` local.

Rule applied: `ProjectTypeConversion::commonNumericType` (`ProjectTypeConversion.cpp`). For `int` and `nuint` a
common type exists only when the `int` operand is a constant that fits (`constantAdaptsUpward`, C#'s implicit
constant expression conversion, which applies to `int` constants only). Otherwise `integerPromotion` gives
`nuint`, which a non-constant `int` does not reach (`int -> nuint` is not an implicit conversion), the `ulong`
fallback fails for the same reason, and the pair is rejected. A `nuint` constant does not adapt to `int`. This is a
deliberate rule of NativeSharp's integer arithmetic (IDs 120/126 in that function), not an interop conversion.

How the source differs from C# and C:

- In C#, `sizeof(float)` is an `int` constant and `MaxParticles * 4` is a constant expression, so all six
  expressions are `int`.
- In C, `sizeof` is `size_t` and the usual arithmetic conversions turn `int * size_t` into `size_t`, so all six
  are `size_t`.
- NativeSharp takes `size_t`'s type for `sizeof` and C#'s rule for mixing signed and unsigned operands, so neither
  route applies.

Language decisions that would be needed (none implemented, none required by interop):

1. Constant folding of binary integer arithmetic in expression analysis, so `MaxParticles * 4` keeps its constant.
   With the existing upward rule this alone accepts lines 91, 95, 159 and 162 (`int` constant 400000 adapts to
   `nuint`).
2. A constant `sizeof` for builtin floating types (their size is fixed by the language as for integers). Alone it
   changes nothing in these lines: it gives `nuint` constants, which do not adapt to `int`.
3. For lines 160 and 163 (runtime `int` times `nuint`), one of: let a non-negative `nuint` constant adapt to the
   other operand's type (a new direction of the constant rule, needing 2 for line 160); make `sizeof` an `int`
   constant as in C#; or accept `int * nuint` as C does. Each of these changes the signed/unsigned rule of the
   language, not of interop.
4. Keep the rule: the source writes the conversion it means (`(nuint)MaxParticles`, `(nuint)ParticlesCount`).

## 2. `compound assignment cannot implicitly convert its right-hand side from 'builtin:double' to 'builtin:float'`

Site: `p->life -= delta;` where `p` points to a NativeSharp struct whose field `life` is `float` and `delta` is a
NativeSharp `double` local (initialized from a C call result, `glfwGetTime()`, but declared `double`).

Both operands are NativeSharp values; no C/C++ declaration is involved in the assignment. NativeSharp rejects the
implicit narrowing on purpose (the message asks for an explicit cast), as C# does for `float -= double`. Not an
interop defect.

## Validation

Documentation only; nothing was built or run for this finding.
