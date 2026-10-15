# NS304-T15-F04 — Aritmética y comparación decimal

- **Estado**: COMPLETO — redondeo half-up de la división + trampa de escala >28.
- **Rama**: development

## Implementado (2026-10-15)

- `1fb0f563` — cierre del residual:
  - **Redondeo de división periódica**: el cociente decimal suma `divisor/2` antes de la división
    entera (round half-up), de modo que `0.5m / 3m` → `0.17` en vez de truncar a `0.16`.
  - **Overflow de escala**: `emitDecimalScaleOverflow` comprueba que la escala efectiva no supere 28;
    `mul` (suma de escalas) y `div` (target + guard) disparan `DecimalOverflow` (exit 128+17) cuando
    excede 28, en vez de envolver los bits de escala.

## Verificado O0

- `TL-LANG-505` (CL-LANG-020, Host.Language, `nsc_component_tests`): PASS — `0.5m/3m` redondea a
  `0.17` (exit 0); `0.0000000000000000000000000001m * 0.1m` (escala 29) trampa exit 145.
- Regresión `TL-LANG-423/424/426/427/430/431/478`: PASS.

## Contexto

- add/sub/mul/div/rem + escala mixta en div ya operaban (2026-10-14). Aquí se cerraron el redondeo
  de la división periódica y el overflow de escala. add/sub/rem alinean a la escala máxima (≤28) y no
  necesitan la trampa de escala.
