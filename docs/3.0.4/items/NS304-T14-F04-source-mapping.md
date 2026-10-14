# NS304-T14-F04 — Source mapping y macro backtrace

- **Estado**: AUDITADO (spans de fuente + diagnóstico primario existen; mapping generado→invocación pendiente)
- **Rama**: development

## Auditoría (2026-10-14)

- Los spans + diagnóstico primario existen; el hueco es el **source mapping de generados** (invocación +
  plantilla) + la **cadena de expansión** en el diagnóstico + la navegación al origen (sin spans
  ficticios) — depende de F02 (origin).
