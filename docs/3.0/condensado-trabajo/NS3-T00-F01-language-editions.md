# NS3-T00-F01 — Ediciones de lenguaje

## Resultado

La edición es una propiedad explícita de la unidad y del grafo de compilación. NSC acepta
únicamente `2` o `3`, conserva la selección en frontend, dumps, NSMETA y claves incrementales, y
rechaza con `NS3001` referencias de proyecto o metadata precompilada cuya edición no coincide.
La edición no altera por sí sola el ABI ni la versión de biblioteca.

## Implementación comprobada

- `LanguageEdition` define el conjunto cerrado, su parser y su huella estable.
- `ProjectLoader`, `ProjectGraphLoader`, `ProjectDumper` y `FrontendCompilation` transportan la
  edición sin inferir `latest`.
- `LibraryMetadataCodec` publica y rehidrata la edición en NSMETA.
- el frontend emite `NS3002` al usar superficie 3.0 bajo edición 2.
- la identidad incremental cambia al cambiar la edición.

## Prueba dirigida

- objetivo: `nsc_ns3_t00_constitution_tests`
- resultado: `PASS: 31 checks`
- cobertura específica: selección 2/3, rechazo de edición desconocida, grafo mixto, NSMETA mixto,
  preservación en frontend/dump, huella incremental y rechazo de sintaxis 3.0 bajo edición 2.
- optimización del lenguaje: `O0` donde se invoca el pipeline productivo.
- baseline: no ejecutado.

## Archivos relevantes

- `include/nativesharp/Language/LanguageEdition.h`
- `src/Language/LanguageEdition.cpp`
- `src/Compilation/ProjectLoader.cpp`
- `src/Compilation/ProjectGraphLoader.cpp`
- `src/Frontend/FrontendCompilation.cpp`
- `src/Interop/LibraryMetadata.cpp`
- `tests/unit/NativeSharp3T00ConstitutionTests.cpp`
- `tests/conformance/t00/edition3-project.json`

## Mejora

El compilador puede evolucionar la semántica de 3.0 sin reinterpretar silenciosamente proyectos
2.x ni contaminar cachés o metadata con resultados de otra edición.
