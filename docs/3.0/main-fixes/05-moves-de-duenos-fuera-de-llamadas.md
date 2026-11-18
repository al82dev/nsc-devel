# Moves de dueños fuera de llamadas en la regla conservadora

Rama `development-main-fixes`. Continúa el documento 04 (puntos M2-02, M2-03 y M2-04 del plan
`A:\nsc-main-fixes-evidencia\claude\PLAN-MEJORAS-PENDIENTES.md`). Revisión de partida: 713a736cc.

## SCOPE LOCK

- Objetivo: registrar como eventos de vida los `move` de un dueño local rastreado que no van a un parámetro.
- Naturaleza: funcional, dentro del contrato del documento 04 (decisión del usuario del 13 de noviembre).
- Archivos: `src/Frontend/Bodies/AssignmentAnalyzer.cpp`, `src/Frontend/Bodies/StatementAnalyzer.cpp`,
  `src/Frontend/Bodies/MethodBodyCallableLifecycle.cpp`, `include/nativesharp/Frontend/ProjectMethodBodyModel.h`,
  `tests/unit/NativeSharp3T06EscapeSummaryTests.cpp`, este documento. Protegidos y prohibiciones: documento 04.

## Causa

El replay sólo registraba los `move` entregados a un parámetro `owned`. `holder.held = move(owner)` dejaba al
dueño original como vivo, y `T inner = move(owner)` no transfería el dueño. Además un callable que mueve un dueño
sin llamada no era candidato del replay.

## Contrato

- `destino = move(dueño)`: si el destino es un local RAII, ese local pasa a ser el dueño (su objeto anterior se
  limpia primero); si es un campo, un elemento u otro lugar, el nuevo dueño no se rastrea y los objetos pasan a
  fin posible (`releaseUnproven`).
- `T local = move(dueño)`: un local RAII no estático pasa a ser el dueño; cualquier otro local, fin posible.
- Es candidato del replay todo callable con un arreglo nativo local que contiene un `move(...)` fuera de lambdas.
- `delete` de un local dueño de clase no es un evento posible: el lenguaje lo rechaza
  (`explicit delete of a RAII local is forbidden`), incluido un local declarado `owned` (M2-03).
- `return move(dueño)` termina el callable: no hay lectura posterior en él; una referencia que escapa en un arreglo
  recibido o guardado en un campo queda fuera del dominio (M2-04, ver M2-06 y M2-07). Un campo estático `owned` no
  existe en el lenguaje (`static field ... cannot be owned`).

## Pruebas

`nsc.ns3.t06.escape-summaries` (`unprovenControl`): rechazo de `holder.held = move(owner)` seguido de un uso;
aceptación de `next = move(owner)` hacia un local vivo; destrucción demostrada cuando el local que recibió el move
termina su bloque; rechazo de `delete` de un local `owned` por la regla existente. Pasan también
`nsc.stage50_7.borrow-escape-analysis`, `nsc.class-value-origins` y `nsc.stage56.c7-definite-assignment`
(`claude\logs\m2-02-*.log`). Las sondas conservan el resultado del documento 04 (`RESUMEN-m2-02.txt`).

## Límites

El dueño que recibe un campo no se sigue: su vida se considera no demostrada. No se ejecutó la batería completa.
