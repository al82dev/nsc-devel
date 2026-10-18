# R02B5D1 — análisis único por brazo de `match` statement

## SCOPE LOCK

- Objetivo: eliminar los recorridos repetidos de cada patrón de brazo para bindings, validez OR,
  catch-all, coverage y ownership.
- Naturaleza: optimización y refactor interno sin cambio semántico.
- Archivos productivos autorizados: `Parser.h`, `ExpressionParser.cpp` y
  `StatementParser.cpp`.
- Contratos protegidos: sintaxis, recovery, rangos, orden y texto de diagnósticos, AST, scopes,
  temporales, evaluation count, coverage `match-coverage:v1`, ABI, NSMETA, HIR y MIR.
- Exclusión expresa: R02B5D2 conserva la recolección, comparación y publicación de intervalos
  enteros; esta tanda no cambia su vector ni su complejidad.
- Criterio de cierre: una ficha iterativa por brazo, cero helpers retirados, funciones bajo 60
  líneas, infraestructura completa, TP-PAR-549 y regresiones focales PASS.

## Causa y costo anterior

`parseMatchArmHeader` analizaba el mismo árbol varias veces. El costo por brazo acumulaba:

1. DFS de modos `ref`/`move` y address preservation;
2. DFS de cada nodo OR y, para cada alternativa, una nueva recolección y ordenación de bindings;
3. DFS de catch-all para reachability;
4. DFS independiente de intervalos, reservado a D2;
5. DFS de coverage, con recorridos anidados de catch-all para variantes y listas;
6. dos recolecciones y ordenaciones completas adicionales para decidir ownership del governing.

En una cadena de OR anidados, el punto 2 volvía a visitar subárboles completos y ordenar sus
bindings para cada ancestro: peor caso `O(N² log N)`. Coverage podía repetir recorridos de
subpatrones y ownership volvía a pagar el costo por todos los brazos después del parseo.

## Diseño aplicado

Cada `MatchArm` conserva el `MatchPatternAnalysis` producido una sola vez por
`analyzeMatchExpressionPattern`. El analizador compartido ya usa frames explícitos, resumen
postorden, ordinales preorden y mapas small-to-large. R02B5D1 añade el modo a cada binding que
pierde dirección, sin retirar el vector de rangos consumido por R02B5C.

La ficha se consume así:

- `nonAddressableBindings` reproduce los diagnósticos `ref`/`ref mut`/`move` en preorden;
- `invalidOrRanges` reproduce después los diagnósticos OR en preorden;
- `wildcard` actualiza el estado catch-all sólo cuando el guard es incondicional;
- `cases`, `listLengths`, `wildcard` y `partial` generan el mismo coverage por brazo;
- `bindings` acumula una sola vez `hasReferenceBinding` y `hasMoveBinding` en el estado del
  statement, antes de mover el brazo al vector final;
- el lowering usa esos flags para preservar constness y `local:ref` del governing.

La validación de intervalos permanece separada y sin cambios para R02B5D2. El costo D1 queda en el
costo del analizador compartido y la canonicalización final de bindings, sin rescans por ancestro ni
por consumidor.

## Invariantes preservados

- Orden diagnóstico por brazo: address-preservation, OR, guard constante falso, catch-all e
  interval overlap.
- Texto y rango exactos de los diagnósticos existentes.
- `ref` y `ref mut` conservan nombres distintos en el mensaje de statement match.
- Guards no constantes siguen marcando coverage parcial y no crean catch-all global.
- Casos nominales y longitudes de lista mantienen orden canónico, deduplicación y marcadores
  `~`, `*`, `?`, `#N` y `#N+`.
- El governing se evalúa una vez; temporales, selected flag, orden de brazos, bindings y scopes no
  cambian.
- No se modifica la semántica del analizador para match expressions ni conditional patterns.

## Contrafactual TP-PAR-549

El caso ejecuta cuatro procesos aislados:

1. 32,768 wrappers estructurales y un binding `ref mut` no direccionable: modo y rango exactos.
2. 32,768 OR anidados incompatibles: bindings, catch-all y rangos inválidos en preorden.
3. Statement superficial con binding no direccionable, OR incompatible, guard falso, catch-all y
   overlap: orden exacto de las cinco familias diagnósticas.
4. Statement con variant, lista guardada y binding `ref mut`: coverage, governing `local:ref`,
   evaluation count, payload scope y orden AST.

La infraestructura forma parte de la misma tanda: módulo Host.Parser, CMake suite, declaración,
registro default, diseño, asignación y manifest canónico.

## Gates previstos

- build de `nsc` y los tres targets de suite;
- TP-PAR-549 aislado;
- Host.Parser TP-PAR-369…384 y TP-PAR-547…549, además del módulo completo;
- TL-LANG-311/312/313/315/322;
- CTests T03 fundamental-patterns, exhaustive-match, conditional-patterns y pattern-guards;
- source-registration, CanonicalCatalog, símbolos retirados en cero, funciones bajo 60 líneas y
  `diff --check`.

## Evidencia de cierre

La validación se ejecutó de forma focal sobre el worktree compartido, sin baseline ni batería
completa:

- build de `nsc`: PASS; log `A:\temp\r02b5d1-build-nsc.log`;
- build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS; log
  `A:\temp\r02b5d1-build-suite.log`;
- `TP-PAR-549` aislado: PASS, cuatro procesos y 32,768 niveles en ambos contrafactuales
  profundos; log `A:\temp\r02b5d1-tp-par-549.log`;
- Host.Parser completo: PASS 549/549; log `A:\temp\r02b5d1-host-parser.log`;
- TL-LANG-311/312/313/315/322: PASS 5/5; log `A:\temp\r02b5d1-tl-lang.log`;
- build de los cuatro targets T03 focales: PASS; log `A:\temp\r02b5d1-build-t03.log`;
- CTests `fundamental-patterns`, `exhaustive-match`, `conditional-patterns` y
  `pattern-guards`: PASS 4/4; log `A:\temp\r02b5d1-ctest-t03-rerun.log`.

La primera invocación CTest no alcanzó el producto porque heredó una ruta de runtime inválida en
`C:`. La repetición exacta usó `NSC_WINDOWS_RUNTIME_LIBRARY` desde el artefacto generado en
`A:\nsc-devel\build-test-infrastructure`; `nslink.exe` se resolvió por nombre exactamente a
`A:\nslink-devel\bin\release\nslink.exe` tras reconstruir el PATH del proceso desde Machine+User.

Segunda pasada estática:

- `validate_source_registration.py`: PASS;
- `CanonicalCatalog.load()`: PASS, 6,004 casos, 30 componentes, 682 contratos y TP-PAR-549
  presente;
- cero referencias a los nueve helpers retirados y cero líneas de diff en la ruta de intervalos
  reservada a R02B5D2;
- registro único del módulo en CMake, `TestRegistry` y la ruta default;
- funciones nuevas o modificadas por debajo de 60 líneas;
- `diff --check`: PASS; los avisos de fin de línea pertenecen al worktree y no son errores de
  whitespace.
