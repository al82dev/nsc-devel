# NS3-T03-F01 — Tagged variants

## Estado

`EN CURSO`. La frontera lifetime-trivial, layout y metadatos está endurecida. El cierre move-only,
drop exactamente una vez y niche explícitamente autorizado depende de T06 y no se declara hecho.

## Qué se hizo

- Se preservó la implementación productiva `variant` como tag mínimo más unión de payload in-place.
- NSMETA avanzó de 1.26 a 1.27 y ahora conserva identidad nominal, orden de casos y tipo de payload.
- El fingerprint ABI incluye el catálogo ordenado; cambiar `CaseTag()` ya no pasa como cambio
  cosmético.
- El materializador de `LibraryReference` reconstruye la sintaxis nominal de variants concretos.
- El merge de metadatos, diff de API, reporte ABI y verificador de referencias entienden el
  catálogo 1.27.
- La compatibilidad de lectura 1.26 permanece probada para modelos sin variants.
- Se documentó que no hay niche implícito y que recursión por valor/ownership activo se rechazan.

## Archivos modificados

- `include/nativesharp/Interop/LibraryMetadata.h`: formato 1.27, flag y catálogo variant.
- `src/Interop/LibraryMetadata.cpp`: producción, codec, fingerprint y validación.
- `src/Interop/LibraryMetadataMerger.cpp`: igualdad estructural del catálogo.
- `src/Interop/LibraryApiDiff.cpp`: casos y orden dentro de contratos source/ABI.
- `src/Interop/LibraryAbiReport.cpp`: exposición legible de casos y payloads.
- `src/Driver/Driver.cpp`: rehidratación nominal desde `LibraryReference`.
- `src/Driver/BreakingChangeLedger.cpp`, `src/Driver/ReleaseCandidateFreeze.cpp`: contrato 1.27.
- `tests/unit/Item402BreakingChangeLedgerTests.cpp`,
  `tests/unit/Item405FinalCompatibilityCertificationTests.cpp` y
  `tests/unit/Stage69MigrationBaselineTests.cpp`: expectativas de contrato actualizadas.
- `CMakeLists.txt`: registro de la prueba exclusiva del ítem.
- Documentos T03 existentes: corrección de evidencia histórica y frontera real.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T03TaggedVariantTests.cpp`: comprobaciones productivas aisladas sobre
  ejecución O0, layout Win64, genéricos locales, recursión, diagnósticos, NSMETA 1.27, diff ABI y
  consumo precompilado concreto.
- `docs/3.0/language/tagged-variants.md`: contrato bilingüe de usuario y ABI.
- Este documento: evidencia por commit y deuda explícita conforme a la metodología del proyecto.

## Mejora obtenida

Antes, mover o reordenar casos podía conservar un struct físicamente válido mientras herramientas
y consumidores perdían la identidad nominal o deducían mal sus tags. Ahora el orden observable se
transporta, autentica, compara, reporta y rehidrata. Los caminos todavía inseguros fallan de forma
explícita en vez de prometer drops o niches inexistentes.

## Validación dirigida

- `nsc_ns3_t03_sum_type_tests`: 9/9.
- `nsc_ns3_t03_tagged_variant_tests`: 20/20.
- `nsc_ns3_t02_native_record_tests`: 12/12, sin regresión de NSMETA 1.26.
- `nsc_library_metadata_merger_tests`: 17/17.
- ledger/freeze/compatibilidad/migración: 56/56.
- fixture `c59_ns3_variants`: compila en O0 con NSLink local y termina en 42.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
