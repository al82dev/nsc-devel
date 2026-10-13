# NS304-T10-F06 — Null-conditional compound assignment (sin `++`/`--`)

- **Estado**: PARCIAL (`obj?.Field += x` numérico) / residual (string concat + `++`/`--`)
- **Rama**: development

## Resultado

- **Sema** (`analyzeAssignment`): el compound sobre LHS null-conditional numérico valida operando
  común + narrowing y deja el read-modify-write condicional al lowering (antes era diagnóstico).
- **HIR** (`NullConditionalFieldStore`): el nodo lleva `operation` (operador compound).
- **LLVM** (`lowerNullConditionalFieldStore`): en la rama no nula hace `load` → `applyBinaryOperation`
  → `store`, reutilizando la aritmética canónica de `applyBinaryOperation`.
- Prueba `TL-LANG-342`: `b?.Value += 1` (skip en null) + `c.Value = 10; c?.Value += 5` → 15 → **PASS**.

## Residual

- **String concat**: `obj?.Name += "x"` requiere el path RAII de concatenación (hoy sólo numérico).
- **`++`/`--`**: postfix/prefijo sobre null-conditional sigue fuera (ambiguo, excluido por el ítem).
