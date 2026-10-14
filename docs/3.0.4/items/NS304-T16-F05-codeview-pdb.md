# NS304-T16-F05 — CodeView/PDB

- **Estado**: AUDITADO (CodeView/PDB existe para básicos; construcciones T03–T15 sin representar)
- **Rama**: development

## Auditoría (2026-10-14)

- El hueco es la **representación debug** de las construcciones nuevas (closures/captures, patterns/
  bindings, frames async/generator, static init, spans/ref-like, decimal, generated source) + sequence
  points honestos + locals vivos — un vertical de debug info (no una feature de lenguaje aislable).
