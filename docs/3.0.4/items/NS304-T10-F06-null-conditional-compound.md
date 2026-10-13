# NS304-T10-F06 — Null-conditional compound assignment (sin `++`/`--`)

- **Estado**: PARCIAL (`obj?.Field += x` numérico + string concat) / residual (`++`/`--`)
- **Rama**: development

## Resultado

- **Sema** (`analyzeAssignment`): el compound sobre LHS null-conditional numérico valida operando
  común + narrowing; el string concat devuelve `string` (el lowering hace load + concat + store).
- **HIR** (`NullConditionalFieldStore`): el nodo lleva `operation` (operador compound).
- **LLVM** (`lowerNullConditionalFieldStore`): en la rama no nula hace `load` → `applyBinaryOperation`
  (numérico) o concat de cadena. El resultado es el valor efectivamente almacenado; en destino
  `owned string`, la cadena desplazada no nula se libera por la ruta canónica antes del store.
- Pruebas: `TL-LANG-342` (RHS omitido en null y resultado compound `15`) y `TL-LANG-346`
  (`owned string`, resultado/store de longitud `11`, reemplazo y destrucción sin crash) → **PASS**
  (2026-10-13).

## Residual

- **`++`/`--`**: postfix/prefijo sobre null-conditional sigue fuera (ambiguo, excluido por el ítem).
