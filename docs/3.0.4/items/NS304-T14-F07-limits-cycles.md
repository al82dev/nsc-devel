# NS304-T14-F07 — Límites y ciclos

- **Estado**: PARCIAL (presupuesto de pasos de macro + backtrace operativos; profundidad/nodos/memoria + ciclos pendientes)
- **Rama**: development

## Resultado (presupuesto de pasos — 2026-10-14)

- **Recursion de expansión**: una macro recursiva termina con diagnóstico estable `macro expansion
  limit exceeded` + `comptime backtrace` (los últimos 8 `#expand`), verificado en O0 — no crash/hang.
  `MacroExpansionEngine` aplica `maximumMacroExpansions` (pasos) por `limits_.maximumMacroExpansions`.

## Residual

- Los presupuestos de **profundidad/nodos/memoria** separados + la detección específica de **ciclo de
  símbolos/colisión** (hoy el presupuesto de pasos los absorbe genéricamente).
