# SEM-04.06 — Aseguramiento del evaluador de referencias

## Propósito

Hacer observable que la resolución de orígenes de referencias utiliza la máquina iterativa y
no un fallback recursivo oculto.

## Métricas

`ProjectEscapePropagationStatistics` publica:

- `referenceOriginEvaluations`: consultas atendidas por el evaluador;
- `referenceContinuationFrames`: continuaciones explícitas creadas;
- `maximumReferenceContinuationDepth`: profundidad máxima de la pila alojada en heap;
- `recursiveReferenceOriginFallbacks`: debe permanecer en cero.

La instrumentación no modifica el resultado semántico. La estructura de estadísticas se marca
`mutable` porque medir una consulta const no cambia el modelo compilado.

## Cobertura dirigida

El fixture seguro de Stage 50.7 exige actividad real del evaluador, cero rescans globales, cero
fallback recursivo de VLA y cero fallback recursivo de origen de referencia. El fixture dirigido
Item 315 recorre además aliases, llamadas con retorno `ref`, storage estático y punteros para
validar el comportamiento de las continuaciones.

## Archivos

- `ProjectEscapePropagationStatistics.h`;
- `ProjectMethodBodyModel.h`;
- `ProjectReferenceEscapeEvaluator.h/.cpp`;
- `tests/unit/Stage50_7BorrowEscapeAnalysisTests.cpp`;
- este documento.

## Validación

Build y ejecución individual de `nsc_stage50_7_borrow_escape_analysis_tests` y
`nsc_item315_unified_escape_analysis_tests` en O0. No se ejecuta baseline, minibaseline, CTest
general ni O1/O2/O3.
