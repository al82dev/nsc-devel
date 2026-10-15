# NS304-T15-F07 — Consteval/runtime y lowering decimal

- **Estado**: COMPLETO — helper estático de aritmética decimal (ABI estable consteval/runtime).
- **Rama**: development

## Implementado (2026-10-15)

- `8b25b044` — cierre del residual:
  - **Helper estático**: `evaluateDecimalArithmetic(left, right, operation, error)` en
    `NumericSemantics` — la ABI estable que consteval y runtime comparten. add/sub/rem alinean a la
    escala máxima, mul suma escalas, div calcula la escala objetivo y redondea half-up; escala >28 y
    coeficiente >96 bits se rechazan con error causal. El cociente/residuo usan la vía de 64 bits
    (sin `__udivti3`/`__umodti3`).
  - **Equivalencia consteval/runtime**: verificada en O0 — `const decimal K = 1.5m + 2m;` y
    `decimal d = 1.5m + 2m;` producen el mismo valor.

## Verificado O0

- `TL-LANG-506` (CL-LANG-016, Host.Language, `nsc_component_tests`): PASS — add/div/mul/overflow del
  helper (1.5+2=3.5; 0.5/3=0.17; 1.5*2=3.0; escala>28 rechazada).
- Unit `nsc_ns3_t14_numeric_tests`: PASS (20 checks).

## Contexto

- El const decimal ya materializaba i128 (2026-10-14). Aquí se cerró el helper estático con ABI
  estable (el "unificar consteval/runtime"). El coeficiente >64 bits en el literal (F05) y el cableado
  del helper al lowering LLVM (para que el runtime lo llame) quedan como trabajo posterior de F05/F08
  y no se sustituyeron por ABI inventado.
