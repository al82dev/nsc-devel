# NS304-T09-F05 — Collection expressions

- **Estado**: PARCIAL (arrays nativos y multi-spread materializado) / residual (builder protocol y
  semántica de copia del single-spread)
- **Rama**: development

## Resultado

- **AST**: nuevo `AstNodeKind::CollectionExpression` (elementos en children, orden de fuente).
- **Parser**: `[a, b, c]` en posición de expresión primaria se reconoce como collection expression
  (`parseCollectionExpression`, marker `inferred-array-initializer`); `..x` baja como
  `SpreadElementExpression`.
- **Sema/HIR/LLVM**: `int[] a = [1, 2, 3]` target-typed, `var b = [4, 5]` y `var d = [1, 2.0]`
  (tipo numérico común → `double[]`) reutilizan `ArrayAllocation`; `int[] b = [..a]` (un único
  spread) conserva por ahora su comportamiento histórico de alias por referencia. Las formas
  materializadas conservan en HIR/MIR una secuencia ordenada `Element`/`Spread`; LLVM evalúa cada
  segmento una vez, suma todas las longitudes con guard de overflow y copia/almacena mediante un
  cursor acumulado. Cada spread debe ser un array nativo cuyo elemento coincida exactamente con el
  tipo destino.
- Pruebas: `TP-PAR-539/540` (parser), `TA-AST-019..025`, `TL-LANG-347/348/349/354` → **PASS**.

## Residual

- **Builder protocol**: fuera de esta base.
- **Single-spread copy semantics**: `[..a]` continúa siendo alias de `a`; su corrección pertenece
  al frente independiente de aliasing y no se considera cerrada por el soporte multi-spread.


