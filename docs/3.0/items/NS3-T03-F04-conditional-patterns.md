# NS3-T03-F04 — Conditional patterns

## Estado

`TERMINADA`. `if let`, `while let` y `let ... else` son productivos en edición 3, bajan al mismo
árbol de patterns que `match` y llegan a O0/NSLink.

## Qué se hizo

- Se extrajo el parser y lowering de patterns de `match` a infraestructura privada compartida.
- Se añadieron las gramáticas `if (let P = expression)`, `while (let P = expression)` y
  `let P = expression else { ... };`.
- La expresión fuente se materializa una vez; tags y payloads respetan short-circuit.
- Los bindings de `if let` y `while let` quedan confinados al camino exitoso.
- `let ... else` publica bindings inicializados en el bloque contenedor sólo después de una rama
  fallida divergente.
- La divergencia estructural cubre `return`, `throw`, `break`, `continue` y `if` total divergente.
- Las sentencias embebidas sin llaves funcionan para `if let` y `while let` bajo las reglas ya
  existentes del lenguaje.
- Se preservaron las restricciones causales de `ref`/`move` y OR patterns del matcher común.

## Archivos modificados

- `include/nativesharp/Parser/Parser.h`: representación y servicios compartidos de patterns.
- `src/Parser/Parser.cpp`: parser/lowering común, gramáticas condicionales, scopes y validación de
  divergencia.
- `CMakeLists.txt`: target exclusivo para la prueba dirigida F04.

## Archivos nuevos y por qué

- `tests/unit/NativeSharp3T03ConditionalPatternTests.cpp`: ejecución O0/NSLink, evaluación única,
  loops, nesting, alcance, borrow `ref`, edición, divergencia y equivalencia MIR con `match`.
- `docs/3.0/language/conditional-patterns.md`: contrato bilingüe de sintaxis, flujo, alcance e
  inicialización.
- Este documento: evidencia atómica del ítem y de sus límites explícitos.

## Mejora obtenida

El código que antes necesitaba un `match` completo o una prueba manual de tag ahora expresa el
camino exitoso directamente. El compilador no duplica el evaluador de patterns: corregir una regla
estructural beneficia simultáneamente a `match`, `if let`, `while let` y `let ... else`.

## Validación dirigida

- `nsc` compilado de forma incremental con clang-cl/lld-link y runtime VS2026 estático.
- `nsc_ns3_t03_conditional_pattern_tests`: 6/6 con programas O0 y NSLink local.
- `nsc_ns3_t03_tagged_variant_tests`: 20/20.
- `nsc_ns3_t03_fundamental_pattern_tests`: 6/6.
- `nsc_ns3_t03_exhaustive_match_tests`: 9/9.

No se ejecutó baseline, CTest general, O1/O2/O3, instalación ni deploy.
