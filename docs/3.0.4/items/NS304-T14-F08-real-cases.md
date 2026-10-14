# NS304-T14-F08 — Casos reales

- **Estado**: AUDITADO (sin fixtures de derive tipado)
- **Rama**: development

## Auditoría (2026-10-14)

- No hay fixtures de derive tipado; el hueco son los **cuatro derives** (serializer/formatter/binding/
  trait) generando código consumible sin runtime reflection + la comparación manual vs generado (ABI/
  metadata/ejecución/costos) — el cierre de T14, depende de F02–F07.
