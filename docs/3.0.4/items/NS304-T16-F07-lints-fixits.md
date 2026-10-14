# NS304-T16-F07 — Lints, fix-its y migración

- **Estado**: AUDITADO (lints/fix-its existen; actualización por cambios T03–T15 + guía sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar lints, fix-its y migración*.
Plan T16-F07 (DeepSeekDocs): *actualizar reglas sólo para cambios reales; fix-it produce sintaxis
válida, conserva intención y es idempotente; la guía ejecutable enlaza cada cambio con fixture
antes/después y no presenta propuestas como implementación*.

Estado real:

- Los lints/fix-its existen; la actualización por los cambios reales de T03–T15 + la guía ejecutable
  antes/después no están cerradas.

## Gap

Falta actualizar las reglas sólo por cambios reales + el fix-it idempotente (sintaxis válida +
intención conservada) + la guía ejecutable con fixtures antes/después.

## Nota

F07 depende de los lints existentes + los cambios T03–T15. No es una surface nueva; es la actualización
de reglas + la guía de migración.
