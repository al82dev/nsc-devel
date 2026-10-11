# Tanda de validación 190 — estructura de la guía de migración

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN CONJUNTA PENDIENTE.

## SCOPE LOCK

La tanda modifica únicamente el oráculo de la guía ejecutable de migración,
la descripción vigente de `Host.Driver` y este documento. El productor y los
demás contratos permanecen protegidos.

## Causa y corrección

El oráculo dependía del sentinel privado `NSMIGRATION-GUIDE`, que no pertenece
al formato generado. Ahora exige los encabezados canónicos del documento. Para
fuente vacía comprueba un único capítulo `VERIFY` y su encabezado numerado; para
fuente con transformaciones exige capítulos y los bloques `Before:` y `After:`.
La variante inválida conserva la comprobación de rechazo de identidad vacía.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación Debug/O0 se hará
conjuntamente después de implementar todas las causas inventariadas.
