# NS304-T15-F04 — Aritmética y comparación decimal

- **Estado**: PARCIAL (add/sub/mul/div/rem + escala mixta en div operativos; rounding de división periódica + overflow de escala pendientes)
- **Rama**: development

## Resultado (add/sub/mul/div/rem + escala mixta en div — 2026-10-14)

- **Type check**: `builtin:decimal` entra en `kNumericTypes`; `commonNumericType` devuelve
  `builtin:decimal` para el par de igual tipo.
- **LLVM**: `+`/`-`/`*`/`/`/`%` sobre `builtin:decimal` extraen el coeficiente (low 96), operan y
  recombinan con la escala correcta: add/sub/rem **alinean a la escala máxima** (cada coeficiente por
  `10^(max−su escala)`), mul **suma** las dos escalas, div usa `target = max(s1,s2)+(s1>s2?1:0)` y
  `numerador = c1·10^(target−s1+s2) / c2` (escala mixta exacta). Div/rem van por la vía de 64 bits
  (`__udivti3` no enlazado). `buildPow10` (bucle runtime) es el helper compartido.
- Verificado: `(a+b)-a == b`, `a*b == b*a`, `1.5m/2.5m == 0.6`, `4.5m%2.5m == 2.0`, `1.5m+2m == 3.5`,
  `4.5m%2m == 0.5`, `1.5m/2m == 0.75`, `1m/2.5m == 0.4` → exit 0. Registrado
  `TL-LANG-423/424/426/427/430/431/478` (PASS).

## Residual

- El **rounding de división periódica** (1/3 → truncamiento, no redondeo) + el overflow de la escala
  (0–28, add/mul/div sin trap) — el resto del corpus de F04.
