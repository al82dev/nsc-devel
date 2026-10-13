# NS304-T09-F05 — Collection expressions

- **Estado**: PARCIAL (parser + target-typed a array nativo) / residual (inferencia abierta + spreads + builder)
- **Rama**: development

## Resultado

- **AST**: nuevo `AstNodeKind::CollectionExpression` (elementos en children, orden de fuente).
- **Parser**: `[a, b, c]` en posición de expresión primaria se reconoce como collection expression
  (`parseCollectionExpression`, marker `inferred-array-initializer`).
- **Sema/HIR/LLVM**: `int[] a = [1, 2, 3]` target-typed y `var b = [4, 5]` (inferencia abierta por
  el primer elemento) reutilizan `ArrayAllocation` (inferencia de cuenta por elementos).
- Pruebas: `TP-PAR-539` (parser), `TA-AST-019..025`, `TL-LANG-347` (target + var = 15) → **PASS**.

## Residual

- **Inferencia de tipo común** (elementos heterogéneos, p. ej. `[1, 2.0]` → `float[]`).
- **Spreads** (`..x`) y **builder protocol**: fuera de esta base.

