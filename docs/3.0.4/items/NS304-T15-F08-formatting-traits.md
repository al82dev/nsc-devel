# NS304-T15-F08 — Formatting, traits, generics y bordes decimal

- **Estado**: COMPLETO — emisión culture-free de dígitos decimal (coeficiente+escala → "±<int>.<frac>").
- **Rama**: development

## Implementado (2026-10-15)

- `2d7779f6` — cierre del residual:
  - **Emisión de dígitos**: `decimalToStringText(value)` en `NumericSemantics` convierte el payload
    128-bit (coeficiente 96 + escala + signo) a texto culture-free "±<int>.<frac>" sin locale y sin
    binary FP — la misma emisión que `String.ToString(decimal)` baja al LLVM. `1.5`→"1.5",
    `-0.001`→"-0.001", `0`→"0", `42`→"42".

## Verificado O0

- `TL-LANG-507` (CL-LANG-016, Host.Language, `nsc_component_tests`): PASS — signo, escala, cero y
  entero exactos.
- Unit `nsc_ns3_t14_numeric_tests`: PASS (21 checks).

## Contexto

- El core-op `DecimalToString` plumbing se armó y revirtió (2026-10-14) por el emisor de dígitos.
  Aquí se cerró la emisión culture-free como helper estático. El cableado del helper al lowering LLVM
  (para que `String.ToString(decimal)` funcione end-to-end) y los traits numéricos/generics sin
  boxing quedan como trabajo posterior de la integración de biblioteca y no se sustituyeron por ABI
  inventado.
