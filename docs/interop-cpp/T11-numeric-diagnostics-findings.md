# T11 - Numeric diagnostics near C calls: findings, no change

Two kinds of diagnostics appear in NativeSharp arithmetic written as an argument of a C function. Neither comes
from a C/C++ boundary conversion; both are NativeSharp's ordinary arithmetic rules. No code was changed: the
decision belongs to the language.

## 1. `binary operator requires compatible numeric operands in Stage 34`

Construction: an `int` operand multiplied by `sizeof(T)`, for example `N * 4 * sizeof(T)` or `count * sizeof(T)`,
where the product is the argument of a C function. Four operand shapes reach the failing `*`:

| Left operand | Right operand | Result |
|---|---|---|
| `K * 4` with `K` a `const int` | `sizeof(float)`: `nuint`, no constant | rejected |
| `K * 4` with `K` a `const int` | `sizeof(T)` of an 8-bit integer: `nuint`, constant 1 | rejected |
| runtime `int` | `sizeof(float)`: `nuint`, no constant | rejected |
| runtime `int` | `sizeof(T)` of an 8-bit integer: `nuint`, constant 1 | rejected |

Types and provenance:

- `sizeof(T)` is `builtin:nuint` for every type (`IndexAndRangeExpressions.cpp`, `SizeofConstant` binding,
  `result.typeKey = "builtin:nuint"`). It carries an integer constant only for a C/C++ value layout fixed by
  Clang and for builtin integer types (T19-002). For a builtin floating type it is a non-constant `nuint`.
- A NativeSharp `const int` namespace state has an integer constant (`IdentifierAnalyzer.cpp`). `K * 4` is
  analyzed by `ProjectMethodBodyModel::analyzeArithmeticBinary` (`OperatorExpressionAnalyzer.cpp`), which sets the
  result type (`int`) but never sets `integerConstant` on the result, so the product is no longer a constant for
  the next operator. The compiler has a constant evaluator (`ProjectIntegerEvaluator`, used through
  `foldIntegerConstant` for constant declarations, case labels and similar positions), but expression analysis
  does not feed it into `ExpressionResult::integerConstant`. (Read in the code; not observed in a dump.)

Rule applied: `ProjectTypeConversion::commonNumericType` (`ProjectTypeConversion.cpp`). For `int` and `nuint` a
common type exists only when the `int` operand is a constant that fits (`constantAdaptsUpward`, C#'s implicit
constant expression conversion, which applies to `int` constants only). Otherwise `integerPromotion` gives
`nuint`, which a non-constant `int` does not reach (`int -> nuint` is not an implicit conversion), the `ulong`
fallback fails for the same reason, and the pair is rejected. A `nuint` constant does not adapt to `int`. This is a
deliberate rule of NativeSharp's integer arithmetic (IDs 120/126 in that function), not an interop conversion.

How the construction differs from C# and C:

- In C#, `sizeof(float)` is an `int` constant and `K * 4` is a constant expression, so the product is `int`.
- In C, `sizeof` is `size_t` and the usual arithmetic conversions turn `int * size_t` into `size_t`.
- NativeSharp takes `size_t`'s type for `sizeof` and C#'s rule for mixing signed and unsigned operands, so neither
  route applies.

Language decisions that would be needed (none implemented, none required by interop):

1. Constant folding of binary integer arithmetic in expression analysis, so `K * 4` keeps its constant. With the
   existing upward rule this alone accepts the two `K * 4 * sizeof(T)` shapes (a non-negative `int` constant adapts
   to `nuint`).
2. A constant `sizeof` for builtin floating types (their size is fixed by the language as for integers). Alone it
   changes nothing in these shapes: it gives `nuint` constants, which do not adapt to `int`.
3. For a runtime `int` times `nuint`, one of: let a non-negative `nuint` constant adapt to the other operand's type
   (a new direction of the constant rule, needing 2 for a floating `T`); make `sizeof` an `int` constant as in C#;
   or accept `int * nuint` as C does. Each of these changes the signed/unsigned rule of the language, not of
   interop.
4. Keep the rule: the source writes the conversion it means (`(nuint)K`, `(nuint)count`).

## 2. `compound assignment cannot implicitly convert its right-hand side from 'builtin:double' to 'builtin:float'`

Construction: `p->f -= d;` where `p` points to a NativeSharp struct whose field `f` is `float` and `d` is a
NativeSharp `double` local (even when that local was initialized from a C call result, it is declared `double`).

Both operands are NativeSharp values; no C/C++ declaration is involved in the assignment. NativeSharp rejects the
implicit narrowing on purpose (the message asks for an explicit cast), as C# does for `float -= double`. Not an
interop defect.

## Validation

Documentation only; nothing was built or run for this finding.
