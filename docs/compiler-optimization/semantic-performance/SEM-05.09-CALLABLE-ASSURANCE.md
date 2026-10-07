# SEM-05.09 — Aseguramiento del aislamiento por callable

**Estado:** implementado y verificado de forma dirigida

**Rango principal:** `d33627b59..1a6effc7d`

**Correccion de cierre:** `a83e359a2`

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

| Criterio de la tanda 5 | Resultado | Evidencia |
|---|---|---|
| Estado mutable exclusivo por callable | Cumplido | Cada worker posee un shard y devuelve un resultado movible |
| Fusion independiente del orden de finalizacion | Cumplido | Reductor ordena por oleada y ordinal; prueba 5/5 |
| Diagnosticos y dumps estables | Cumplido dirigido | Modelo de cuerpos 76/76 y Driver O0 6/6 |
| Descubrimientos canonicos deduplicados | Cumplido | Reductor y fixture de multiples oleadas |
| Resumen de escape no se pierde en el handoff | Cumplido | Correccion `a83e359a2`; escape summaries 8/8 |
| Ausencia de recursion nueva | Cumplido por inspeccion | Shard, merger y reducer usan recorridos iterativos |

## Ledger

| Commit | Responsabilidad |
|---|---|
| `d33627b59` | Extrae la frontera de analisis de callable |
| `e297c928c` | Entrega contexto inmutable |
| `fc64f4237` | Define el resultado aislado |
| `cf2251517` | Comparte el catalogo inmutable de adapters |
| `8cb7e1dd0` | Analiza cada callable en un shard exclusivo |
| `4cb6167d0` | Fusiona resultados por tipos y orden estable |
| `d97ffe2c9` | Conduce descubrimientos desde resultados aislados |
| `e778ec5b8` | Reduce oleadas deterministamente |
| `b34e09d9b` | Publica contadores de aseguramiento |
| `1a6effc7d` | Documenta la arquitectura aislada |
| `a83e359a2` | Compacta y preserva el resumen de escape del shard |

## Archivos

- `ProjectCallableAnalysisStatistics.h`;
- `ProjectMethodBodyModel.h/.cpp`;
- `ProjectCallableResultMerger.cpp`;
- `ProjectCallableWaveReducer.cpp`;
- `ProjectCallableWaveReducerTests.cpp`;
- `diagrams/src/callable-isolation-and-reduction.dot`;
- este documento.

## Validación dirigida

En el corte final pasaron:

- `nsc_project_callable_analysis_result_tests`: 6/6;
- `nsc_project_callable_wave_reducer_tests`: 5/5;
- `nsc_project_method_body_model_tests`: 76/76;
- `nsc_project_callable_wave_driver_tests`: 6/6 en O0;
- `nsc_ns3_t06_escape_summary_tests`: 8/8.

El merger queda limpio bajo clang-tidy dirigido. El TU monolitico del modelo no termino su pasada en 150 segundos y
no se declara limpio. No se ejecuto baseline, minibaseline, CTest general ni optimizaciones O1/O2/O3.

## Limites

El aislamiento es por callable dentro de una construccion del modelo. No implementa cache persistente, sesion
incremental ni aislamiento global entre requests concurrentes. Esos contratos pertenecen al plan amplio de v3.0.4.
