# Tanda de validación 189 — cierre y reutilización del pool Driver

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN CONJUNTA PENDIENTE.

## SCOPE LOCK

La tanda modifica exclusivamente el oráculo de cancelación del worker pool y
este documento. El producto y los demás contratos permanecen protegidos.

## Causa y corrección

El oráculo consultaba `cancellationRequested()` sólo después de `wait()`, aunque
el contrato del pool cierra la solicitud al alcanzar reposo. Ahora captura que
la cancelación fue observable antes de liberar la tarea activa, comprueba que
quedó cerrada después de `wait()` y envía una tarea adicional para demostrar
que el pool sigue siendo reutilizable. También conserva la comprobación exacta
del trabajo activo y de las tareas descartadas.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación Debug/O0 se hará
conjuntamente después de implementar todas las causas inventariadas.
