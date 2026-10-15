# NS304-T13-F09 — Cost report

- **Estado**: COMPLETO — `--explain-project-costs` demuestra stack/arena/heap elegido.
- **Rama**: development

## Implementado (2026-10-15)

- `345d63a0` — el explorador de costo (`ProjectCostExplorer`) ahora cuenta el nodo MIR
  `PlacementConstruct` (`new (allocator) T(...)`) como **arena** (categoría nueva
  `arenaAllocationSites`), separada de `heap` y `stack`; causa `arena <- placement-construct`.
  Texto + JSON exponen `arena-sites`/`arenaAllocationSites`. Caso focal TL-LANG-499 (integración
  vía `nsc.exe --explain-project-costs`).

## Verificado O0

- TL-LANG-499 (cost report): PASS — el reporte de un proyecto con `scope` + `new` +
  `new (allocator)` muestra `arena-sites=1`, `heap-sites` y `stack-bytes-lower-bound`.
- Unit `nsc_ns3_p08_p10_explain_tests`: PASS (8 checks) — el explorer deriva stack/arena/heap/move/drop.

## Contexto

- El reporte de costo (`--explain-project-costs` / `--explain-project-costs-json`) ya existía
  (P08-P10 "explain costs") para stack + heap. El hueco de T13-F09 era la **arena** (placement vía
  allocator) y la evidencia de integración: el `PlacementConstruct` no se contaba.
- `noheap` sigue verificando ausencia de heap; el reporte es la evidencia positiva de storage elegido.
