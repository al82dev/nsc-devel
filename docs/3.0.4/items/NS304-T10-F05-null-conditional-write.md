# NS304-T10-F05 — `?.`/`?[]` a la izquierda con evaluación condicional del RHS

- **Estado**: PARCIAL (`obj?.Field = value`) / residual (`?[]` write + RHS condicional)
- **Rama**: development

## Resultado

- **Sema** (`analyzeAssignment`): `obj?.Field = value` ya no se rechaza como "not assignable"; se
  devuelve el valor del RHS y se deja el store condicional al lowering.
- **HIR** (`ProjectOopHirModel`): nuevo nodo `NullConditionalFieldStore` (receiver + value) en la
  rama de asignación.
- **MIR/LLVM**: nuevo `NullConditionalFieldStore` con null-check + store condicional (reutiliza el
  patrón de `NullConditionalFieldLoad`).
- Prueba `TL-LANG-340`: `b?.Name = "x"` (skip en null) + `c?.Name = "hello"` (store) → 5 → **PASS**.

## Residual

- **`?[]` (index) write**: no existe el token `?[`; requiere lexer/parser/sema nuevos.
- **RHS condicional**: el RHS se evalúa incondicionalmente (simplificación); C# lo evalúa solo si el
  receptor no es null. Campo de tipo valor (`int`) sigue rechazado por el READ nullable.
