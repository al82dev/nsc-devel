# SEM-05.07 — Descubrimiento conducido por resultados

## Propósito

Eliminar los cuatro cursores globales que reescaneaban sufijos acumulados de llamadas,
delegates, optional y propagación de Result después de cada oleada.

## Cambio

Cada shard ya publica en `ProjectCallableAnalysisResult` los ordinales globales que descubrió.
El coordinador:

1. consume esos ordinales antes de mover el resultado al merger;
2. conserva únicamente especializaciones genéricas diferidas;
3. deduplica contra el conjunto global `demandedGenericMethods`;
4. ordena el conjunto de la oleada;
5. lo agrega como la siguiente oleada.

## Mejora

- desaparecen `callDemandCursor`, `delegateDemandCursor`, `optionalDemandCursor` y
  `resultPropagationDemandCursor`;
- ningún descubrimiento depende de la posición de un vector global;
- el contrato queda listo para recopilar descubrimientos de workers en cualquier orden y
  reducirlos de forma determinista.

## Archivos

- `ProjectMethodBodyModel.cpp`;
- este documento.

## Validación dirigida

Build y ejecución individual de las pruebas de method bodies y escape unificado en O0. No
baseline, minibaseline, CTest general ni O1/O2/O3.
