# Tanda de validación 237 — orden canónico de dimensiones fijas

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. VALIDACIÓN DE PRODUCTO PENDIENTE.

## Causa confirmada

Cada grupo `[N]` se anexaba a `TypeSyntax` en orden textual. Como Sema aplica
los modificadores de dentro hacia fuera, `int[2][3]` terminaba representado
como un array exterior de tres elementos con arrays interiores de dos. La
segunda indexación válida `a[1][2]` se diagnosticaba fuera de rango.

## Corrección

`TypeSyntax::addArrayModifierGroup` inserta iterativamente cada nuevo grupo de
corchetes antes de los grupos de array fijo ya reunidos. Las dimensiones de un
solo grupo multidimensional conservan su orden interno canónico y los
punteros siguen delimitando la aplicación de modificadores.

## Alcance

La causa corresponde a `TS-SEMA-208`. No se modificaron los límites ni el
cálculo de layout de un array ya canonicalizado.

## Validación

No se compiló ni ejecutó por orden del usuario. La validación queda delegada a
su corrida posterior.
