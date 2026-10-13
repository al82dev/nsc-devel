# NS304-T06-F01 — Retirar rechazo NS3049 de yield return/yield break

- **Estado**: CERRADO (AST) / residual (lowering T06-F03)
- **Rama**: development

## Resultado (validado)

- `yield return expr` → `YieldReturnStatement`, `yield break` → `YieldBreakStatement` (nuevos
  `AstNodeKind`); el rechazo NS3049 (`Language3Feature::YieldGenerators`) está retirado.
- Sin regresión (13/13).
- El lowering productivo a frame/state machine es T06-F03.
