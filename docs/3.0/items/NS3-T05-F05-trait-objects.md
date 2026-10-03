# NS3-T05-F05 — Explicit trait objects

## Estado

`TERMINADA`. `dyn Trait` tiene object-safety verificable, representación Windows x64 declarada,
ownership explícito y transporte completo mediante NSMETA 1.31.

## Qué se hizo

- `ProjectInterfaceModel` construye un contrato de objeto por trait: tamaño/alineación 8/8,
  lookup por descriptor/interface-map, null cero y conversión sin asignación.
- Todo uso `dyn` valida que el destino sea object-safe. Métodos genéricos y asociados sin default
  impiden el borrado dinámico, pero no el uso estático del trait.
- `dyn Trait*` y `dyn Trait[]` se rechazan porque `dyn` ya es un carrier de referencia.
- Las conversiones de clases y upcasts de traits reutilizan el objeto existente. Los structs no se
  convierten mediante boxing silencioso.
- Ownership se conserva: una vista `borrowed` no destruye; una referencia `owned` transfiere el
  objeto y el descriptor ejecuta su destructor concreto una sola vez.
- NSMETA subió de 1.30 a 1.31. Publica representación/object-safety y conserva `dyn` en retornos,
  parámetros, fields/events/accessors, funciones y estado de namespace, y delegates.
- El materializador reconstruye la sintaxis `dyn` para consumidores precompilados. El reporte ABI
  muestra representación, número de palabras y aristas dinámicas.
- El diff API clasifica agregar/quitar `dyn` como cambio fuente; la ABI permanece compatible
  cuando el carrier físico sigue siendo la referencia nominal de una palabra.

## Archivos modificados

- `include/nativesharp/Frontend/ProjectInterfaceModel.h` y
  `src/Frontend/ProjectInterfaceModel.cpp`: contrato, object-safety y validación de usos.
- `include/nativesharp/Language/TraitSemantics.h` y
  `src/Language/TraitSemantics.cpp`: layout declarado Windows x64.
- `include/nativesharp/Interop/LibraryMetadata.h`, `src/Interop/LibraryMetadata.cpp`,
  `src/Interop/LibraryMetadataMerger.cpp`, `src/Interop/LibraryApiDiff.cpp` y
  `src/Interop/LibraryAbiReport.cpp`: formato 1.31, round-trip, validación, merge y reporting.
- `src/Driver/Driver.cpp`: reconstrucción de todas las formas públicas `dyn`.
- `src/Driver/BreakingChangeLedger.cpp` y `src/Driver/ReleaseCandidateFreeze.cpp`: contratos de
  producto alineados con NSMETA 1.31.
- `tests/unit/NativeSharp3T05TraitTests.cpp`,
  `tests/unit/NativeSharp3T05OpaqueResultTests.cpp` y
  `tests/unit/Item405FinalCompatibilityCertificationTests.cpp`: expectativas alineadas al nuevo
  contrato y versión.
- `CMakeLists.txt`: target de la batería dirigida F05.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T05TraitObjectTests.cpp`: 12 checks dirigidos para dispatch, ownership,
  object-safety, no-boxing, MIR, DLL, NSMETA, reporte y compatibilidad.
- `tests/fixtures/nsmeta/historical-1.30.nsmeta`: conserva bytes del formato anterior mediante el
  generador oficial, sin reescribir fixtures históricos.
- `docs/3.0/language/trait-objects.md`: contrato bilingüe consumible por usuarios y herramientas.
- Este documento: trazabilidad del commit atómico.

## Mejora obtenida

El borrado dinámico ya no depende de una interpretación implícita del tipo interface. El lenguaje
define qué traits pueden convertirse, cómo se despachan, qué ocurre con null y ownership, y qué
representación cruza una DLL. La conversión es barata y predecible: una palabra ya existente, cero
allocations y cero boxing oculto.

## Validación dirigida

- `nsc_ns3_t05_trait_object_tests`: 12/12.
- `nsc_ns3_t05_trait_tests`: 9/9.
- `nsc_library_metadata_merger_tests`: 17/17.
- `nsc_stage48_2_library_metadata_tests`: 351/351; fixture 1.30 añadido sin sobrescribir otros.
- Contratos de release dirigidos: Item 402 15/15, Item 404 10/10, Item 405 16/16 y Stage 69
  15/15.
- `nsc` O0 compilado; contrato de runtime host estático: passed.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
