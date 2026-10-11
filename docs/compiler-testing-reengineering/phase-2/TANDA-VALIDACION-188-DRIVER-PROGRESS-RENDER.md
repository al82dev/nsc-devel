# Tanda de validación 188 — render de progreso Driver

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN CONJUNTA PENDIENTE.

## SCOPE LOCK

La tanda modifica exclusivamente el oráculo de `CF-DRIVER-012`, sus filas de
plan, inventario y manifiesto, y este documento. El producto, los productores
de telemetría y los demás contratos permanecen protegidos.

## Causa y corrección

El oráculo textual aceptaba cualquier salida que contuviera `NSC parse`. Esa
condición no distinguía el evento terminado del evento fallido ni verificaba
posición, contador o mensaje completo. El caso JSON ya comprobaba el esquema y
el escape del nombre, pero ahora ambas condiciones quedan expresadas como el
contrato observable exacto de cada variante.

Las variantes JSON exigen el esquema `nativesharp-progress-event` y
`unit\\\"42`; la variante fallida exige `-- NSC: failed parse unit"42`; las
restantes exigen `[2/3] NSC parse unit"42`.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación Debug/O0 se hará
conjuntamente después de implementar todas las causas inventariadas.
