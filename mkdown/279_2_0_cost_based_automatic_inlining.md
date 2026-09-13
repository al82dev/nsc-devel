# 279 — Inlining automático basado en costo / Cost-based automatic inlining

## Resumen / Summary

El compilador hace explícita la política automática por nivel: desactivada en O0, conservadora en
O1, balanceada en O2 y agresiva en O3. La decisión concreta permanece en el modelo target-aware del
pipeline canónico de LLVM; NativeSharp no duplica umbrales ni inventa una heurística paralela.

The compiler now makes automatic policy explicit per level: disabled at O0, conservative at O1,
balanced at O2, and aggressive at O3. Concrete decisions remain in LLVM's canonical target-aware
pipeline; NativeSharp does not duplicate thresholds or invent a competing heuristic.

## Archivos y funciones / Files and functions

- `include/nativesharp/LLVM/LlvmOptimizer.h` — declara `AutomaticInliningCostModel`, su configuración
  y la consulta canónica.
- `src/LLVM/LlvmOptimizer.cpp` — mapea nivel, intención y pipeline; `optimize()` consume ese mapa.
- `tests/unit/LlvmOptimizationTests.cpp` — prueba el mapa y la expansión real de una llamada barata.
- `docs/1.0.1/` — actualiza roadmap, changelog, estado, especificación, contrato, libro y conformidad.

## Implementación y decisiones / Implementation and decisions

El mapa devuelve simultáneamente el modelo y el texto de pipeline. Esta unión es deliberada: evita
que una ruta futura declare O1 conservador mientras ejecuta accidentalmente `default<O3>`. O0
conserva `default<O0>` para sus legalizaciones, pero marca el inlining automático como desactivado.

The mapping returns cost intent and pipeline text together. This deliberately prevents a future
path from claiming conservative O1 while accidentally running `default<O3>`. O0 keeps its default
pipeline for legalization but reports automatic inlining as disabled.

LLVM conserva autoridad sobre costos de instrucciones, target y rentabilidad. Fijar thresholds
numéricos en NSC duplicaría política dependiente de versión/arquitectura y haría más frágil la
evolución del backend. La reproducibilidad se define por fuente, target, LLVM y nivel iguales.

LLVM remains authoritative for instruction costs, target information, and profitability. Numeric
thresholds in NSC would duplicate version- and architecture-dependent policy. Reproducibility is
therefore defined for identical source, target, LLVM version, and level.

## Evidencia dirigida / Focused evidence

La fixture contiene dos llamadas locales a `Square`. El IR crudo y O0 conservan llamadas; O1, O2 y
O3 las eliminan y preservan el resultado. La prueba también exige la correspondencia exacta entre
configuración automática y pipeline.

The fixture contains two local `Square` calls. Raw IR and O0 retain calls; O1, O2, and O3 remove
them while preserving the result. The test also requires an exact configuration-to-pipeline match.

No se ejecutó baseline para esta entrega. / No baseline was run for this delivery.

## Commit de referencia / Reference commit

**Título / Title**

`Formaliza el inlining automático por costo / Formalize cost-based automatic inlining`

**Descripción / Description**

`Mapea O0-O3 a una política target-aware única, conserva O0 sin expansión automática y certifica que cada nivel habilitado inlinea llamadas locales rentables. / Maps O0-O3 to one target-aware policy, keeps O0 free of automatic expansion, and certifies that every enabled level inlines profitable local calls.`
