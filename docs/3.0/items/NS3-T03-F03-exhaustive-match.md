# NS3-T03-F03 — Exhaustive match

## Estado

`TERMINADA`. `match` statement y expresión comparten exhaustividad/utilidad sobre variants locales,
genéricos y rehidratados desde NSMETA. La forma expresión tiene AST/HIR/MIR 1.26 propios, join de
tipo/ownership, resultado `ref`, cleanup por brazo y selección branch/tag-switch sin objeto runtime.

## Qué se hizo

- El parser conserva un resumen compiler-owned de cobertura junto al bloque bajado.
- El body model combina ese resumen con el tipo real del temporal gobernante.
- El dominio variant se obtiene de la declaración nominal o de los factories marcados que
  sobreviven especialización genérica y rehidratación NSMETA.
- Se conectó `language::analyzeClosedMatch` al pipeline productivo.
- Patterns de payload irrefutables cubren el caso; patterns restrictivos cuentan como útiles pero
  no aportan exhaustividad.
- Dominios abiertos siguen exigiendo wildcard/binding total.
- Se endureció utilidad: un wildcard posterior a cobertura completa es inalcanzable.
- Se retiraron de fixtures 3.0 los wildcards provisionales que ya eran redundantes.
- Se añadió `match (value) { pattern => expression, ... }` con evaluación única del governing.
- Cada brazo obtiene scope propio para bindings; un guard fallido descarga sus temporales antes de
  probar el brazo siguiente y un brazo seleccionado descarga después de obtener/mover el resultado.
- El join ignora brazos `never`, calcula tipo numérico común o conversión implícita única y conserva
  ownership sólo cuando todos los caminos coinciden.
- `=> ref place` exige storage mutable/addressable, todos los brazos deben ser `ref` y compartir
  tipo; el análisis de escape combina los orígenes de todos los resultados.
- Variants cerrados con patterns de caso completos y sin guard bajan a una carga única de
  `__nsc_variant_tag` y `switch`; patterns parciales, guards y dominios abiertos conservan ramas
  ordenadas. Ninguna ruta crea heap, boxing ni un objeto match.

## Archivos modificados

- `src/Parser/Parser.cpp`: resumen estable de cobertura y eliminación del wildcard obligatorio en
  el parse temprano.
- `src/Frontend/ProjectMethodBodyModel.cpp`: resolución de dominio y diagnósticos productivos.
- `include/nativesharp/AST/AstNodeKind.h`, `src/AST/AstNodeKind.cpp`,
  `include/nativesharp/Parser/Parser.h` y `src/Parser/Parser.cpp`: sintaxis y AST propios para
  expresión, brazo y valor gobernante.
- `include/nativesharp/Frontend/ProjectMethodBodyModel.h`: binding estable de resultado,
  conversiones, referencia y tags de dispatch.
- `include/nativesharp/Frontend/ProjectOopHirModel.h`,
  `src/Frontend/ProjectOopHirModel.cpp`, `include/nativesharp/Frontend/ProjectOopMirModel.h`,
  `src/Frontend/ProjectOopMirModel.cpp` y dumper MIR: nodos inspeccionables, cleanups y schema 1.26.
- `src/LLVM/ProjectOopLlvmLowerer.cpp`: evaluación única, CFG por brazos, phi/ref result,
  cleanup de guard/resultado y switch por tag cuando es seguro.
- `src/Language/SumTypeSemantics.cpp`: utilidad correcta tras cobertura completa.
- `tests/unit/NativeSharp3T03SumTypeTests.cpp`: regresión del wildcard redundante.
- `tests/unit/NativeSharp3T03TaggedVariantTests.cpp` y
  `tests/cc1/c59_ns3_variants/Main.ns`: uso de exhaustividad cerrada sin wildcard.
- `CMakeLists.txt`: registro de la prueba exclusiva del ítem.
- `docs/3.0/language/fundamental-patterns.md`: frontera actualizada entre dominio abierto/cerrado.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T03ExhaustiveMatchTests.cpp`: ejecución directa y cross-module, casos
  faltantes, payload parcial, dominio abierto y brazos inalcanzables.
- `docs/3.0/language/exhaustive-match.md`: contrato bilingüe de cobertura, utilidad y lowering.
- Este documento: evidencia por commit y deuda explícita conforme a la metodología.

## Mejora obtenida

El usuario ya no necesita un `_` defensivo que oculte casos futuros o resulte muerto. El compilador
prueba la totalidad contra la identidad nominal transportada y señala exactamente qué caso falta.
La misma regla se aplica dentro del proyecto, sobre especializaciones genéricas y al consumir una
biblioteca precompilada. `match` puede producir un valor sin materializar un carrier adicional; los
brazos incompatibles fallan en sema y el backend recibe un único tipo físico. El dispatch cerrado
evita repetir `IsCase`, mientras que la ruta general conserva orden, guards y short-circuit.

## Validación dirigida

- `nsc_ns3_t03_exhaustive_match_tests`: 15/15 con O0 y NSLink local; incluye valor/ref, evaluación
  única, join numérico, rechazos y evidencia MIR `dispatch=tag-switch`.
- `nsc_ns3_t03_sum_type_tests`: 10/10.
- `nsc_ns3_t03_tagged_variant_tests`: 20/20.
- `nsc_ns3_t03_fundamental_pattern_tests`: 6/6.
- Fixture `c59_ns3_variants`: compila en O0, enlaza con NSLink y termina en 42.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
