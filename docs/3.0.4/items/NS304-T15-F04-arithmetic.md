# NS304-T15-F04 — Aritmética y comparación decimal

- **Estado**: PARCIAL (add/sub de igual escala operativos; mul/div/rem + alineación de escalas + rounding/overflow pendientes)
- **Rama**: development

## Resultado (add/sub/mul/div/rem — 2026-10-14)

- **Type check**: `builtin:decimal` entra en `kNumericTypes`; `commonNumericType` devuelve
  `builtin:decimal` para el par de igual tipo.
- **LLVM**: `+`/`-`/`*`/`/`/`%` sobre `builtin:decimal` extraen el coeficiente (low 96), operan y
  recombinan con la escala correcta: add/sub/rem **alinean a la escala máxima** (cada coeficiente por
  `10^(max−su escala)`), mul **suma** las dos escalas, div **alinea** el numerador `*10`. Div/rem van
  por la vía de 64 bits (`__udivti3` no enlazado). `buildPow10` (bucle runtime) es el helper compartido.
- Verificado: `(a+b)-a == b`, `a*b == b*a`, `1.5m/2.5m == 0.6`, `4.5m%2.5m == 2.0`, `1.5m+2m == 3.5`,
  `4.5m%2m == 0.5` → exit 0. Registrado `TL-LANG-423/424/426/427/430/431` (PASS).

## Residual

- El **rounding** de división periódica + la escala mixta en **div** (solo add/sub/rem alinean hoy) +
  el overflow de la escala (0–28) — el resto del corpus de F04.
