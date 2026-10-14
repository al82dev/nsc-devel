# NS304-T12-F05 — Eventos parciales

- **Estado**: AUDITADO (eventos nativos T09-F08 cerrados; parcial sin surface)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar eventos parciales*.
Plan T12-F05 (DeepSeekDocs): *emparejar tipo de handler, add/remove, accessibility y storage declarado;
no crear multicast heap oculto; dos storage definitions o accessors incompatibles producen diagnóstico
estable*.

Estado real:

- Eventos nativos (handler + add/remove + storage) están cerrados (T09-F08). Un evento parcial
  (declaration + implementation separadas) no existe.

## Gap

Falta el evento parcial: emparejar handler/add/remove/accessibility/storage + el diagnóstico estable
de dos storage definitions/accessors incompatibles (sin multicast heap oculto).

## Nota

F05 depende de F01 (identidad) + T09-F08 (eventos). No es una surface nueva; es el emparejamiento
parcial del evento.
