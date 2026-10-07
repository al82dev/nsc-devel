# Tanda universal 07: contratos acumulativos y NSMETA neutral

## Estado

`IMPLEMENTADA; VALIDACIÓN PENDIENTE DESPUÉS DEL COMMIT`

## SCOPE LOCK

- Objetivo autorizado: eliminar las decisiones funcionales restantes basadas en Language 2/3 de los contratos
  compartidos del compilador, NSMETA, herramientas de conformidad y pruebas históricas consumidoras.
- Naturaleza: corrección funcional de compatibilidad acumulativa.
- Comportamiento que permanece: sintaxis reconocida, semántica acumulativa, resolución de módulos, target
  Windows x64, ABI, layout, ownership, fingerprints de contratos reales, lectura de metadata histórica,
  diagnósticos no relacionados y nombres históricos de pruebas.
- Revisión conocida: `ade7af37c`; la batería de `fd25ae369` ejecutó 684 CTests y dejó 47 fallos, mientras las
  pruebas universales de proyecto ya pasaban.
- Rango evaluado: `ade7af37c..HEAD` para esta tanda.
- Archivos permitidos: contratos Language/Driver/Interop, ProjectMetaprogramming, runner de conformidad,
  fixtures directamente consumidores, CMake para retirar la unidad obsoleta y este documento.
- Archivos protegidos: parser, runtime, bibliotecas, bindings, NSLink, NSDBG, NSASM, VS Code, instalaciones,
  toda unidad `C:` y expectativas ajenas a la eliminación de edición.
- Pruebas protegidas: se conservan nombres y propósito; sólo se sustituyen aserciones que exigían una edición
  por contratos acumulativos, ABI, target, estructura, release o deprecación reales.
- Acciones prohibidas: deploy, escritura en `C:`, push, baseline, O1/O2/O3, renombrar pruebas históricas,
  eliminar el byte histórico del wire format NSMETA 1.23 o adaptar fallos no relacionados.
- Criterio de terminación: no existe `LanguageEdition` como tipo ni campo; ningún contrato usa una edición para
  aceptar, rechazar, invalidar, promover, fingerprintar o fusionar; NSMETA lee y descarta su byte histórico y
  lo emite con un valor legado constante; la conformidad no exige ni exporta edición.

## Inventario causal

La búsqueda inicial encontró 39 archivos con `LanguageEdition`, 167 con `<Language>`, 14 con `NS3001` y 18 con
`NS3002`. La mayoría de documentos y nombres son historia, no productores. Los productores funcionales son:

1. `LanguageEdition.h/.cpp`: tipo y parser de edición.
2. `CompatibilitySemantics`: edición en módulos, unidades y rebuild incremental.
3. `ExpressionErgonomics`: parámetro de edición ya convertido en no-op.
4. `DeprecationPolicy`: estado `Active` exclusivo de la edición antigua.
5. `ModuleSemantics`: `sinceEdition`/`removedEdition` y estados de disponibilidad derivados.
6. `ReleaseSemantics`: requisito de edición en promoción oficial.
7. `LibraryMetadata`: campo, validación, fingerprint y merge de la edición.
8. `ProjectMetaprogrammingModel`: almacenamiento de disponibilidad por edición.
9. `ns3_conformance.py`: manifest y variable de entorno de edición.
10. `DiagnosticId`: códigos NS3001/NS3002 reservados al gate retirado.

## Decisión de compatibilidad NSMETA

NSMETA 1.23 añadió físicamente un byte después del target triple. Retirarlo desplazaría todos los campos
posteriores y rompería readers históricos. La solución conserva el wire layout sin conservar el concepto:

- el modelo `LibraryMetadata` no expone edición;
- el writer emite siempre el valor legado `3` para minors 23 o superiores;
- el reader consume el byte en una variable local y lo descarta;
- no se valida si contiene 2, 3 u otro valor;
- API diff, ABI projection, merge y fingerprints no lo observan.

