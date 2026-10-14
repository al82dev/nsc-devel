# NS304-T13-F03 — Scope/stack

- **Estado**: AUDITADO (construcción en stack + drop RAII existen; escape analysis del lifetime léxico pendiente)
- **Rama**: development

## Auditoría (2026-10-14)

- La construcción en stack + drop automático (RAII) ya existe; el hueco es la **surface `scope`** +
  el **escape analysis** que rechaza retorno/capture/almacenamiento más largo + el tamaño/alignment
  consteval (nunca heap fallback) — un vertical ancho que depende de F01/F02.
