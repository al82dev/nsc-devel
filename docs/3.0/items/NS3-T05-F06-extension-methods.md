# NS3-T05-F06 — Extensiones estáticas / Extension methods

## Estado

`TERMINADA`. NativeSharp 3.0 admite extensiones estáticas sobre receptores por valor, `ref` e `in`,
con lookup explícito, prioridad de miembros reales, lowering directo y transporte NSMETA 1.32.

## Qué se hizo

- El parser acepta `extension T`, `extension ref T` y `extension in T` en edition 3. Cada miembro
  se representa como una función de namespace con un primer parámetro receptor sintético; `this`
  se resuelve a ese parámetro.
- Name lookup publica los miembros del bloque sin convertirlos en miembros del tipo receptor.
- El conjunto visible reúne el namespace léxico y todos los namespaces importados explícitamente.
  No se inspecciona automáticamente el namespace del tipo receptor.
- Los miembros reales y facades del compilador ganan antes de intentar extensiones. Los candidatos
  restantes usan el overload resolver ordinario y reportan ambigüedad explícita.
- Los receptores por valor admiten conversiones implícitas normales. `ref`/`in` requieren una
  expresión addressable y conversión de identidad; `ref` además exige mutabilidad.
- HIR inserta el receptor como argumento cero y pide su dirección para `ref`/`in`. MIR y LLVM
  conservan una llamada directa a función de namespace, sin allocation ni dispatch runtime.
- El backend LLVM declara correctamente parámetros by-reference de funciones libres como
  punteros físicos.
- NSMETA subió de 1.31 a 1.32. El bit de extensión y el contrato del receptor sobreviven una
  `LibraryReference`; el materializador reconstruye el bloque y la validación física incluye
  funciones libres públicas.

## Archivos modificados

- `include/nativesharp/AST/AstNode.h`, `include/nativesharp/AST/AstNodeKind.h`,
  `src/AST/AstNode.cpp` y `src/AST/AstNodeKind.cpp`: contenedor AST y receptor sintético.
- `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`: gramática edition 3, clonación
  del tipo receptor y significado de `this`.
- `src/Frontend/ProjectNameLookup.cpp`,
  `include/nativesharp/Frontend/ProjectNamespaceFunctionModel.h` y
  `src/Frontend/ProjectNamespaceFunctionModel.cpp`: publicación, importación y catálogo de
  extensiones visible.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h` y
  `src/Frontend/ProjectMethodBodyModel.cpp`: prioridad, filtrado del receptor, overloads y
  diagnósticos.
- `src/Frontend/ProjectOopHirModel.cpp` y `src/LLVM/ProjectOopLlvmLowerer.cpp`: argumento receptor
  y ABI by-reference.
- `include/nativesharp/Interop/LibraryMetadata.h`, `src/Interop/LibraryMetadata.cpp` y
  `src/Driver/Driver.cpp`: NSMETA 1.32, materialización y validación de símbolos.
- `src/Language/Language3Surface.cpp`: la feature pasa de diferida a implementada.
- Contratos de producto y sus pruebas: ledger, freeze, compatibility matrix y migration baseline
  declaran NSMETA 1.32.
- `CMakeLists.txt` y pruebas existentes: registro de la batería, fixture CC1 y expectativas del
  formato vigente.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T05ExtensionMethodTests.cpp`: doce checks dirigidos de lookup,
  prioridad, generic receivers, no-ADL, ambigüedad, visibilidad completa, seguridad `ref`, NSMETA
  y productor/consumidor.
- `tests/cc1/c69_ns3_extension_methods/`: fixture permanente O0/NSLink que ejecuta receptores por
  valor y `ref` y devuelve 42.
- `tests/fixtures/nsmeta/historical-1.31.nsmeta`: golden inmutable del formato anterior, generado
  mediante el escritor oficial cuando 1.32 lo convirtió en histórico.
- `docs/3.0/language/extension-methods.md`: contrato bilingüe de sintaxis, lookup, costos,
  módulos y recomendaciones.
- Este documento: trazabilidad del commit atómico F06.

## Mejora obtenida

Las APIs pueden añadir operaciones ergonómicas a tipos propios, importados, primitivos o genéricos
sin wrappers ni modificación de layout. La selección es reproducible: las importaciones visibles
definen el universo, el orden textual no interviene, los miembros reales conservan prioridad y los
conflictos nunca se resuelven silenciosamente. En ejecución el costo es el de una función estática
ordinaria.

## Validación dirigida

- `nsc_ns3_t05_extension_method_tests`: 12/12, O0 y NSLink.
- `nsc_ns3_t49_language_surface_tests`: 27/27.
- `nsc_ns3_t05_opaque_result_tests`: 14/14.
- `nsc_ns3_t05_trait_object_tests`: 12/12.
- `nsc_library_metadata_merger_tests`: 17/17.
- `nsc_stage48_2_library_metadata_tests`: 356/356; golden 1.31 añadido sin sobrescribir anteriores.
- Contratos dirigidos: Item 393 17/17, Item 402 15/15, Item 404 10/10, Item 405 16/16 y Stage 69
  15/15.
- `nsc` compilado; contrato de runtime host estático: passed.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
