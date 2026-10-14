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

### Auditado ronda 113

- El const genérico (`const N`) es un feature **transversal del type-system** (no específico de
  inline arrays): exige materializar el valor const como parámetro de tipo + aridad en el layout.
  Es un vertical grande; el fixed buffer con length literal (la base de F06) ya está cerrado.
