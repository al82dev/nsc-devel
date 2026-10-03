# NS3-T05-F04 — Opaque result types

## Estado

`TERMINADA`. `some Trait` produce una identidad nominal única por declaración, conserva una sola
representación concreta in-place y cruza módulos mediante NSMETA 1.30 sin boxing ni nombre
escribible por el consumidor.

## Qué se hizo

- El parser reconoce `some Trait` únicamente en retornos directos de funciones y métodos de
  edition 3. Campos, parámetros, locales, argumentos de tipo, punteros, arrays, tuplas y la mezcla
  con `dyn` se rechazan explícitamente.
- `ProjectOpaqueResultModel` infiere todos los retornos no recursivos, exige un único tipo concreto
  y valida conformidad nominal con el trait. Tener métodos con el mismo nombre no cuenta como
  implementación.
- Cada declaración recibe un struct nominal `__nsc_some_<hash>` estable. Contiene exactamente un
  campo de representación, una fábrica interna y forwarders para métodos requeridos, incluidos los
  heredados. Los retornos se envuelven sin reservar heap; la recursión directa reutiliza la misma
  identidad.
- La identidad estable incluye namespace, dueño, firma y especialización completa del trait. Dos
  declaraciones no comparten accidentalmente tipo opaco aunque oculten la misma representación.
- Las definiciones de tipos y constantes asociados se transportan desde el concreto al wrapper.
  Los traits de structs participan en constraints nominales sin crear tablas de despacho dinámico
  ni alterar layout.
- La inferencia genérica conserva retornos ordinarios no ambiguos antes del modelo de cuerpos. Un
  consumidor puede pasar el resultado anónimo como `T` sin conocer el nombre generado.
- NSMETA subió de 1.29 a 1.30. El bit `kMetadataTypeOpaqueResult` congela identidad, layout de un
  campo, traits, asociados y superficie API/ABI. Downgrade, metadata corrupta y borrado del flag se
  rechazan o clasifican como cambio de compatibilidad.
- El materializador público reconstruye structs opacos con su trait y asociados. Se preserva la
  representación concreta internal necesaria para compilar la implementación sin exponerla como
  nombre público de retorno.

## Archivos modificados

- `include/nativesharp/AST/TypeSyntax.h`, `src/AST/TypeSyntax.cpp`: marcador y trait de `some`.
- `src/Parser/Parser.cpp`: gramática contextual, posiciones válidas y traits/asociados de structs.
- `include/nativesharp/Frontend/ProjectGenericModel.h`,
  `src/Frontend/ProjectGenericModel.cpp`: integración de fase, asociados anidados e inferencia de
  retornos anónimos.
- `src/Frontend/ProjectGenericConstraintModel.cpp`,
  `src/Frontend/ProjectInterfaceModel.cpp`: conformidad nominal de structs sin tabla dinámica.
- `src/Frontend/ProjectNameLookup.cpp`: preservación de `some` al expandir aliases.
- `src/Driver/Driver.cpp`: reconstrucción precompilada y cierre de tipos internal de contrato.
- `include/nativesharp/Interop/LibraryMetadata.h`, `src/Interop/LibraryMetadata.cpp` y
  `src/Interop/LibraryAbiReport.cpp`: NSMETA 1.30, validación y reporte `kind=opaque-result`.
- `src/Driver/BreakingChangeLedger.cpp`, `src/Driver/ReleaseCandidateFreeze.cpp` y pruebas de
  compatibilidad/migración: versión contractual 1.30.
- `CMakeLists.txt`: fase productiva y target dirigido de F04.

## Archivos nuevos y por qué

- `include/nativesharp/Frontend/ProjectOpaqueResultModel.h` y
  `src/Frontend/ProjectOpaqueResultModel.cpp`: una fase única posee inferencia, identidad y lowering
  del resultado opaco; evita repartir decisiones incompatibles entre parser, genéricos y backend.
- `tests/unit/NativeSharp3T05OpaqueResultTests.cpp`: 14 checks dirigidos O0/NSLink para ejecución,
  diagnósticos, herencia, asociados, módulos, inferencia genérica y NSMETA.
- `tests/fixtures/nsmeta/historical-1.29.nsmeta`: bytes históricos generados por el mecanismo
  oficial sin sobrescribir fixtures anteriores.
- `docs/3.0/language/opaque-result-types.md`: contrato bilingüe para usuarios y herramientas.
- Este documento: trazabilidad del commit atómico.

## Mejora obtenida

Una API puede ocultar su clase/struct concreto y devolver sólo un contrato estático, manteniendo
tamaño y alineación conocidos por el consumidor. El valor permanece in-place y usa llamadas
directas; no aparece heap, objeto runtime, RTTI ni dispatch dinámico oculto. El compilador conserva
la identidad a través de `.lib/.nsmeta`, genéricos y asociados, por lo que una recompilación no
convierte el tipo opaco en un struct estructuralmente equivalente pero semánticamente distinto.

## Validación dirigida

- `nsc_ns3_t05_opaque_result_tests`: 14/14.
- Positivos: branches, recursión, trait heredado, productor/consumidor, asociado, inferencia
  genérica y ejecución O0 con salida 42.
- Negativos: representaciones distintas, destino no trait, falsa conformidad nominal y posición
  ilegal de `some`.
- NSMETA 1.30: layout de un campo, flag, trait, asociado, reporte ABI, diff de compatibilidad,
  corrupción rechazada y downgrade a 1.29 rechazado.
- Regresores dirigidos: F01 14/14, F03 16/16, contratos de release y Stage 48.2 346/346.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
