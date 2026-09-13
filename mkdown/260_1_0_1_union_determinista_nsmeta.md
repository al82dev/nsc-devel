# Entrega 260 — Unión determinista de NSMETA / Deterministic NSMETA merging

## Estado / Status

- Tanda / Batch: 1 — acelerar el ciclo de desarrollo.
- Item: 260.
- Resultado / Result: los fragmentos NSMETA recolectados independientemente se unen por identidad
  canónica y ya no dependen del orden en que terminan los workers.
- Baseline: reservado para el cierre de la Tanda 1.

## Diseño e implementación / Design and implementation

`LibraryMetadataMerger` is the single publication coordinator for metadata fragments. It first
requires every fragment to carry the same format, target, producer, module and frozen ABI header.
It then combines every top-level record family and publishes a canonical model whose fingerprint
and payload can be serialized without observing worker completion order.

La identidad de cada familia es explícita: identidad nominal para tipos, tipo declarador más clase
de miembro y firma para miembros, clave canónica para plantillas, clave de especialización para
genéricos cerrados, tipo para tablas de despacho y nombre C para exports. Los registros idénticos
se colapsan; dos contenidos distintos bajo la misma identidad se rechazan antes de escribir el
archivo, con un diagnóstico estable que nombra la categoría y la clave en conflicto.

Set-like nested data is normalized too. Implemented interface keys, generic dependency keys,
dispatch interfaces and external nominal dependencies are sorted and deduplicated. ABI-ordered
data remains ordered: method parameters, delegate parameters, generic type arguments, vtable slots
and their callees are never rearranged. Physical fields are ordered by offset and then nominal
identity, retaining deterministic union-layout ties.

`LibraryMetadataCodec::writeModel` always sends its model through the merger. This is important even
for a one-fragment producer: the ordinary compiler path, compatibility goldens and future parallel
collectors now share exactly one canonicalization authority instead of relying on every caller to
remember an informal sequence of `sort` operations.

## Archivos / Files

- `include/nativesharp/Interop/LibraryMetadataMerger.h`: public coordinator contract for one or
  more independently collected metadata fragments.
- `src/Interop/LibraryMetadataMerger.cpp`: header validation, canonical keys, nested normalization,
  exact deduplication and deterministic conflict rejection.
- `src/Interop/LibraryMetadata.cpp`: routes all NSMETA model serialization through the merger.
- `tests/unit/LibraryMetadataMergerTests.cpp`: order inversion, byte identity, every record family,
  nested sets, duplicates, collisions, incompatible headers and empty input.
- `CMakeLists.txt`: compiler source integration and focused test target.
- Compiler contract, conformance map and 2.0 roadmap: normative behavior and traceability.

## Decisiones / Decisions

1. Coordinator merge, not concurrent mutation: workers can own fragments without locks around one
   shared metadata model.
2. Canonical identity before serialization: completion timing cannot affect bytes or fingerprints.
3. Exact duplicate collapse: repeated dependency discovery is harmless.
4. Conflicting duplicate rejection: a collision is never resolved by first-writer or last-writer
   timing.
5. Header equality is mandatory: fragments from different modules or ABI contracts cannot be
   accidentally mixed.
6. Ordered ABI vectors stay ordered: normalization never changes call or dispatch semantics.

## Aceptación / Acceptance

- Dos fragment sets con orden de llegada opuesto producen el mismo modelo canónico.
- Los dos modelos serializan archivos NSMETA byte por byte idénticos.
- Tipos, miembros, templates, specializations, dependencias externas, dispatch y exports sobreviven
  la unión.
- Los duplicados exactos se colapsan y los conflictos de identidad fallan de forma determinista.
- La prueba enfocada pasa 16 checks.
- La regresión NSMETA amplia conserva 226 de 227 checks; su único rojo es una colisión preexistente
  de campos enum estáticos en el linker y se reproduce igual retirando temporalmente el merger.
- La reconstrucción limpia de `nsc` pasa; no hay baseline intermedio.

## Commit de referencia / Reference commit

**Título / Title**

`Une fragmentos NSMETA de forma determinista / Merge NSMETA fragments deterministically`

**Descripción / Description**

`Add a canonical NSMETA merge coordinator that validates fragment headers, preserves ABI-ordered
vectors, normalizes set-like records, collapses exact duplicates, rejects identity conflicts, and
routes every metadata serialization through the same completion-order-independent authority.`
