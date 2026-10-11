# Tanda de validación 191 — CF-INTEROP-011

Fecha: 2026-10-10

## Estado

IMPLEMENTADA. PENDIENTE DE VALIDACIÓN FINAL.

## SCOPE LOCK

Esta causa corrige exclusivamente el oráculo compartido por
`TF-INTEROP-081` a `TF-INTEROP-088`, pertenecientes a `CF-INTEROP-011`. No
modifica `LibraryAbiReporter`, el compilador, el plan, el inventario ni el
manifiesto.

## Causa confirmada

Clasificación: `DEFECTO_DE_PRUEBA_ORACULO`.

`LibraryAbiReporter::write` ordena y publica cada tipo mediante
`LibraryMetadataType::identityKey`. El fixture construía correctamente
`identityKey = "type:Point"`, pero el oráculo buscaba
`fullyQualifiedName = "Test.Point"`, campo que el formato ABI no serializa.
Por ello las ocho variantes rechazaban una salida válida con el mismo mensaje.

## Corrección

El oráculo exige ahora la línea estructural `type type:Point kind=struct` y
comprueba así la identidad canónica que realmente forma parte del reporte ABI.
No se relajó la prueba, no se añadió fallback y no se cambió el producto.

El plan, el inventario y el manifiesto conservan los identificadores, perfiles,
rutas y contrato correctos; no requieren sincronización.

No se compiló ni ejecutó la prueba por instrucción del usuario: todas las
causas se implementan primero y se validan juntas al final.
