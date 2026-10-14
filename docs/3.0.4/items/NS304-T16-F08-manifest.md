# NS304-T16-F08 — Manifest de superficie

- **Estado**: AUDITADO (Language3Surface + manifest existen; regeneración desde inventario sin cerrar)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar manifest de superficie*.
Plan T16-F08 (DeepSeekDocs): *regenerar desde inventario canónico, no por edición manual; ninguna
feature T03–T15 queda `DeferredTo4`/`PlannedFor304`/helper-only; validar que cada `implemented` apunte
a pruebas, símbolos, metadata y self-host cuando aplique; un residual bloquea el cierre*.

Estado real:

- `Language3Surface` + el manifest de capacidades existen; la regeneración desde el inventario canónico
  + la validación de que cada `implemented` apunta a evidencia (pruebas/símbolos/metadata/self-host) no
  está cerrada.

## Gap

Falta regenerar el manifest desde el inventario (no manual) + validar que ningún feature T03–T15 quede
diferida/helper-only + que cada `implemented` apunte a pruebas/símbolos/metadata/self-host.

## Nota

F08 es el cierre de T16 (y la condición para T17). Depende de F01–F07 + el inventario canónico. Un
residual de T03–T15 bloquea el cierre.
