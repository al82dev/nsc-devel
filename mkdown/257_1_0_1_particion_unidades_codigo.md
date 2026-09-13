# Entrega 257 — Partición en unidades de código / Module code partitioning

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 257.
- Resultado / Result: el MIR ejecutable tiene un plan explícito, balanceado y determinista de
  unidades de código listo para la generación LLVM/OBJ paralela.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

The new `ProjectOopCodePartition` turns the flat canonical MIR function sequence into bounded code
units. It records stable unit ordinals/keys, estimated work, canonical function ordinals and the
reverse function-to-unit mapping. This is an explicit backend boundary rather than an implicit loop
inside the driver.

Sólo las funciones con definición entran al reparto. Los imports C y native son declaraciones que
deberán estar disponibles en cada módulo LLVM que las necesite, pero no representan trabajo de
emisión propio y por ello no pertenecen a ninguna unidad.

Work is estimated by counting the complete MIR node tree for each function. Candidates use a stable
longest-processing-time assignment: descending cost, physical function name and original MIR
ordinal; each goes to the least-loaded unit, with unit ordinal as the final tie-breaker. This avoids
the severe imbalance of contiguous source chunks without making scheduling order observable.

After assignment, each unit restores its functions to canonical MIR order. Unit identities use the
fixed `code-NNNN` form. The number of units is clamped between one and the number of definitions, so
no plan contains empty work units. A project without definitions produces a valid empty plan.

## Archivos / Files

- `include/nativesharp/LLVM/ProjectOopCodePartition.h`: public code-unit and partition-plan model.
- `src/LLVM/ProjectOopCodePartition.cpp`: saturated MIR cost calculation, stable balancing,
  canonical publication, reverse lookup and deterministic dump.
- `CMakeLists.txt`: integration in the LLVM backend library.
- `tests/unit/ProjectOopLoweringTests.cpp`: serial/parallel plan equivalence, unique coverage,
  stable keys and exclusion of imports.
- Compiler contract, conformance map and 2.0 roadmap: normative behavior and traceability.

## Decisiones / Decisions

1. Partición previa a LLVM: the expensive backend receives an immutable work plan.
2. Costo desde MIR: units reflect executable body size without depending on LLVM timing noise.
3. LPT determinista: balance improves while every tie has a canonical answer.
4. Imports sin propietario: declarations are not duplicated as definition work.
5. Orden MIR dentro de cada unidad: output does not inherit scheduling order.
6. Mapa inverso explícito: later codegen and diagnostics need constant-time ownership lookup.

## Aceptación / Acceptance

- Cada función definida pertenece exactamente a una unidad.
- Ninguna función importada pertenece a una unidad.
- No existen unidades vacías y el conteo solicitado se limita de forma segura.
- Equivalent serial and parallel MIR produce exactly the same partition dump.
- Reverse lookup agrees with every unit membership entry.
- La prueba enfocada OOP lowering pasa 79 checks.
- Reconstrucción limpia de `nsc` pasa; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Particiona el módulo en unidades de código / Partition module into code units`

**Descripción / Description**

`Introduce a deterministic MIR-to-code-unit plan, estimate per-function work from complete MIR
trees, balance definitions through stable LPT assignment, preserve canonical order and reverse
ownership lookup, exclude imports from definition work, and verify exact coverage and determinism.`
