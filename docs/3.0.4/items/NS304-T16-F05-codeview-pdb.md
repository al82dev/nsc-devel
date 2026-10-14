# NS304-T16-F05 — CodeView/PDB

- **Estado**: AUDITADO (CodeView/PDB existe para básicos; construcciones nuevas sin representar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar CodeView/PDB*.
Plan T16-F05 (DeepSeekDocs): *representar closures/captures, patterns/bindings, async/generator frames,
static initialization, spans/ref-like, decimal y generated source donde aplique; sequence points mapean
a fuente lógica; locals visibles sólo cuando vivos; no emitir información falsa*.

Estado real:

- El CodeView/PDB existe para las construcciones básicas; closures/captures, patterns/bindings,
  async/generator frames, static init, spans/ref-like, decimal y generated source no están
  representados.

## Gap

Falta representar las construcciones nuevas (closures, patterns, frames async/generator, static init,
spans/ref-like, decimal, generated source) + sequence points honestos + locals visibles sólo vivos.

## Nota

F05 depende de F01 (versiones) + el CodeView/PDB existente. No es una surface nueva; es el debug
honesto de las construcciones T03–T15.
