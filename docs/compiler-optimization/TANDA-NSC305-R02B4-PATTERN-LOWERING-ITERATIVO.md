# NSC 3.0.5 — R02B4 lowering de patterns iterativo

## SCOPE LOCK

- Objetivo: eliminar la recursión directa, indirecta y mediante factories del lowering de
  patterns, además del rescaneo cuadrático de subárboles `Or` anidados.
- Naturaleza: refactor algorítmico interno; no agrega ni modifica sintaxis.
- Fachada estable: `Parser::lowerPattern` conserva exactamente su firma y sus consumidores.
- Contratos inmutables: topología AST, identidad de la continuación, orden de hijos, orden y
  cantidad de invocaciones de `PatternExpressionFactory`, nombres temporales, rangos, flags,
  auxiliares, modo de binding y clonación histórica de continuaciones `Or`.
- Archivos productivos: `include/nativesharp/Parser/Parser.h` y
  `src/Parser/PatternParser.cpp`.
- Protegido: gramática R02B1, recorridos R02B2, condition builder R02B3, emisión de bindings y
  rutas match/conditional R02B5, diagnósticos, recovery, AST público, ABI, NSMETA, HIR, MIR y
  Driver.
- Criterio de cierre: cero ciclo recursivo del lowering y de sus fuentes derivadas; clasificación
  condition-only memoizada; funciones bajo 60 statements; TP-PAR-545 y regresión focal PASS;
  infraestructura, catálogo, diff y tracking consistentes.

## Call graph histórico y causa

El dispatch histórico era una recursión indirecta:

```text
lowerPattern
  -> lowerOrPattern         -> lowerPattern(child)
  -> lowerTuplePattern      -> lowerPattern(child)
  -> lowerStructPattern     -> lowerPattern(child)
  -> lowerPositionalPattern -> lowerPattern(child)
  -> lowerVariantPattern    -> lowerPattern(first payload)
```

`lowerStructPattern` añadía otra recursión oculta: cada factory `member` capturaba el factory
padre y lo invocaba al materializar una hoja. Un DFS iterativo que hubiera conservado esas
closures aún podría desbordar la pila nativa al evaluar el source final.

Además, cada `Or` no condition-only ejecutaba `patternIsConditionOnly` sobre su subárbol. Una
cadena de N `Or` con un binding al final recorría N + (N-1) + ... + 1 patterns: costo O(N²) antes
de empezar a construir el AST.

Los siete helpers históricos no tenían consumidores independientes: cada uno aparecía solamente
en su declaración privada, definición y dispatch de `lowerPattern`. Se retiran, pero la fachada
pública para el resto del parser permanece.

## Diseño iterativo

`PatternLoweringMachine` concentra una sola ejecución de lowering y usa estructuras heap-backed:

1. una worklist LIFO de fases `Build`, continuaciones de `Or`, secuencias y `Variant`;
2. una pila de valores `AstNode*` terminados;
3. un catálogo de source plans por índice estable;
4. un memo por identidad `Pattern*` para clasificación condition-only.

Los source plans tienen tres formas:

- `Root`: invoca el factory entregado a la fachada;
- `Identifier`: materializa el temporal de tuple, positional o payload;
- `Member`: encadena un acceso de campo sobre otro plan.

`evaluateSource` recorre los planes Member hacia su base, crea el source raíz o identificador una
sola vez y aplica los campos en orden inverso. No existe una closure derivada que invoque otra
closure derivada.

La clasificación condition-only usa frames iterativos con estado de siguiente hijo. Cada nodo
visitado se memoiza como true o false. El primer `Or` profundo cuesta O(N); todos los dispatches
posteriores consultan O(1), eliminando el rescaneo triangular.

## Equivalencia por familia

### Binding y wildcard

- Wildcard devuelve el mismo puntero de continuación.
- Binding con `emitBindings=false` devuelve la misma continuación.
- Binding emitido conserva bloque `[declaration, continuation]`, tipo `var`, const histórico,
  nombre, initializer source y `applyPatternBindingMode`.

### Or

- Crea el bool `prefix_or_selected_N = false` antes de visitar hijos.
- Visita hijos en orden fuente.
- Cada selected body asigna true y usa la continuación original sólo para el último hijo; los
  anteriores reciben `cloneDeclarationNode(continuation)`.
- El matcher de cada hijo queda bajo el mismo guard `if (!selected)` y rango del hijo.
- Un `Or` condition-only continúa por el fallback de condición sin crear temporales de selección.

### Tuple y positional

- Evalúan el source antes de bajar hijos.
- Reservan todos los nombres temporales en orden fuente.
- Conservan `prefix_tuple_N_index` y `prefix_pos_N_index`, incluyendo el contador global
  `nextMatchTemporary_`.
- Bajan hijos de derecha a izquierda para envolver la misma continuación.
- Positional conserva el `TypeSyntax` nominal; Tuple no lo agrega.
- El bloque final mantiene `[deconstruction, lowered continuation]`.

### Struct

