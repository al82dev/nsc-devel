# SEM-05.08 — Reducción determinista de resultados aislados

## Propósito

Cerrar la frontera entre análisis aislado y publicación global antes de introducir workers.
El orden de finalización de los callables no puede alterar vectores, diagnósticos ni trabajo
descubierto.

## Cambio

`ProjectCallableWaveReducer` recibe todos los resultados terminados de una oleada y ejecuta
una única reducción coordinada:

1. ordena resultados por `(waveOrdinal, callableOrdinal)`;
2. fusiona shards en ese orden estable;
3. reduce diagnósticos mediante el reductor canónico por ubicación de fuente;
4. aplica el presupuesto global de errores una sola vez;
5. devuelve descubrimientos ordenados y sin duplicados.

`ProjectCallableResultMerger` dejó de publicar diagnósticos. Su responsabilidad queda limitada
a remapear y anexar el estado semántico del shard.

## Mejora

- el análisis no publica estado global mientras produce un resultado;
- invertir el orden de finalización conserva exactamente los diagnósticos;
- los descubrimientos ya no dependen del orden del worker;
- la tanda 6 puede sustituir la producción serial por workers sin cambiar la reducción.

## Archivos

- `ProjectCallableWaveReducer.h/.cpp`;
- `ProjectCallableResultMerger.h/.cpp`;
- `ProjectMethodBodyModel.cpp`;
- `ProjectCallableWaveReducerTests.cpp`;
- `CMakeLists.txt`;
- este documento.

## Validación dirigida

- prueba unitaria que invierte el orden de llegada de dos resultados;
- prueba de method bodies;
- fixtures O0 de borrow/escape y escape unificado con NSLink de `A:`.

No se ejecuta baseline, minibaseline, CTest general ni optimizaciones O1/O2/O3.
