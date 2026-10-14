# NS304-T08-F06 — Imports, precedencia, ambigüedad y lookup incremental

- **Estado**: AUDITADO (nominal-antes-extensión ya es el orden; ranking/índices incrementales pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar imports, precedencia y ambigüedad*.
Plan T08-F06 (DeepSeekDocs): *orden normativo: miembro nominal aplicable antes de extensión; después
filtrar por imports/visibilidad/constraints y rankear sin depender del orden de archivo; mantener
índices incrementales invalidables por identidad, no cache textual*.

Estado real:

- **Nominal antes de extensión**: `bindExtension` ya es el último paso del member lookup
  (`ProjectMethodBodyModel.cpp:15445`), así que el miembro nominal gana.
- **Ranking**: sin ranking explícito por imports/visibilidad/constraints; la ambigüedad no se
  detecta/diagnostica de forma normativa.
- **Índices incrementales**: el `extensionBindingBySyntax_` es por nodo (cache textual), no por
  identidad.

## Gap

Falta el ranking normativo (filtrar por imports/visibilidad/constraints + rankear sin orden de
archivo) y los índices incrementales por identidad (invalidables, no cache textual).

### Verificación (2026-10-13)

- La precedencia nominal-antes-extensión y el despacho básico de extensión funcionan
  (`extension Box { int Twice() }` → `b.Twice()` exit 1). Lo pendiente es el ranking/ambigüedad
  normativos y los índices por identidad (un vertical de determinismo, no una surface).

## Nota

F06 depende de la precedencia (ya en orden) + el ranking + los índices. No es una surface nueva; es
el determinismo de la resolución de extensiones.
