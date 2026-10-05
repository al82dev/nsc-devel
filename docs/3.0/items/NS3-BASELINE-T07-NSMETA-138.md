# NS3 baseline: T07 alineado con NSMETA 1.38

## Problema

T07 verificaba correctamente los contratos de typed throws, pero todavía exigía que el productor
actual emitiera NSMETA 1.37. El formato vigente es 1.38, por lo que el round-trip era válido y aun
así la aserción fallaba.

## Cambio

La expectativa y su descripción usan ahora NSMETA 1.38. La prueba negativa conserva el límite
histórico correcto: degradar typed throws por debajo de 1.37 continúa prohibido.

## Archivos

- `tests/unit/NativeSharp3T07ErrorTests.cpp`

## Mejora

T07 distingue la versión del productor actual de la versión mínima que introdujo typed throws y ya
no confunde una evolución aditiva del formato con una pérdida del contrato de errores.
