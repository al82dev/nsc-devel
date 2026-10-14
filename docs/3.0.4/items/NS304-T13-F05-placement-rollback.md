# NS304-T13-F05 — Placement/in-place y rollback

- **Estado**: AUDITADO (drop inverso existe; placement/rollback de construction pendiente)
- **Rama**: development

## Auditoría (2026-10-14)

- El drop inverso de subobjetos ya existe (T09); el hueco es la **construcción placement/in-place**
  (validación storage/alignment/capacidad + construcción en orden) + el **rollback ante failure**
  (destruir subobjetos completos en inverso, sin doble free/drop) — un vertical ancho que depende de
  F01/F02 + T09.
