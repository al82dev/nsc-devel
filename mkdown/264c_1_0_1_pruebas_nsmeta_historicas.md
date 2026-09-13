# Corrección 264C — Pruebas NSMETA históricas / Historical NSMETA tests

## Origen / Origin

El baseline 1/16 marcó `nsc.stage50_9.metadata-host-compatibility` como fallo y, bajo una de las dos
configuraciones de enlace, como `SegFault`. Los mensajes parecían atribuir el problema al lector de
NSMETA 1.2 y 1.3.

La reproducción aislada demostró que ambos archivos se escriben, decodifican y verifican
correctamente. El defecto estaba en el test: después de que `writeModel` adoptó el merger canónico,
seguía buscando `Optional` y `AfterParameter` por su posición de inserción. El orden serializado es
por identidad canónica, por lo que los dos miembros quedan legítimamente reordenados. Después de una
expectativa fallida, el caso 1.3 desreferenciaba el vector sin volver a validar su forma.

## Implementación / Implementation

The compatibility test now resolves decoded members by their stable name, verifies pointers and
parameter counts before access, and separates decode success from record inspection. It continues
to test the original binary invariant: fields introduced in NSMETA 1.3 and 1.4 must not shift a 1.2
or 1.3 payload, invent defaults, or invent `ref/out/in` identity.

No se relajó el lector, no se reescribieron goldens y no se cambió el formato NSMETA 1.12.

## Archivos / Files

- `tests/unit/Stage50_9MetadataHostCompatibilityTests.cpp`: adds nominal member lookup and guarded
  inspection for the 1.2/1.3 round trips.
- Compiler contract and conformance map: document canonical-order-safe compatibility evidence.

## Evidencia dirigida / Focused evidence

- `nsc_stage50_9_metadata_host_compatibility_tests`: 56 checks pass.
- Direct verification of the generated NSMETA 1.2 and 1.3 files succeeds.
- No additional baseline was run; this repairs the baseline 1/16 test root in isolation.

## Commit de referencia / Reference commit

**Título / Title**

`Estabiliza pruebas históricas de NSMETA / Stabilize historical NSMETA tests`

**Descripción / Description**

`Resolve down-level metadata records by canonical identity after writeModel ordering, guard every
decoded record before inspection, preserve the 1.2/1.3 alignment assertions, and prevent the
compatibility harness from crashing on a failed expectation.`
