# NS304-T09-F05 — Collection expressions

- **Estado**: PARCIAL (parser + target-typed a array nativo) / residual (inferencia abierta + spreads + builder)
- **Rama**: development

## Resultado

- **AST**: nuevo `AstNodeKind::CollectionExpression` (elementos en children, orden de fuente).
- **Parser**: `[a, b, c]` en posición de expresión primaria se reconoce como collection expression
  (`parseCollectionExpression`, marker `inferred-array-initializer`); `..x` baja como
  `SpreadElementExpression`.
- **Sema/HIR/LLVM**: `int[] a = [1, 2, 3]` target-typed, `var b = [4, 5]` y `var d = [1, 2.0]`
  (tipo numérico común → `double[]`) reutilizan `ArrayAllocation` (inferencia de cuenta).
- Pruebas: `TP-PAR-539/540` (parser), `TA-AST-019..025`, `TL-LANG-347/348/349` → **PASS**.

## Residual

- **Spread flattening en Sema/HIR**: `..x` se parsea y diagnostica claro, pero el aplanado/baja
  (copiar los elementos del spread en la asignación) aún no está cableado.
- **Builder protocol**: fuera de esta base.


