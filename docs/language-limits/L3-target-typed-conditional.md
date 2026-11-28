# L3 - Target-typed conditional expression

## Limitation

`c ? a : b` had only its natural type. `uint x = c ? K1 : K2;` with `K1` and `K2` constants of type `int` failed:
the natural type is `int`, which a non-constant `int` result does not convert to `uint`.

## Rule

As in C# 9, a conditional expression that is the whole initializer of a local, or the whole right-hand side of a
plain assignment, is target-typed when its natural type is missing or does not convert to the target, and each
branch converts to the target on its own (with its own constant, if any). The result then has the target type and
each branch is converted to it. Elsewhere, and when no branch-wise conversion exists, the normal rules and their
diagnostics apply.

## Implementation

- `ProjectMethodBodyModel::targetTypedConditional_`: the local initializer (`StatementAnalyzer.cpp`) and the plain
  assignment (`AssignmentAnalyzer.cpp`) name the conditional that is their whole source, next to
  `activeTargetTypeKey_`.
- `analyzeExpressionKindConditionalExpression` (`OperatorAndConditionalExpressions.cpp`): for that conditional the
  natural type is resolved with a separate diagnostic engine; if it fits the target its result and diagnostics are
  kept, otherwise, when both branches convert (`classifyImportedFlexibleConversion`), the result is the target type.
- HIR converts each branch to the conditional's type (`convertConditionalBranch`), unchanged.

## Not covered

Return statements, arguments, and compound assignments are not target-typed by this change.

## Validation

NSC built in Release with `/clang:-O3`. No tests were run.
