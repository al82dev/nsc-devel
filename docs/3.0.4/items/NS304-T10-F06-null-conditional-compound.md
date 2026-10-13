# NS304-T10-F06 — Null-conditional compound assignment (sin `++`/`--`)

- **Estado**: PARCIAL (`obj?.Field += x` numérico + string concat) / residual (`++`/`--`)
- **Rama**: development

## Resultado

- **Sema** (`analyzeAssignment`): el compound sobre LHS null-conditional numérico valida operando
  común + narrowing; el string concat devuelve `string` (el lowering hace load + concat + store).
- **HIR** (`NullConditionalFieldStore`): el nodo lleva `operation` (operador compound).
- **LLVM** (`lowerNullConditionalFieldStore`): en la rama no nula hace `load` → `applyBinaryOperation`
  (numérico) o concat de cadena (asigna nueva, la vieja queda sin liberar — simplificación).
- Pruebas: `TL-LANG-342` (numérico = 15) y `TL-LANG-346` (`c.Name += " world"` → 11) → **PASS**.

## Residual

- **`++`/`--`**: postfix/prefijo sobre null-conditional sigue fuera (ambiguo, excluido por el ítem).
