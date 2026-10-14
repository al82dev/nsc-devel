# NS304-T09-F06 — Inline arrays/fixed buffers seguros con length const genérico

- **Estado**: CERRADO (fixed buffer `T[n]` literal + length const genérico `const N`)
- **Rama**: development

## Resultado

- Los fixed buffers `T[n]` ya son seguros y de layout determinista (storage por valor, zeroing por
  el backend, bounds/borrow en list patterns): cubiertos por `TL-LANG-308`/`309`/`310` y el
  lowering de `FixedArrayLength`/`RawElementAddress`.
- **Length const genérico**: `struct Fixed<const int N> { public int[N] data; }` materializa el
  valor `N` como **aridad del layout** y como **valor** (`Count() { return N; }`). Verificado:
  `Fixed<3>` (3 ints) + `Fixed<2>` (2 ints) + `f.data[..]` + `f.Count()` → `TL-LANG-401` exit 42.
  El const genérico ya era productivo (el valor como expresión en `FrontendGenericsFlowModule.cpp`);
  la aridad del fixed buffer también lo es (verificación 2026-10-14).
