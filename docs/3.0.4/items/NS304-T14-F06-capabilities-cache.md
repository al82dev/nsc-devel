# NS304-T14-F06 — Capacidades, inputs y cache

- **Estado**: AUDITADO (cache/build graph existe; capacidades/inputs del macro sin declarar)
- **Rama**: development

## Auditoría (2026-10-14)

- La cache/build graph existe; el hueco es **declarar capacidades/inputs del macro** (archivos/config/
  variables) + la **cache key completa** (sin reloj/red/cwd/estado global) + la equivalencia clean/
  incremental — el determinismo de inputs, depende de F02.
