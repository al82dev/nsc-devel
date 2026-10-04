# Baseline 2026-10-04: cardinalidad del contrato runtime

## Problema

El contrato bootstrap runtime ABI 1.2 y symbol set 4 publica catorce símbolos, incluido el filtro de
`panic`. La prueba de infraestructura todavía fijaba la cardinalidad anterior de trece.

## Cambio

La expectativa se alinea a los catorce nombres enumerados por el propio contrato versionado.

## Archivo

- `tests/selfhost/test_bootstrap_infrastructure.py`

## Mejora

La prueba vuelve a detectar adiciones o pérdidas accidentales sin reportar como defecto el símbolo
que ya forma parte del ABI runtime 1.2.
