# NS304-T09-F06 — Inline arrays/fixed buffers seguros con length const genérico

- **Estado**: PARCIAL (fixed array `T[n]` con length literal) / residual (length const genérico)
- **Rama**: development

## Resultado

- Los fixed buffers `T[n]` ya son seguros y de layout determinista (storage por valor, zeroing por
  el backend, bounds/borrow en list patterns): cubiertos por `TL-LANG-308`/`309`/`310` y el
  lowering de `FixedArrayLength`/`RawElementAddress`.
- No requería cambios de código para la base; se registra para cerrar esa parte.

## Residual

- **Length const genérico**: permitir `T[N]` donde `N` es un parámetro genérico (`const N`)
  requiere materializar el tamaño en el sistema de tipos (misma familia que el Span<T> de T03-F03).

### Auditado ronda 113 + verificación 2026-10-14

- El const genérico (`const N`) es un feature **transversal del type-system**: exige materializar el
  valor const como parámetro de tipo + aridad en el layout.
- El valor const YA es productivo como expresión (`Fixed<T, const int N> { int Count() { return N; } }`
  + `Fixed<int, 0>`/`Fixed<int, 1>`; ver `FrontendGenericsFlowModule.cpp`).
- El hueco concreto: `public int[N] data;` **no parsea** (`expected '(', found ';'` en el `[N]`): el
  array count sólo acepta longitud literal, no un const genérico. La vertical es: parser (const expr
  en el count) → Sema (materializar el valor const del argumento) → layout (aridad = valor const).
