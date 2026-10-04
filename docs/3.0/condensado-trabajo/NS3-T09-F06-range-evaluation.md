# NS3-T09-F06 — Fusión y garantías de evaluación / Fusion and evaluation guarantees

## Español

### Resultado

La optimización de pipelines ya no pregunta solamente si dos operadores “parecen lazy”.
`analyzeFusion` produce un veredicto causal y conserva como contrato observable:

- evaluación perezosa, sin materialización adelantada;
- orden de efectos de izquierda a derecha;
- categoría single-pass o multi-pass;
- evaluación a lo sumo una vez de cada item single-pass;
- short-circuit de adaptadores como `take`, `first` o `any`;
- lifetime de items prestados, que no pueden atravesar un buffer oculto.

El resultado de la fusión sólo conserva categoría multi-pass cuando fuente y adaptador ya la
garantizan. Cualquier prueba de optimización ausente rechaza conservadoramente la fusión; el
compilador conserva entonces el pipeline original en vez de cambiar semántica por rendimiento.

### Diagnóstico interno

El veredicto diferencia `EagerBoundary`, `EffectReordering`, `TraversalUpgrade`,
`RepeatedEvaluation`, `ShortCircuitLost` y `BorrowedItemBuffered`. Esta causa puede exponerse en
MIR/cost explorer sin volver a deducir el motivo desde un booleano.

### Archivos y validación

- `include/nativesharp/Language/IterationSemantics.h`: contrato de evaluación, solicitud y causa.
- `src/Language/IterationSemantics.cpp`: análisis conservador y compatibilidad de `mayFuse`.
- `tests/unit/NativeSharp3T09RangeEvaluationTests.cpp`: ocho comprobaciones puntuales.
- `CMakeLists.txt`: target aislado `nsc_ns3_t09_range_evaluation_tests`.

La prueba cubre laziness, orden, single/multi-pass, repetición, short-circuit, borrowing y el helper
histórico. No se ejecutó baseline, CTest general ni matriz de optimizaciones.

## English

### Result

Pipeline optimization no longer asks only whether two operators “look lazy”. `analyzeFusion`
returns a causal verdict and preserves the following observable contract:

- lazy evaluation with no early materialization;
- left-to-right effect order;
- single-pass or multi-pass traversal category;
- at-most-once evaluation for each single-pass item;
- short-circuit behavior for adapters such as `take`, `first` or `any`;
- borrowed-item lifetime, which cannot cross a hidden buffer.

The fused result remains multi-pass only when both source and adapter already guarantee it. A
missing optimizer proof rejects fusion conservatively, preserving the original pipeline instead of
trading semantics for speed.

### Internal reason

The verdict distinguishes `EagerBoundary`, `EffectReordering`, `TraversalUpgrade`,
`RepeatedEvaluation`, `ShortCircuitLost` and `BorrowedItemBuffered`. MIR and the cost explorer can
surface that reason without reverse-engineering it from a boolean.

### Focused validation

The dedicated eight-check executable covers laziness, ordering, traversal category, repetition,
short-circuiting, borrowing and the compatibility helper. No baseline, general CTest suite or
optimization matrix was run.
