# NS3-T03-F01 — Tagged variants

## Estado

`TERMINADA`. El contrato de layout, ownership, cambio de caso activo, drop y metadatos está
cerrado para NativeSharp 3.0.

## Qué se hizo

- Se preservó la implementación productiva `variant` como tag mínimo más unión de payload in-place.
- NSMETA avanzó de 1.26 a 1.27 y ahora conserva identidad nominal, orden de casos y tipo de payload.
- El fingerprint ABI incluye el catálogo ordenado; cambiar `CaseTag()` ya no pasa como cambio
  cosmético.
- El materializador de `LibraryReference` reconstruye la sintaxis nominal de variants concretos.
- El merge de metadatos, diff de API, reporte ABI y verificador de referencias entienden el
  catálogo 1.27.
- La compatibilidad de lectura 1.26 permanece probada para modelos sin variants.
- Se fijó que NativeSharp 3.0 siempre conserva el tag explícito: no existe niche implícito ni una
  segunda representación autorizada que pueda romper el ABI.
- Los payloads `owned` y move-only son productivos. Reemplazar el valor evalúa primero el RHS,
  destruye únicamente el payload activo desplazado y después instala el nuevo carrier.
- La salida de alcance destruye exactamente una vez el payload que permanece activo.
- `borrowed variant` y un payload `borrowed` directo no son deuda futura: quedan fuera del contrato
  3.0. Un préstamo almacenado se expresa mediante un `borrowed struct` con lifetime nombrado.
- `readonly` continúa reservado a structs no-union; no puede redefinir la construcción interna de
  un variant.

## Archivos modificados

- `include/nativesharp/Interop/LibraryMetadata.h`: formato 1.27, flag y catálogo variant.
- `src/Interop/LibraryMetadata.cpp`: producción, codec, fingerprint y validación.
- `src/Interop/LibraryMetadataMerger.cpp`: igualdad estructural del catálogo.
- `src/Interop/LibraryApiDiff.cpp`: casos y orden dentro de contratos source/ABI.
- `src/Interop/LibraryAbiReport.cpp`: exposición legible de casos y payloads.
- `src/Driver/Driver.cpp`: rehidratación nominal desde `LibraryReference`.
- `src/Driver/BreakingChangeLedger.cpp`, `src/Driver/ReleaseCandidateFreeze.cpp`: contrato 1.27.
- `src/Frontend/ProjectOopHirModel.cpp`: marca todo reemplazo de un valor con drop, incluso cuando
  el RHS es un temporal recién construido y no un `move(local)` explícito.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: destruye el valor desplazado de parámetros, campos y
  elementos antes del store; los locales reutilizan su cleanup region y su bandera de inicializado.
- `src/Parser/Parser.cpp`: convierte las antiguas referencias a una T06 futura en diagnósticos
  definitivos del contrato 3.0.
- `tests/unit/Item402BreakingChangeLedgerTests.cpp`,
  `tests/unit/Item405FinalCompatibilityCertificationTests.cpp` y
  `tests/unit/Stage69MigrationBaselineTests.cpp`: expectativas de contrato actualizadas.
- `CMakeLists.txt`: registro de la prueba exclusiva del ítem.
- Documentos T03 existentes: corrección de evidencia histórica y frontera real.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T03TaggedVariantTests.cpp`: comprobaciones productivas aisladas sobre
  ejecución O0, layout Win64, genéricos locales, recursión, payload move-only, cambio de caso con
  drop exacto, diagnósticos, NSMETA 1.27, diff ABI y consumo precompilado concreto.
- `docs/3.0/language/tagged-variants.md`: contrato bilingüe de usuario y ABI.
- Este documento: evidencia por commit y deuda explícita conforme a la metodología del proyecto.

## Mejora obtenida

Antes, mover o reordenar casos podía conservar un struct físicamente válido mientras herramientas
y consumidores perdían la identidad nominal o deducían mal sus tags. Ahora el orden observable se
transporta, autentica, compara, reporta y rehidrata. Los caminos todavía inseguros fallan de forma
explícita en vez de prometer drops o niches inexistentes.

El hueco de reemplazo move-only también quedó eliminado: antes, `owner = Owner.Empty()` podía
sobrescribir el carrier sin ejecutar el destructor del payload activo. Ahora el reemplazo usa la
misma obligación de drop que la salida normal de alcance, después de materializar por completo el
RHS para no invalidar expresiones que leen el valor anterior.

## Validación dirigida

- `nsc_ns3_t03_tagged_variant_tests`: 22/22 en O0 con NSLink; incluye reemplazo de caso activo y
  drop final exactamente una vez.
- Target `nsc`: construcción exacta completada después de la prueba dirigida.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
