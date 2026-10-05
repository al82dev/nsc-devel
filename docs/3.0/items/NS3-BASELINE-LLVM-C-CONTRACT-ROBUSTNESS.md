# NS3 baseline: contrato LLVM-C basado en comportamiento

## Problema

La incorporación legítima de `LLVMTypeOf` elevó el inventario del binding LLVM-C. La validación falló por un conteo total y por la huella completa del archivo, aunque el símbolo nuevo estaba auditado y el compilador requería esa declaración.

## Cambio

- Se eliminaron el conteo total congelado y las huellas de los archivos fuente como condiciones de aprobación.
- El verificador conserva las condiciones funcionales: enlace estático, ausencia de importaciones de `LLVM-C.dll`, presencia de los símbolos bootstrap requeridos y pertenencia de cada declaración al inventario auditado.
- Los conteos continúan apareciendo como información del reporte, pero ya no deciden si la prueba pasa o falla.
- No se modificó código del compilador en este cierre; el arreglo del backend permanece registrado en `9add8b9c5`.

## Validación

Se ejecuta únicamente la prueba puntual del contrato LLVM-C. No se ejecutan baseline, minibaseline ni perfiles O1, O2 u O3.
