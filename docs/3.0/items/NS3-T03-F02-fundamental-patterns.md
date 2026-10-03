# NS3-T03-F02 — Fundamental patterns

## Estado

`EN CURSO`. Wildcard, literal, binding, tuple, struct, variant, integer range and binding-free OR
patterns are productive and execute at O0 through NSLink. Binding-bearing OR patterns wait for the
T03-F03 arm-merge model; partial owner/payload moves and aliasing through copied tuple/variant
payloads wait for T06 drop/borrow tracking and are causally rejected.

## Qué se hizo

- Se sustituyó el parser monolítico de `Caso(nombre)` por un árbol recursivo de patterns.
- Se preservó `Caso(nombre)` como binding value compatible y se añadió `Caso(var nombre)` explícito.
- Se implementaron literales, rangos enteros semiabiertos, OR sin bindings y detección estática de
  rangos solapados.
- Se implementó nesting arbitrario de variants, tuples y campos de structs.
- Los tuple patterns reutilizan la deconstrucción T02-F03, incluidos su chequeo de aridad, layout y
  lifetime trivial; no existe una segunda ABI de tuple.
- `ref` usa storage direccionable real y conserva alias; el `ref var` interno infiere el tipo pero
  sigue pasando por exactitud de tipo, mutabilidad, addressability y escape analysis ordinarios.
- La expresión gobernante se materializa una vez. Un booleano compiler-owned evita evaluar brazos
  posteriores después de un match sin introducir un loop que cambie el significado de `break`.
- `move` sobre valores triviales se representa como binding mutable explícito. Ownership parcial no
  se simula y permanece bloqueado hasta T06.
- Se añadieron diagnósticos causales para float patterns, rangos invertidos/solapados, OR-bindings
  incompatibles y ref/move que cruzarían copias intermedias.

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
- `nsc_ns3_t03_fundamental_pattern_tests`: 6/6; el fixture O0 enlazado con NSLink termina en 42.
- `nsc_ns3_t03_tagged_variant_tests`: 20/20.
- `nsc_ns3_t02_deconstruction_tests`: 6/6.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
