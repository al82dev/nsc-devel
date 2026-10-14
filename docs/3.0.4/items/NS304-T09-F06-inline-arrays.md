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

### Auditado ronda 113 + verificación 2026-10-14 (corregida)

- El const genérico (`const N`) es un feature **transversal del type-system**: exige materializar el
  valor const como parámetro de tipo + aridad en el layout.
- El valor const YA es productivo como expresión (`Fixed<T, const int N> { int Count() { return N; } }`
  + `Fixed<int, 0>`/`Fixed<int, 1>`; ver `FrontendGenericsFlowModule.cpp`).
- **Hueco base (corregido)**: la DECLARACIÓN del fixed buffer sí parsea (`struct Fixed { public
  int[3] data; }` compila; `int[3] a = default;` también). El fallo real es el **index access**:
  `a[0]`/`f.data[0]` da `expected '(', found ';'` (el parser no reconoce el `[0]` sobre un fixed
  array como indexación). Sobre esa base, `int[N]` añade la const expr en el count. La vertical es:
  parser (index access de fixed array) → Sema (materializar el valor const) → layout (aridad).