- Baja campos de derecha a izquierda.
- Cada hijo recibe un source plan Member con el nombre de `fieldNames[index]`.
- La materialización final conserva el orden raíz-a-hoja y la cantidad histórica de llamadas al
  factory raíz.

### List

- Permanece deliberadamente en la frontera de R02B5 para emisión de bindings.
- Ejecuta `emitPatternBindings` primero, agrega después la continuación y construye al final la
  condición mediante el builder R02B3, en el mismo orden observable anterior.

### Variant

- Construye primero `Is<Case>()`.
- Payload ausente o wildcard agrega directamente la continuación.
- Payload no trivial conserva el extractor `__nsc_variant_extract_<Case>`, auxiliar
  `variant-pattern-extract`, temporal `prefix_payload_N`, declaración const y sólo el primer hijo.
- El payload hijo se baja mediante un source plan Identifier sin closure recursiva.

### Fallback condition-only

- Conserva `IfStatement(makePatternCondition(pattern, source), continuation)` para Literal,
  Range, Relational, Not, And, Or condition-only y demás clases históricas.
- No redefine patterns estructuralmente inválidos ni agrega recovery nuevo.

## Contrafactual TP-PAR-545

El caso usa cuatro procesos hijos separados para limitar memoria y distinguir contratos:

1. 32,768 Struct anidados terminados en binding por referencia: valida un source plan Member de
   profundidad completa, una sola llamada al factory raíz, auxiliar `local:pattern-ref` y la
   identidad exacta de la continuación.
2. 32,768 Tuple/Positional alternados: valida bloques de deconstruction, source temporales,
   nombres monotónicos, TypeSyntax positional, orden de envoltura y una sola llamada raíz.
3. 32,768 Variant anidados terminados en binding: valida dos evaluaciones del source raíz,
   extractores, auxiliares, nombres payload monotónicos y continuación exacta.
4. 32,768 Or unarios terminados en binding: valida lowering sin pila nativa, nombres selected,
   guards, selected bodies, source único y el caso contrafactual que antes provocaba rescaneo
   O(N²).

Los `Pattern` profundos se dejan vivir hasta la salida del proceso aislado. Destruir una cadena
de `vector<Pattern>` de 32,768 niveles probaría recursión del contenedor estándar, no del lowering.

## Infraestructura

TP-PAR-545 queda registrado en el mismo cambio en:

- `tests/compiler-suite/CMakeLists.txt`;
- `tests/compiler-suite/host/include/nsc_suite/TestRegistry.h`;
- `tests/compiler-suite/host/src/main.cpp`;
- `docs/compiler-testing-reengineering/inventory/test-case-design.csv`;
- `docs/compiler-testing-reengineering/inventory/suite-case-allocation.csv`;
- `tests/compiler-suite/manifest/cases.jsonl`.

Usa el contrato existente CP-PAR-042, componente `Host.Parser`, perfil `NIGHTLY` y executor
`nsc_component_tests`. `PatternParser.cpp` sigue llegando a la suite mediante su shim productivo;
no se compila una segunda copia directa.

## Estado de validación

- Build focal `nsc`: PASS; `PatternParser.cpp`, fachada y frontend enlazaron correctamente; log
  `A:\temp\r02b4-build-nsc.log`.
- Build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS; el módulo TP-PAR-545 y el shim productivo de
  PatternParser compilaron en el mismo runner; log
  `A:\temp\r02b4-build-suite-three-targets.log`.
- TP-PAR-545: PASS 1/1, exit `0`, 581,690 us; los cuatro procesos aislados completaron 32,768
  niveles y comprobaron Struct, Tuple/Positional, Variant y Or; log
  `A:\temp\r02b4-tp-par-545-final.log`.
- Regresión focal TP-PAR-349..410 + TP-PAR-525..544: PASS 82/82, exit `0`; incluye
  explícitamente TP-PAR-366/367 y los contrafactuales R02A/R02B1/R02B2/R02B3; log
  `A:\temp\r02b4-pattern-regression-82.log`.
- `validate_source_registration.py`: PASS, exit `0`; log
  `A:\temp\r02b4-source-registration.log`.
- `CanonicalCatalog`: PASS, 6,000 casos, 30 componentes y 682 contratos; TP-PAR-545 se descubre
  como `Host.Parser/NIGHTLY/nsc_component_tests`; log
  `A:\temp\r02b4-canonical-catalog.log`.
- Gate conservador de tamaño: producción máximo 36 líneas físicas en `startVariant`;
  contrafactual máximo 30 en `testDeepOrLowering`; todas las funciones quedan bajo 60.
- Los siete helpers recursivos históricos tienen cero declaraciones, definiciones o llamadas.
  El registro del módulo aparece exactamente una vez en CMake, TestRegistry y `main.cpp`.
- Diff-check y whitespace: PASS sobre las rutas R02B4.
- Tracking externo e índice Git se cierran sólo después de releer el estado concurrente y aislar
  este cambio de M02O y de `ProjectMethodBodyModelTests.cpp`.
