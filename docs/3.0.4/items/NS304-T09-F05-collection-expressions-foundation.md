# NS304-T09-F05 — Collection expressions

- **Estado**: PARCIAL (arrays nativos y spreads materializados) / residual (builder protocol)
- **Rama**: development

## Resultado

- **AST**: nuevo `AstNodeKind::CollectionExpression` (elementos en children, orden de fuente).
- **Parser**: `[a, b, c]` en posición de expresión primaria se reconoce como collection expression
  (`parseCollectionExpression`, marker `inferred-array-initializer`); `..x` baja como
  `SpreadElementExpression`.
- **Sema/HIR/LLVM**: `int[] a = [1, 2, 3]` target-typed, `var b = [4, 5]` y `var d = [1, 2.0]`
  (tipo numérico común → `double[]`) reutilizan `ArrayAllocation`. Todas las formas con spread,
  incluido `int[] b = [..a]`, materializan almacenamiento independiente. HIR/MIR conserva una
  secuencia ordenada `Element`/`Spread`; LLVM evalúa cada
  segmento una vez, suma todas las longitudes con guard de overflow y copia/almacena mediante un
  cursor acumulado. Cada spread debe ser un array nativo cuyo elemento coincida exactamente con el
  tipo destino.
- Pruebas: `TP-PAR-539/540` (parser), `TA-AST-019..025`, `TL-LANG-347/348/349/354` → **PASS**.
  `TL-LANG-354` demuestra evaluación única y mutación independiente en ambas direcciones.

## Residual

- **Builder protocol**: fuera de esta base.


