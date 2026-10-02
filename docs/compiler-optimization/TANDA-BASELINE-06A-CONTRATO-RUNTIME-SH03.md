# Tanda baseline 06A — Contrato runtime SH03 alineado

## Problema

SH03 comparaba el runtime actual contra valores históricos:

- contrato esperado: `NSRUNTIMEABI 1.0`, `symbol_set=2`;
- runtime del staging y del árbol actual: `NSRUNTIMEABI 1.1`, `symbol_set=3`.

El staging `A:\native-sharp-3.0-baseline` no estaba atrasado. La expectativa obsoleta estaba en el
contrato fuente de SH03.

## Cambio

`selfhost/contracts/runtime-bootstrap.contract.json` ahora exige ABI `1.1` y conjunto de símbolos
`3`, que coinciden con el contrato generado por NativeSharp 3.0.

## Archivos

- Modificado: `selfhost/contracts/runtime-bootstrap.contract.json`.
- Nuevo: este documento, para registrar causa, corrección y validación.

## Mejora

SH03 deja de rechazar el runtime 3.0 correcto como si fuera una versión incompatible. No se
relajaron símbolos requeridos, target, pointer model ni calling convention.

## Validación

- `nsc.selfhost.llvm-c-bridge`: 1/1 aprobado en 2.67 s.
- SH03 validó LLVM estático sin `LLVM-C.dll`, el runtime ABI 1.1/symbol set 3 y el enlace NSLink.
- No se ejecuta baseline.
- No se recompilan librerías.
- No se modifica `C:\native-sharp`.
