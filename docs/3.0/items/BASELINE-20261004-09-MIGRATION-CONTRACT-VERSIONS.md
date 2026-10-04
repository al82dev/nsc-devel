# Baseline 2026-10-04: versiones del contrato de migración

## Problema

El contrato generado ya congelaba NSMETA 1.37 y MIR 1.28, pero Stage 69 esperaba todavía NSMETA
1.33 y MIR 1.21. Las versiones nuevas corresponden a capacidades publicadas después de aquellos
valores, no a una variación no determinista.

## Cambio

La prueba de migración exige las versiones actuales `nsmeta=1.37` y `mir_schema=1.28`, conservando
sin cambios todas las demás cláusulas del contrato.

## Archivo

- `tests/unit/Stage69MigrationBaselineTests.cpp`

## Mejora

El baseline de migración vuelve a congelar la matriz real completa y seguirá fallando ante cualquier
cambio futuro no revisado.
