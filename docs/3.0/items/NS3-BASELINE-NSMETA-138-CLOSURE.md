# NS3 baseline: cierre de NSMETA 1.38 y MIR 1.30

## Problema

El compilador ya publica NSMETA 1.38 y MIR 1.30, pero el baseline de migración todavía esperaba
1.37/1.29 y la matriz final describía al compilador como consumidor NSMETA 1.37. Además, al avanzar
el formato no se archivó la fixture 1.37 que ahora forma parte de la ventana histórica legible.

## Cambio

- El baseline de migración fija NSMETA 1.38 y MIR 1.30.
- La matriz N/N-1 identifica NSMETA 1.38 como consumidor actual y conserva 1.21 como productor N-1.
- Stage 48 archiva `tests/fixtures/nsmeta/historical-1.37.nsmeta` mediante su mecanismo de refresh
  inmutable; no se reescriben las fixtures anteriores.

## Archivos

- `tests/unit/Stage69MigrationBaselineTests.cpp`
- `tests/unit/Item405FinalCompatibilityCertificationTests.cpp`
- `tests/fixtures/nsmeta/historical-1.37.nsmeta`

## Mejora

Los contratos ejecutables, el codec y la evidencia histórica vuelven a describir la misma ventana
de compatibilidad, sin rebajar el formato actual ni ampliar el productor N-1 certificado.
