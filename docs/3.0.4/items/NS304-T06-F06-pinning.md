# NS304-T06-F06 — Pinning y construcción in-place para frames autoreferenciales

- **Estado**: HECHO (pinning caller-stack + frame autoreferencial cerrados; in-place movible = T13)
- **Rama**: development

## Resultado (pinning + frame autoreferencial — 2026-10-14)

- **Pinning (caller-stack)**: el frame es `alloca` (no se mueve), así que el pinning es un no-op
  correcto. Verificado O0 (`TL-LANG-442`, exit 20): `Iterator<ref int> Gen(){ int x=10; yield return
  x; yield return x; }` — la referencia a `x` sigue válida entre yields (el frame no se movió).
- **Frame autoreferencial**: el item `ref`/`ref mut` presta el local del frame (F05), formando la
  self-reference productiva.

## Residual (T13)

La **construcción in-place + pinning para storage movible** (arena/heap) pertenece al contrato de
T13 (allocators/scope/inplace). El plan F06 dice explícitamente no adelantarlo; con caller-stack no
hay hueco productivo.
