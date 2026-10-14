# NS304-T11-F06 — Traits sin dyn escapable

- **Estado**: AUDITADO (witnesses estáticos existen en TraitSemantics; dyn ref-like sin rechazo)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar traits sin dyn escapable*.
Plan T11-F06 (DeepSeekDocs): *permitir implementación estática/witness de traits por ref-like;
rechazar conversión a `dyn` cuyo storage pueda escapar o requiera box; probar dispatch genérico sin
heap*.

Estado real:

- `TraitSemantics` tiene witnesses/coherence (implementación estática nominal). Un ref-like
  implementando un trait por witness: sin path específico.
- Conversión a `dyn` de un ref-like: sin rechazo de escape/box.

## Gap

Falta permitir el witness estático de un trait por ref-like y rechazar la conversión a `dyn` cuyo
storage escape o requiera box (dispatch genérico sin heap).

## Nota

F06 depende de F05 (constraint ref-like) + el witness de T08-F04/F05. No es una surface nueva; es la
interacción ref-like × trait (static dispatch, no box).
