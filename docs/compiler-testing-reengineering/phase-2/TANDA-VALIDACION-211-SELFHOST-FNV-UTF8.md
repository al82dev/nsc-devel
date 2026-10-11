# Validacion 211 - Vectores FNV UTF-8 de SelfHost.Basic

## Alcance

`TSH-00019`, contrato `CSH-BAS-003`.

## Causa confirmada

El oraculo congelaba los hashes de `é` y `ñ` como si cada texto aportara un
solo byte Latin-1. En NativeSharp, `string` expone la representacion UTF-8 que
recorre `StableHash.Text`; ambos caracteres aportan dos bytes.

## Correccion

Los vectores esperados se alinean con los bytes UTF-8 reales:

- `é`: `775207407765167617`;
- `ñ`: `775181019486090553`.

El algoritmo FNV-1a y el producto self-host permanecen sin cambios.

## Validacion

No se compilo ni se ejecutaron pruebas por instruccion expresa. La validacion
queda a cargo del usuario al cerrar todas las correcciones.
