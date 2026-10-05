# NS3 baseline: inventario runtime actualizado

## Problema

El contrato SH48 congelaba 81 fuentes entre sus cinco raíces de biblioteca. La incorporación de
`NativeSharp.Core.Threading/ReferenceCounting.ns` elevó el inventario real a 82, pero el contador
del contrato no se actualizó; el verificador fallaba antes de evaluar completamente la emisión.

## Cambio

`runtime-library.contract.json` declara ahora 82 fuentes. Se verificó el inventario por raíz:
41 Core, 23 Collections.Generic, 4 Core.IO, 4 Diagnostics y 10 Threading.

## Archivos

- `selfhost/contracts/runtime-library.contract.json`

## Mejora

SH48 vuelve a medir exactamente el conjunto runtime actual y mantiene su función de detectar tanto
fuentes omitidas como incorporaciones no registradas.
