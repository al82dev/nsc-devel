# NS304-T14-F05 — Integración generics/associated/partial/extension

- **Estado**: AUDITADO (constraints/associated/partial/extension existen; generados sin unificar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar integración de generados*.
Plan T14-F05 (DeepSeekDocs): *los generados participan en constraints, associated items, merge parcial
y lookup extension por las mismas identidades; resolver dependencias/ciclos determinista; esta
evidencia desbloquea T12-F06*.

Estado real:

- constraints/associated/partial/extension (T08/T09/T12) existen para fuente. Un miembro GENERADO no
  participa por las mismas identidades (merge parcial + lookup extension + associated).

## Gap

Falta que los generados participen en constraints/associated/partial/extension por las mismas
identidades + la resolución determinista de dependencias/ciclos (desbloquea T12-F06).

## Nota

F05 depende de F02 (generación tipada) + T12 (partial) + T08 (extension). No es una surface nueva; es
la unificación de la identidad de generados con fuente.
