# Entrega 263 — Invalidación incremental precisa / Precise incremental invalidation

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 263.
- Resultado / Result: un cambio aislado en el cuerpo de una función invalida únicamente su unidad
  de código cuando el cache verificado está activo.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

The code partitioner now has two explicit policies. `Balanced` preserves the existing
longest-processing-time distribution for ordinary builds. `StableIncremental` assigns every
physical function identity to a deterministic bucket with FNV-1a and is selected only when the
content-addressed artifact cache is enabled.

La política incremental no usa el costo estimado del cuerpo para decidir el bucket. Por ello,
cambiar instrucciones, constantes o complejidad de una función no redistribuye las demás. Los
buckets vacíos se omiten, pero los buckets sobrevivientes conservan su ordinal y su clave
`code-NNNN`; nunca se compactan ni renumeran.

Each emitted LLVM module still receives the complete cache identity from item 262. The stable
partition merely prevents an unrelated body edit from changing other modules' IR. Signature,
coordinator, target, optimization, debug or worker-count changes remain honest invalidations because
they can alter ABI, shared declarations or the bucket mapping.

## Archivos / Files

- `include/nativesharp/LLVM/ProjectOopCodePartition.h`: declares balanced and stable-incremental
  partition policies.
- `src/LLVM/ProjectOopCodePartition.cpp`: stable physical-name hashing, bucket assignment and
  non-renumbering removal of empty buckets.
- `src/LLVM/ProjectOopObjectGenerator.cpp`: selects stable partitioning only for an enabled verified
  artifact cache.
- `tests/unit/ProjectOopLoweringTests.cpp`: two source revisions, stable assignment proof and exact
  cache hit/miss assertions.
- Compiler contract, conformance map and roadmap: normative behavior and delivery traceability.

## Decisiones / Decisions

1. Opt-in behavior: builds without artifact cache retain the previous balanced distribution.
2. Physical identity is the only incremental bucket authority; body cost cannot cause cascaded
   invalidation.
3. Fixed bucket identities are preserved even when intermediate buckets are empty.
4. The resolved code-unit count remains policy input; changing parallelism may intentionally remap
   functions rather than pretending incompatible partitions are reusable.
5. Existing SHA-256 sidecars and COFF validation remain the authority for accepting every hit.

## Aceptación / Acceptance

- OOP lowering passes 91 checks.
- Two projects with identical API and one changed body preserve every function-to-bucket assignment.
- La primera generación produce solamente misses.
- La segunda generación, después de cambiar un solo cuerpo, produce exactamente un miss y reutiliza
  el coordinador y las unidades pares.
- Una tercera generación sin cambios produce solamente hits.
- La reconstrucción limpia de `nsc` pasa; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Hace precisa la invalidación incremental / Make incremental invalidation precise`

**Descripción / Description**

`Add an explicit stable-incremental code partition policy keyed by physical function identity,
preserve ordinary balanced builds, keep surviving bucket identities stable, and prove that a
body-only edit causes exactly one verified object-cache miss while coordinator and peer units remain
reusable.`
