# L6 - `int * sizeof(T)`: alternatives and impact (analysis only, no change)

## Construction

An `int` operand multiplied by `sizeof(T)` (`count * sizeof(T)`, `K * 4 * sizeof(T)`), typically as a size argument.
`sizeof` is `nuint` (`IndexAndRangeExpressions.cpp`); `commonNumericType` (`ProjectTypeConversion.cpp`) gives
`int`/`nuint` a common type only when the `int` operand is a constant that fits (`constantAdaptsUpward`). Binary
integer arithmetic does not keep constants (`analyzeArithmeticBinary`), and `sizeof` of a builtin floating type is
not a constant. T11 records the details.

## Alternatives

| Option | What it changes | Impact on the normal rules |
|---|---|---|
| A. Fold constants of binary integer arithmetic | `K * 4` keeps its constant; with the existing upward rule an `int` constant then adapts to `nuint`. | Small: only expressions made of constants gain a constant; every value is the one computed at run time. Overflow of the folded value must follow the expression's overflow policy, as `foldIntegerConstant` already does. Needed for C#'s constant-expression behavior in general. |
| B. Constant `sizeof` for builtin floating types | `sizeof(float)` becomes the constant 4 (`nuint`). | None alone (a `nuint` constant does not adapt to `int`); useful with C. |
| C. A non-negative `nuint` constant adapts to the other operand | `count * sizeof(byte)` becomes `int * int`. | Introduces a downward direction for constants that C# does not have; the product type becomes `int`, which can overflow where the `nuint` product would not. |
| D. `sizeof` is an `int` constant (C#) | `count * sizeof(T)` is `int`. | Changes every existing use of `sizeof` as `nuint` (pointer arithmetic, `nuint` sizes, C `size_t` arguments rely on it). Large. |
| E. Accept `int * nuint` as C does (usual arithmetic conversions) | The product is `nuint`. | Reverses the deliberate signed/unsigned rule (IDs 120/126): a negative `int` silently becomes a huge `nuint`. Large. |
| F. Keep the rule | The source writes `(nuint)count * sizeof(T)`. | None. |

## Assessment

A is consistent with C# and has the smallest impact; with B and C a runtime `int` times a constant `sizeof` would
also compile, but C changes the direction of the constant rule. A runtime `int` times a non-constant `nuint`
needs D or E, which change the language's integer rules. No option is implemented.
