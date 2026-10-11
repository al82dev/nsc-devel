# Tanda de validación 192 — CF-INTEROP-018

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente el oráculo compartido por
`TF-INTEROP-137` a `TF-INTEROP-144`, pertenecientes a `CF-INTEROP-018`. No
modifica `LibraryApiDiffer`, el compilador, el plan, el inventario ni el
manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

El contrato de `LibraryApiDiffer::classification` da prioridad a un conjunto
de cambios vacío y lo clasifica como `identical`. El fixture dejaba
`report.changes` vacío, cambiaba solamente los indicadores de compatibilidad y
esperaba categorías incompatibles con el estado vacío. Las ocho variantes
ejercitaban el mismo fixture inválido.

La taxonomía vigente es `identical` para cero cambios, `additive` para cambios
compatibles, `source-breaking` para ruptura sólo de fuente, `abi-breaking`
para ruptura sólo ABI y `source-and-abi-breaking` para ambas rupturas.

## Corrección

El oráculo comprueba primero `identical`, añade después un cambio observable y
recorre las cuatro combinaciones restantes de compatibilidad. De este modo las
banderas se evalúan dentro de su precondición real y cada categoría conserva
su significado exacto.

No se relajó la prueba, no se añadió fallback y no se cambió el producto. El
plan, el inventario y el manifiesto conservan los identificadores, perfiles,
rutas y contrato correctos; no requieren sincronización.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas se implementan primero y se validan juntas al final.
