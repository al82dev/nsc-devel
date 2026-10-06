# SEM-05.09 — Aseguramiento del aislamiento por callable

## Propósito

Hacer observable la nueva frontera aislada sin depender de tiempos, texto de consola ni hashes.

## Contadores estructurales

`ProjectCallableAnalysisStatistics` publica:

- callables encolados;
- callables analizados y omitidos;
- resultados reutilizados —cero hasta incorporar caché incremental—;
- shards creados;
- resultados fusionados;
- oleadas reducidas;
- descubrimientos canónicos publicados.

Los contadores pertenecen al modelo raíz. Los shards no comparten ni actualizan este estado;
el coordinador registra creación y análisis, y la reducción serial registra publicación.

## Arquitectura

La fuente versionada
[`callable-isolation-and-reduction.dot`](diagrams/src/callable-isolation-and-reduction.dot)
documenta la frontera exacta entre catálogos inmutables, shard exclusivo, resultado movible y
reducción serial determinista. El generador común producirá el SVG cuando Graphviz esté
disponible; la ausencia local de esa herramienta no modifica ni bloquea el compilador.

## Criterios comprobables

- `resultsMerged == shardsCreated` al concluir un análisis sin cancelación;
- invertir el orden de llegada no altera los contadores de reducción;
- `callablesReused == 0` documenta honestamente que esta tanda no implementa caché;
- ningún contador participa en decisiones semánticas.

## Archivos

- `ProjectCallableAnalysisStatistics.h`;
- `ProjectMethodBodyModel.h/.cpp`;
- `ProjectCallableResultMerger.cpp`;
- `ProjectCallableWaveReducer.cpp`;
- `ProjectCallableWaveReducerTests.cpp`;
- `diagrams/src/callable-isolation-and-reduction.dot`;
- este documento.

## Validación dirigida

Prueba unitaria del reductor, prueba de method bodies y fixtures O0 de escape. Sin baseline,
minibaseline, CTest general ni optimizaciones O1/O2/O3.