Así se preserva compatibilidad binaria hacia atrás sin permitir que el byte gobierne comportamiento actual.

## Validación de la tanda

Por instrucción expresa, no se ejecutó build ni prueba antes del commit. Las comprobaciones estáticas previas
fueron:

- `git diff --check`: sin errores;
- búsqueda exacta en `CMakeLists.txt`, `include`, `src`, `tools`, `tests`, `cmake` y `selfhost`:
  - cero productores o consumidores de `LanguageEdition`;
  - cero `EditionSemantics` y `EditionCompilationUnit`;
  - cero campos `languageEdition`, `sinceEdition` y `removedEdition`;
  - cero diagnósticos `NS3001` y `NS3002`;
  - cero usos de `NATIVESHARP_LANGUAGE_EDITION`;
- las únicas coincidencias restantes de `LanguageEdition` son nombres históricos de tres archivos/CTest. Se
  conservan por orden explícita de no renombrar pruebas; no participan en ningún contrato.
- ninguna línea añadida supera 120 caracteres;
- no se añadió recursión.

Después del commit deben ejecutarse, en este orden:

1. comprobación sintáctica del harness Python;
2. targets unitarios de contratos, metadata, migración y promoción;
3. conformidad T00;
4. la batería O0 completa con NSLink, sin baseline, cuando corresponda al cierre de la tanda.

## Implementación

### Contrato acumulativo

Se eliminó `LanguageEdition.h/.cpp` y su entrada de CMake. `BinaryModuleContract` conserva únicamente las
propiedades que realmente determinan compatibilidad: versión de formato, target, fingerprint ABI y validez
estructural. La compatibilidad directa ya no puede cambiar por una etiqueta de fuente.

`EditionCompilationUnit` se sustituyó por `CompilationUnitContract`. La validación exige identidades de fuente,
fingerprints, módulos únicos e imports resolubles. La invalidación incremental observa cambios de contenido y
propaga el rebuild por el grafo de imports; no existe una segunda semántica seleccionable por edición.

### Superficie, disponibilidad y deprecación

- las expresiones registradas por `ExpressionErgonomics` están disponibles sin parámetro de edición;
- `Availability` conserva target y deprecación, pero no ventanas `sinceEdition`/`removedEdition`;
- los atributos históricos `since` y `removed` siguen siendo parseables, pero no crean gates semánticos;
- la deprecación depende de release, supresión y política warnings-as-errors;
- la promoción oficial exige evidencia y hashes reales, no una constante de edición.

### NSMETA

El modelo perdió `LibraryMetadata::languageEdition`. La proyección ABI, el API diff y el merge tampoco observan
ese dato. Para no romper el layout de NSMETA 1.23 o superior:

- el encoder escribe `kLegacySourceRevisionByte = 3` en la posición reservada;
- el decoder consume el byte como `legacySourceRevision` y lo descarta;
- el valor no se valida, no llega al modelo y no participa en ningún fingerprint.

La complejidad temporal y espacial de encode/decode no cambia: se mantiene una operación O(1) sobre un byte ya
presente en el wire format.

### Diagnósticos y conformidad

Se retiraron los IDs `NS3001` y `NS3002` porque sólo representaban rechazo por edición. Los tests de diagnósticos
de usuario usan códigos neutrales que conservan el propósito original: validar códigos reservados y agrupación
causal, no una política de versión.

El harness `ns3_conformance.py` ya no exige `edition`, no valida valores `2/3` y no exporta una variable de
entorno capaz de seleccionar semántica. Los manifests y el proyecto T00 conservan sus nombres históricos, pero
ya no contienen el campo o la propiedad funcional.

## Inventario de archivos

### Build, diagnósticos y contratos

