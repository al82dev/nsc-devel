# NS3-T03-F03 — Exhaustive match

## Estado

`EN CURSO`. Exhaustividad y utilidad de `match` statement sobre variants concretos, genéricos
locales y variants rehidratados desde NSMETA son productivas. La forma `match` expresión, join de
tipos/ownership, retorno `ref`, jump-table y cleanup de valores de brazo siguen pendientes.

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

## Archivos modificados

- `src/Parser/Parser.cpp`: resumen estable de cobertura y eliminación del wildcard obligatorio en
  el parse temprano.
- `src/Frontend/ProjectMethodBodyModel.cpp`: resolución de dominio y diagnósticos productivos.
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
biblioteca precompilada.

## Validación dirigida

- `nsc_ns3_t03_exhaustive_match_tests`: 9/9 con O0 y NSLink local.
- `nsc_ns3_t03_sum_type_tests`: 10/10.
- `nsc_ns3_t03_tagged_variant_tests`: 20/20.
- `nsc_ns3_t03_fundamental_pattern_tests`: 6/6.
- Fixture `c59_ns3_variants`: compila en O0, enlaza con NSLink y termina en 42.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
