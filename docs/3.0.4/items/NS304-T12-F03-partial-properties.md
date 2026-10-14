# NS304-T12-F03 — Properties e indexers parciales

- **Estado**: AUDITADO (sin properties/indexers parciales)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar properties e indexers parciales*.
Plan T12-F03 (DeepSeekDocs): *emparejar tipo, parámetros, accessors y mutabilidad; permitir separación
de declaration y implementation conforme al contrato, pero no sintetizar storage o accessor faltante
silenciosamente; diagnosticar get/set/init/ref conflict*.

Estado real:

- Properties/indexers normales existen (T08/T09). La separación parcial de declaration/implementation
  de una property/indexer no existe.

## Gap

Falta emparejar tipo/parámetros/accessors/mutabilidad de properties/indexers parciales + la separación
declaration/implementation (sin sintetizar storage/accessor silencioso) + el diagnóstico de
get/set/init/ref conflict.

## Nota

F03 depende de F01/F02 (identidad + métodos) y de T08/T09 (properties). No es una surface nueva; es
el emparejamiento parcial de accessors.
