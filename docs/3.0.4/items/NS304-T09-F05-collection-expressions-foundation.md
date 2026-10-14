# NS304-T09-F05 — Collection expressions

- **Estado**: CERRADO (arrays nativos + spreads + builder protocol ruta mínima)
- **Rama**: development

## Resultado

- **AST**: nuevo `AstNodeKind::CollectionExpression` (elementos en children, orden de fuente).
- **Parser**: `[a, b, c]` en posición de expresión primaria se reconoce como collection expression
  (`parseCollectionExpression`, marker `inferred-array-initializer`); `..x` baja como
  `SpreadElementExpression`.
- **Sema/HIR/LLVM**: `int[] a = [1, 2, 3]` target-typed, `var b = [4, 5]` y `var d = [1, 2.0]`
  (tipo numérico común → `double[]`) reutilizan `ArrayAllocation`. Todas las formas con spread,
  incluido `int[] b = [..a]`, materializan almacenamiento independiente.
- Pruebas base: `TP-PAR-539/540`, `TA-AST-019..025`, `TL-LANG-347/348/349/354` → **PASS**.

### Builder protocol (surface A, ruta causal mínima)

- **Sema**: un target no-array con `CreateBuilder` (estático, 0 params), `Add` (instancia, 1 param)
  y `ToCollection` (instancia, 0 params) se reconoce como builder. Cada elemento se valida contra
  el parámetro de `Add` (diagnóstico si no es convertible).
- **HIR**: baja `[a, b, c]` a una `BlockExpression` (nodo HIR/MIR/LLVM de expresión secuenciada):
  1. `LocalDeclaration` del temporal (inicializado por `CreateBuilder()`);
  2. `ExpressionStatement` de `Add(elemento)` por cada elemento, en orden de fuente, con el
     temporal como receptor (`LocalAddress`);
  3. `ToCollection()` como el hijo final → valor resultante del bloque.
- **Sin heap como fallback**; `CreateBuilder` se evalúa exactamente una vez.
- Pruebas: `TL-LANG-395/396/397/398` → **PASS** (múltiples, cero/uno/anidado, orden/efectos
  laterales, elemento inválido diagnostica).

## Residual (fuera del cierre)

- El marcador explícito `[CollectionBuilder]` de la surface A (atributo para declarar el builder por
  tipo en vez de por convención de nombre) queda fuera de esta ruta mínima; la detección actual es
  por nombre `CreateBuilder`/`Add`/`ToCollection`.
- Capacidad/storage/failure explícitos + cleanup parcial ante builder fallido no se implementan
  (la surface A mínima no los requiere para el cierre funcional).



