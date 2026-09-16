# 405 — Certificación N-1/N final / Final N-1 and N certification

## Español

La matriz provisional calculaba N-1 restando el parche, lo que habría convertido erróneamente
`2.0.0` en `1.0.0`. La certificación final fija de forma explícita `1.0.1` y `2.0.0`. NSMETA 1.20,
Native ABI 1.0 y Runtime ABI 1.0 se conservan en ambas generaciones.

`NativeSharp2Compatibility.matrix` se genera, instala y verifica como parte del paquete. La matriz
declara aceptación de productores N-1 y N por el consumidor N, y rechazo del productor N por el
consumidor N-1.

## English

The provisional matrix computed N-1 by decrementing a version component, which would have turned
`2.0.0` into the incorrect `1.0.0`. Final certification explicitly fixes `1.0.1` and `2.0.0`.
NSMETA 1.20, Native ABI 1.0, and Runtime ABI 1.0 are preserved in both generations.

`NativeSharp2Compatibility.matrix` is generated, installed, and verified as part of the package.
It records that the N consumer accepts N-1 and N producers, while the N-1 consumer rejects N.
