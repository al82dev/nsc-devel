# NS3-T03-F02 — Fundamental patterns

## Estado

`TERMINADA`. Wildcard, literal, binding, tuple, struct, variant, integer range and OR patterns are
productive and execute at O0 through NSLink. OR alternatives may bind values when every alternative
publishes the same names and modes. `ref` remains restricted to address-preserving paths; tuple
deconstruction and variant payload extraction are value projections and reject a false alias.

## Qué se hizo

- Se sustituyó el parser monolítico de `Caso(nombre)` por un árbol recursivo de patterns.
- Se preservó `Caso(nombre)` como binding value compatible y se añadió `Caso(var nombre)` explícito.
- Se implementaron literales, rangos enteros semiabiertos, OR con o sin bindings compatibles y
  detección estática de rangos solapados.
- Se implementó nesting arbitrario de variants, tuples y campos de structs.
- Los tuple patterns reutilizan la deconstrucción T02-F03, incluidos su chequeo de aridad, layout y
  lifetime trivial; no existe una segunda ABI de tuple.
- `ref` usa storage direccionable real y conserva alias; el `ref var` interno infiere el tipo pero
  sigue pasando por exactitud de tipo, mutabilidad, addressability y escape analysis ordinarios.
- La expresión gobernante se materializa una vez. Un booleano compiler-owned evita evaluar brazos
  posteriores después de un match sin introducir un loop que cambie el significado de `break`.
- Los OR-patterns con bindings se distribuyen en alternativas completas. Cada alternativa tiene su
  propio scope y copia sintáctica del guard/resultado; `let ... else` fusiona sus bindings mediante
  una expresión `match` interna, sin exponer nombres sin inicializar.
- Se corrigió el predicado de un binding dentro de una expresión `match`: un binding es una prueba
  irrefutable, no `false`. Con ello los payloads seleccionados ya llegan al valor del brazo.
- `move` sobre valores triviales se representa como binding mutable explícito. Un `move` que
  necesitaría extraer parcialmente storage propietario a través de una proyección por valor se
  rechaza por el contrato de addressability, no se simula como copia.
- Se añadieron diagnósticos causales para float patterns, rangos invertidos/solapados, OR-bindings
  incompatibles y `ref`/`move` que cruzarían copias intermedias.

## Archivos modificados

- `src/Parser/Parser.cpp`: gramática recursiva, validaciones y lowering de patterns.
- `src/Frontend/ProjectMethodBodyModel.cpp`: inferencia de tipo para el `ref var` interno conservando
  todas las validaciones semánticas existentes.
- `CMakeLists.txt`: registro de la prueba exclusiva del ítem.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T03FundamentalPatternTests.cpp`: ejecución O0 y diagnósticos aislados de
  nesting, tuple/struct/variant, range, OR, value/ref/move y fronteras de ownership.
- `docs/3.0/language/fundamental-patterns.md`: contrato bilingüe de gramática, evaluación y ownership.
- Este documento: evidencia por commit y deuda explícita conforme a la metodología.

## Mejora obtenida

`match` dejó de ser un selector limitado a un nombre de caso y un payload plano. Ahora puede
describir datos nativos anidados sin allocation ni objeto runtime, conserva evaluación única y
rechaza en el frontend los casos que todavía no tienen una representación de ownership honesta.

## Validación dirigida

- `nsc` se recompiló de forma aislada con host clang/lld-link y runtime MSVC estático.
- `nsc_ns3_t03_fundamental_pattern_tests`: 6/6; el fixture O0 enlazado con NSLink termina en 42 y
  cubre OR-bindings en sentencia, expresión, `if let` y `let ... else`, además de alias `ref` por
  campo de struct.
- Target `nsc`: compilación incremental correcta y contrato de runtime host estático aprobado.

No se ejecutó baseline, CTest, suite general, O1/O2/O3, instalación ni deploy.
