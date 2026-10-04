# NS3-T05-F03 — Associated types and constants

## Estado

`TERMINADA`. Tipos asociados, constantes asociadas, defaults, proyecciones encadenadas,
constraints de igualdad, diagnósticos y transporte NSMETA están conectados al frontend productivo.

## Qué se hizo

- Se añadieron nodos AST propios para `type Item` y `const T Name` dentro de traits e impls.
- El parser acepta requisitos y defaults en traits, exige definiciones en impls cuando corresponde
  y conserva `where T.Item == Concrete` como constraint de igualdad estructurado.
- El modelo genérico registra definiciones por dueño concreto y normaliza `T.Item`, incluidas
  cadenas de proyecciones y defaults de traits genéricos sustituidos con sus argumentos.
- `T.Name` se sustituye por el valor constante asociado durante la especialización.
- Se diagnostican requisitos ausentes, duplicados, tipos incompatibles de constantes,
  inicializadores no constantes, defaults competidores, proyecciones inexistentes y ciclos.
- Los bloques `impl` concretos participan en la materialización de traits genéricos; los impls
  abiertos continúan diferidos por el flujo normal de plantillas.
- NSMETA subió de 1.28 a 1.29 y publica requisitos/defaults de traits y definiciones concretas.
  Serialización, lectura, merge, diff de API, reporte ABI y rechazo downlevel conocen el contrato.

## Archivos modificados

- `include/nativesharp/AST/AstNode.h`, `include/nativesharp/AST/AstNodeKind.h`,
  `src/AST/AstNodeKind.cpp`: representación de asociados y proyección de igualdad.
- `include/nativesharp/Parser/Parser.h`, `src/Parser/Parser.cpp`: gramática de trait, impl y
  constraints.
- `include/nativesharp/Frontend/ProjectGenericModel.h`,
  `src/Frontend/ProjectGenericModel.cpp`, `src/Frontend/ProjectGenericConstraintModel.cpp`:
  registro, sustitución, normalización y diagnósticos.
- `include/nativesharp/Interop/LibraryMetadata.h`, `src/Interop/LibraryMetadata.cpp`,
  `src/Interop/LibraryMetadataMerger.cpp`, `src/Interop/LibraryApiDiff.cpp` y
  `src/Interop/LibraryAbiReport.cpp`: contrato NSMETA 1.29.
- `src/Driver/Driver.cpp`, `src/Driver/BreakingChangeLedger.cpp` y
  `src/Driver/ReleaseCandidateFreeze.cpp`: emisión precompilada y versión contractual.
- Pruebas de ledger, freeze, traits, metadata y migración: expectativas de versión 1.29.
- `CMakeLists.txt`: target dirigido de F03.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T05AssociatedItemTests.cpp`: 16 checks productivos O0/NSLink que cubren
  ejecución, negativos, genéricos, módulos separados y downgrade de metadata.
- `tests/fixtures/nsmeta/historical-1.28.nsmeta`: fixture histórico generado por el mecanismo
  oficial; demuestra que 1.28 sigue siendo legible sin fingir soporte de escritura para F03.
- `docs/3.0/language/associated-items.md`: contrato de usuario bilingüe.
- Este documento: trazabilidad del commit atómico.

## Mejora obtenida

Una API genérica puede expresar la relación entre el tipo implementador y sus tipos/constantes sin
parámetros repetidos, boxing ni tablas runtime. La normalización es determinista, los conflictos no
dependen del orden de módulos y el consumidor de una biblioteca recibe el mismo contrato mediante
NSMETA. El tipo concreto resultante llega al backend y al contrato de depuración sin símbolos
runtime inventados.

## Validación dirigida

- `nsc_ns3_t05_associated_item_tests`: 16/16.
- Casos positivos: iterador, constante, defaults, default genérico, cadena, igualdad y consumidor
  NSMETA; los ejecutables O0 terminan con 42.
- Casos negativos: requisito ausente, duplicado, tipo de constante, expresión no constante, ciclo,
  ambigüedad y proyección no normalizable.
- NSMETA 1.29: productor/consumidor correcto y downgrade a 1.28 rechazado.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
