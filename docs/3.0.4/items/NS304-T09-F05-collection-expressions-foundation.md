# NS304-T09-F05 — Collection expressions

- **Estado**: PARCIAL (parser + target-typed a array nativo) / residual (inferencia abierta + spreads + builder)
- **Rama**: development

## Resultado

- **AST**: nuevo `AstNodeKind::CollectionExpression` (elementos en children, orden de fuente).
- **Parser**: `[a, b, c]` en posición de expresión primaria se reconoce como collection expression
  (`parseCollectionExpression`, marker `inferred-array-initializer`).
- **Sema/HIR/LLVM**: `int[] a = [1, 2, 3]` target-typed a array nativo reutiliza `ArrayAllocation`
  (inferencia de cuenta por elementos); inferencia vía `activeTargetTypeKey_`.
- Pruebas: `TP-PAR-539` (parser), `TA-AST-019..025`, `TL-LANG-347` (`a[0]+a[1]+a[2]` = 6) → **PASS**.

## Residual

- **Inferencia abierta** (sin target type, p. ej. `var` o argumento genérico).
- **Spreads** (`..x`) y **builder protocol**: fuera de esta base.

