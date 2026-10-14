# NS304-T08-F05 — Conformance externa (orphan/coherence)

- **Estado**: AUDITADO (witnesses/coherence en TraitSemantics; orphan + metadata pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar conformance externa*.
Plan T08-F05 (DeepSeekDocs): *congelar orphan/coherence: definir qué módulo posee tipo, trait y
extensión; detectar dos impls visibles, ciclos y conformance retroactiva incompatible; publicar
witness y constraints en metadata para resolución intermodular*.

Estado real:

- **TraitSemantics**: `Language/TraitSemantics.*` tiene witnesses, coherence y conformances (la
  resolución de conformance nominal ya existe).
- **Orphan rule**: sin definición congelada de qué módulo posee tipo/trait/extensión (para la
  conformance EXTERNA, no nominal).
- **Metadata**: sin publicación del witness + constraints del operador extension.

## Gap

Falta congelar la regla orphan/coherence para la conformance externa (dos impls visibles, ciclos,
conformance retroactiva incompatible) y publicar el witness + constraints en NSMETA.

## Nota

F05 es la coherence de la conformance externa (operador extension, F04). Es un vertical de reglas +
metadata, no una surface nueva; depende de F04 (witness) y de la definición de posesión de módulo.
