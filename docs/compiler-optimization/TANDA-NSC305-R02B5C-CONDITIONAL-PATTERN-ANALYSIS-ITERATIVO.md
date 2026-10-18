# R02B5C — validación y lowering de conditional patterns iterativos

## Alcance

Esta hoja elimina la recursión de pila nativa y el rescaneo cuadrático que permanecían en las
formas `if (let ...)`, `while (let ...)` y `let ... else`. No modifica la gramática, el AST
público, los contratos de scopes, recovery, diagnósticos, ABI, NSMETA, HIR ni MIR. Tampoco invade
el análisis de `match` statements reservado a R02B5D.

La máquina `StructuredControlParser` ya era iterativa para bodies, `else if`, bloques y validación
de divergencia. El residuo estaba concentrado en:

- `collectPatternBindings`, un DFS recursivo seguido por sort;
- las lambdas recursivas `validateBindingModes` y `validateOrBindings`;
- el nuevo recorrido completo de bindings dentro de `lowerConditionalPattern`;
- el rescaneo de cada subárbol alternativo para cada `Or` ancestro;
- `parseConditionalPatternHeader` y `lowerConditionalPattern`, que excedían 60 líneas.

## Diseño

`MatchExpressionPatternAnalyzer` conserva su contrato B5B y publica dos hechos genéricos nuevos:

- rangos de nodos `Or` cuyas alternativas no poseen el mismo multiset exacto `(nombre, modo)`;
- rangos de bindings `ref`/`move` que cruzan un nodo `Tuple` o `Variant` y pierden dirección.

El analizador usa frames explícitos, recorrido postorden, ordinales preorden y mapas de conteos con
merge small-to-large. No usa llamadas recursivas ni vuelve a recorrer subárboles por cada `Or`.
Los hechos no alteran los diagnósticos de match expressions. El consumidor conditional publica sus
dos familias diagnósticas en el orden histórico: primero todos los errores de dirección en preorden
y después todos los errores `Or` en preorden.

El resultado conditional conserva `hasReferenceBinding` y `hasMoveBinding` dentro de su estado de
parseo. El lowering deja de recolectar y ordenar bindings una segunda vez. Sus decisiones permanecen
idénticas:

- un binding de referencia marca el temporal con `local:ref`;
- sólo ausencia simultánea de referencia y move habilita la copia ordinaria del source;
- `if-let` evalúa el source una vez y limita bindings a la rama de éxito;
- `while-let` evalúa el source una vez por iteración y emite el break de `!matched`;
- `let-else` valida divergencia, consume su `;`, difiere bindings hasta después de la rama fallida y
  conserva `conditional-pattern-inline`;
- los nombres `$__nsc_conditional_pattern_N` y `_matched`, rangos y orden de hijos no cambian.

La orquestación se divide en prefix, payload, análisis, prelude, success block, lowering while,
lowering selection y finalización. Cada función nueva o modificada queda por debajo de 60 líneas.

## Contrafactual TP-PAR-548

`TP-PAR-548` ejecuta cuatro procesos aislados:

1. 32,768 wrappers sobre un tuple con binding `ref`: valida cero recursión, propagación exacta de
   address-preservation, flags de binding, rango y mensaje causal.
2. 32,768 nodos `Or` anidados con alternativa incompatible: valida complejidad no cuadrática,
   conteo completo y publicación preorden de rangos y mensajes.
3. Un árbol corto con tuple, variant, `ref`, `move` y dos `Or` incompatibles: valida que ambas
   familias diagnósticas conserven orden, rango, severidad y texto exactos.
4. Formas superficiales `if-let`, `while-let` y `let-else`: valida temporales, `_matched`, source
   único, `local:ref`, loop/break y marker inline sin cambiar el AST observable.

La infraestructura queda completa en el mismo cambio: fuente Host.Parser, CMake de suite,
declaración y registro default, diseño, asignación y manifest canónico.

## Gates de cierre

Antes de cerrar la hoja deben pasar:

- build de `nsc` y los tres targets de suite;
- `TP-PAR-548` aislado;
- regresión Host.Parser de control-flow y patterns, incluyendo TP-PAR-333..368 y TP-PAR-525..548;
- `nsc.ns3.t03.conditional-patterns`;
- `nsc.ns3.t03.fundamental-patterns`;
- registro estático, `CanonicalCatalog`, símbolos únicos, umbral de funciones y `diff --check`.

## Evidencia de cierre

La validación focal se ejecutó sobre el worktree compartido, sin baseline ni batería completa:

- build de `nsc`: PASS; log `A:\temp\r02b5c-build-nsc.log`;
- build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS; log
  `A:\temp\r02b5c-build-suite-rerun.log`;
- `TP-PAR-548` aislado: PASS, cuatro procesos y 32,768 niveles en ambos contrafactuales
  profundos; log `A:\temp\r02b5c-tp-par-548-final.log`;
- Host.Parser completo: PASS 548/548; log `A:\temp\r02b5c-host-parser.log`;
- build de los targets `nsc_ns3_t03_conditional_pattern_tests` y
  `nsc_ns3_t03_fundamental_pattern_tests`: PASS; log
  `A:\temp\gate-f04-build-t03-fixtures.log`;
- CTests `nsc.ns3.t03.conditional-patterns` y `nsc.ns3.t03.fundamental-patterns`: PASS 2/2;
  log `A:\temp\gate-f04-ctest-t03-patterns.log`;
- `nslink.exe` se resolvió por nombre exactamente a
  `A:\nslink-devel\bin\release\nslink.exe` después de reconstruir el PATH del proceso desde
  Machine+User; no se usó ruta absoluta para invocarlo.

Los CTests T03 descubrieron una fixture histórica anterior a `70a5a1904`: tres bindings escritos
usaban `ref`, que desde ese commit es readonly, en lugar de `ref mut`. El gate se reparó por
separado y quedó aislado en `d21014f8a`; no forma parte del diferencial R02B5C ni altera el
contrato del lenguaje.

Segunda pasada estática:

- `validate_source_registration.py`: PASS;
- `CanonicalCatalog.load()`: PASS, 6,003 casos, 30 componentes, 682 contratos y TP-PAR-548
  presente;
- cero referencias restantes a `collectPatternBindings`, `validateBindingModes` o
  `validateOrBindings` en `StatementParser.cpp`;
- registro único del módulo en CMake, `TestRegistry` y la ruta default;
- helpers productivos y funciones del contrafactual por debajo de 60 líneas;
- `diff --check`: PASS para los cambios de esta hoja; los avisos de fin de línea corresponden al
  worktree compartido y no son errores de whitespace.
