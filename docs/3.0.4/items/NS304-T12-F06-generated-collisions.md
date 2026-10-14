# NS304-T12-F06 — Generated members, colisiones, NSMETA e incremental

- **Estado**: AUDITADO (macro/derive generan members; identidad/procedencia + colisiones pendientes)
- **Rama**: development

## Auditoría (línea base)

Contrato (tracking): *Implementar generated members y colisiones*.
Plan T12-F06 (DeepSeekDocs): *asignar identidad y procedencia a miembros de macro/derive; mezclar
fuentes y generados por la misma regla; detectar colisiones con macro backtrace; cache e invalidación
por identidad/inputs, no orden; publicar una sola superficie NSMETA y comparar build limpio versus
incremental*.

Estado real:

- Macro/derive generan members (metaprogramación de declaraciones), pero la identidad/procedencia
  canónica de los generados + la detección de colisiones (con backtrace) no están unificadas con la
  regla de members parciales.

## Gap

Falta asignar identidad/procedencia a los members generados, mezclar fuentes y generados por la
misma regla, detectar colisiones con backtrace, cache/invalidación por identidad (no orden) y una
sola superficie NSMETA.

## Dependencia

F06 **no cierra** hasta integrarse con la generación tipada de **T14** (no falsear el orden).

## Nota

F06 es el cierre de T12 (identidad + generación) + el cola de NSMETA/incremental. Depende de F01
(identidad) y de T14 (generación tipada).
