# NS304-T13-F06 — Trailing/append storage

- **Estado**: AUDITADO (layout consteval existe; trailing/append pendiente)
- **Rama**: development

## Auditoría (2026-10-14)

- El layout consteval de tipos ya existe (ObjectLayoutModel); el hueco es el **trailing/append storage**
  (cola variable con longitud/capacidad contractual + overflow/alignment/bounds + vistas con provenance
  + ABI versionado) — un vertical ancho que depende de F01.
