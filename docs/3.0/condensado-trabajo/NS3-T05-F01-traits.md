# NS3-T05-F01 — Trait declarations

## Estado

`TERMINADA`. La declaración `trait` ya tiene requisitos, defaults, requisitos estáticos y contrato
precompilado versionado, sin introducir layout ni un runtime paralelo.

## Qué se hizo

- El parser distingue traits de interfaces ordinarias y permite métodos requeridos o default.
- Las propiedades de trait son requisitos `static`; las propiedades de instancia se rechazan.
- Los operadores de trait son requisitos `static` sin cuerpo y se validan contra el tipo concreto.
- El modelo de interfaces hereda requisitos y defaults, selecciona un default cuando no existe un
  miembro concreto y diagnostica defaults competidores.
- Los requisitos estáticos se verifican fuera de la tabla dinámica y no crean slots de instancia.
- El preescáner de tipos reconoce el contextual `trait`, incluso en source sintético importado.
- NSMETA sube a 1.28 y conserva el marcador trait y el orden normativo de métodos. Esto evita que
  un default precompilado use un slot reconstruido con otro orden.
- El reporte ABI muestra `kind=trait` y `trait-slot`; API diff trata un reordenamiento como cambio
  de compatibilidad.
- El dumper de interfaces identifica traits y sus requisitos estáticos.

## Archivos modificados

- `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp`: gramática de miembros trait.
- `include/nativesharp/Frontend/ProjectInterfaceModel.h`,
  `src/Frontend/ProjectInterfaceModel.cpp`, `src/Frontend/ProjectInterfaceModelDumper.cpp`:
  requisitos/defaults, conflictos, requisitos estáticos y dump.
- `src/Frontend/ProjectMemberModel.cpp`: operadores abstractos válidos en traits.
- `src/Frontend/ProjectCompilation.cpp`: siembra de nombres contextuales `trait`.
- `include/nativesharp/Interop/LibraryMetadata.h`, `src/Interop/LibraryMetadata.cpp`:
  NSMETA 1.28, marcador y catálogo ordenado de slots.
- `src/Interop/LibraryMetadataMerger.cpp`, `src/Interop/LibraryAbiReport.cpp`,
  `src/Interop/LibraryApiDiff.cpp`: merge, reporte y compatibilidad del nuevo contrato.
- `src/Driver/Driver.cpp`: rehidratación fiel de traits precompilados.
- Contratos de migración/freeze y sus pruebas: versión actual NSMETA 1.29; el marcador y los slots
  de traits siguen siendo la sección introducida por NSMETA 1.28.
- `CMakeLists.txt`: target dirigido de F01.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T05TraitDeclarationTests.cpp`: batería atómica positiva, negativa y
  cross-module de F01.
- `tests/fixtures/nsmeta/historical-1.26.nsmeta` y `historical-1.27.nsmeta`: snapshots inmutables
  que pasan a ser históricos al publicar NSMETA 1.28; se crearon con el modo de refresh del propio
  test sin sobrescribir goldens anteriores.
- `docs/3.0/language/trait-declarations.md`: contrato de usuario ES/EN.
- Este documento: evidencia y alcance del commit lógico.

## Mejora obtenida

Un trait puede publicar comportamiento reutilizable y obligaciones estáticas sin storage. El mismo
contrato funciona desde fuente o desde `.lib/.nsmeta`; un consumidor no depende del orden alfabético
de metadata y los defaults conservan exactamente los slots con los que fueron compilados.

## Validación dirigida

- `nsc_ns3_t05_trait_declaration_tests`: 14/14.
- Construcción local de `nsc`: correcta; contrato de runtime host estático: correcto.
- Regresiones de parser, Stage 36, metadata y vertical T05 ejecutadas individualmente al cierre.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