- `CMakeLists.txt`: retira la unidad obsoleta.
- `include/nativesharp/Diagnostics/DiagnosticId.h`: retira los dos diagnósticos de gate.
- `include/nativesharp/Language/LanguageEdition.h`: eliminado.
- `src/Language/LanguageEdition.cpp`: eliminado.
- `include/nativesharp/Language/CompatibilitySemantics.h` y
  `src/Language/CompatibilitySemantics.cpp`: contratos acumulativos.
- `include/nativesharp/Language/ExpressionErgonomics.h` y
  `src/Language/ExpressionErgonomics.cpp`: disponibilidad universal.
- `include/nativesharp/Language/ModuleSemantics.h` y `src/Language/ModuleSemantics.cpp`: disponibilidad por
  target y deprecación.
- `include/nativesharp/Language/ReleaseSemantics.h` y `src/Language/ReleaseSemantics.cpp`: promoción por
  evidencia.

### Driver y frontend

- `include/nativesharp/Driver/DeprecationPolicy.h`, `src/Driver/DeprecationPolicy.cpp`,
  `src/Driver/MigrationAnalyzer.cpp` y `src/Driver/ExecutableMigrationGuide.cpp`: política y texto neutrales.
- `include/nativesharp/Frontend/ProjectMetaprogrammingModel.h` y
  `src/Frontend/ProjectMetaprogrammingModel.cpp`: retiran gates por `since`/`removed`.
- `include/nativesharp/Frontend/ProjectConstructionModel.h`,
  `include/nativesharp/Frontend/ProjectMemberModel.h`,
  `include/nativesharp/Frontend/ProjectMethodBodyModel.h` y
  `src/Frontend/ProjectOpaqueResultModel.cpp`: comentarios alineados con la semántica acumulativa.
- `src/Language/Language3Surface.cpp`: descripción del contrato de move sin edición.

### Metadata

- `include/nativesharp/Interop/LibraryMetadata.h` y `src/Interop/LibraryMetadata.cpp`: modelo neutral y byte
  legado constante.
- `src/Interop/LibraryApiDiff.cpp`: fingerprint de superficie sin edición.
- `src/Interop/LibraryMetadataMerger.cpp`: igualdad de headers sin edición.

### Pruebas y herramientas

- `tools/ns3_conformance.py` y `tests/conformance/t00/{Edition3.nsproj,edition3-project.json,edition3-mir.json}`:
  conformidad sin selección de edición.
- `tests/unit/NativeSharp3T01ExpressionTests.cpp`, `NativeSharp3T03TaggedVariantTests.cpp`,
  `NativeSharp3T11ComptimeDiagnosticsTests.cpp`, `NativeSharp3T11MetaprogrammingTests.cpp`,
  `NativeSharp3T12ApiFingerprintTests.cpp`, `NativeSharp3T12ModuleTests.cpp`,
  `NativeSharp3T16CompatibilityTests.cpp`, `NativeSharp3T16Edition2CompatTests.cpp`,
  `NativeSharp3T16MigrationTests.cpp`, `NativeSharp3T17OfficialPromotionTests.cpp` y
  `Stage48_2LibraryMetadataTests.cpp`: expectativas sobre contratos reales.
- `tests/unit/test_nscbuilder_pipeline.py`: reporte condensado con código neutral.

## Austeridad, riesgos y trabajo posterior

No se añadió capa, wrapper, caché, allocation, excepción, concurrencia ni estado mutable. La solución elimina
ramas y datos que ya no representan una decisión válida. El byte legado es el único residuo deliberado y existe
exclusivamente para conservar offsets físicos de NSMETA.

Esta tanda no afirma haber retirado todavía cada `<Language>` de los fixtures generales. Esos proyectos forman
otra tanda mecánica y verificable: deben dejar de depender de la propiedad sin renombrar las pruebas ni cambiar
su conducta semántica. Tampoco corrige fallos de ownership, borrow, generics, foreach, excepciones o ABI; se
atenderán por causa raíz de lenguaje antes de los cierres self-host e infraestructura.
