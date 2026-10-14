# NS304-T16-F02 — Writer/reader y compatibilidad 3.0

- **Estado**: AUDITADO (codec/merger/API diff existen; round-trip de campos nuevos + fixtures previos sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar writer/reader y compatibilidad 3.0*.
Plan T16-F02 (DeepSeekDocs): *actualizar writer y reader juntos; round-trip de cada nuevo campo;
defaults seguros para metadata anterior; rechazo explícito de versión futura o contrato incompatible;
merger/API diff conservan semántica y orden estable; fixtures binarios de versión anterior, no
fabricados sólo en memoria*.

Estado real:

- El codec/merger/API diff existen; el round-trip de los campos nuevos (T03–T15) + los defaults seguros
  de metadata anterior + el rechazo de versión futura + los fixtures binarios no están cerrados.

## Gap

Falta actualizar writer+reader juntos (round-trip de cada campo nuevo), defaults seguros para metadata
3.0 anterior, rechazo explícito de versión futura/incompatible, y fixtures binarios de versión previa.

## Nota

F02 depende de F01 (versiones) + el codec existente. No es una surface nueva; es el round-trip + la
compatibilidad downlevel del metadata.
