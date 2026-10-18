# NSC 3.0.5 — R03C3: precedencia binaria Pratt iterativa

## SCOPE LOCK

- Objetivo: sustituir exclusivamente la escalera recursiva de diez niveles binarios por una
  tabla de binding powers y una agenda Pratt iterativa.
- Revisión base: `112c1bc5405654168ffb86675759fd3360f63a0e`.
- Dependencias ya presentes en la base: cierre integrado postfix/primary R03C5C y migración
  unary R03C4.
- Archivos productivos autorizados: `Parser.h` y `ExpressionParser.cpp`.
- Contratos protegidos: gramática, precedencia, asociación, AST, tokens, rangos, cursor,
  diagnósticos, recovery, API, ABI y NSMETA.
- Frontera superior protegida: range, null-coalescing, conditional y assignment permanecen
  fuera del nivel binario y conservan sus binding powers históricos.
- Fronteras laterales protegidas: `loop` y `comprehend` pertenecen a R03A/R03B; HIR, MIR,
  driver y metadata quedan fuera de esta hoja.
- Validación autorizada: compilar sólo `nsc_component_tests` y ejecutar sólo `TP-PAR-565`.
- Prohibido: otros casos, Host.Parser completo, CTest, suite, baseline, deploy, instalación,
  red, cambios en `development` o en `ProjectMethodBodyModelTests.cpp`.

## Causa

La implementación anterior expresaba la precedencia mediante diez métodos encadenados:
logical-or, logical-and, bitwise-or, bitwise-xor, bitwise-and, equality, relational, shift,
additive y multiplicative. Cada nivel llamaba al siguiente antes de poder consumir su propio
operador. Aunque una cadena homogénea se reducía con un bucle, cada operando atravesaba de nuevo
la escalera y la estructura repartía el contrato entre métodos, tablas lineales y un caso
especial para `is`/`as`.

R03C3 elimina esa escalera y hace que el nivel binario entre una sola vez a
`PostfixIterationMachine`. La precedencia deja de estar codificada en llamadas C++ y pasa a una
clasificación O(1) por token. Los operandos y continuaciones viven en la misma worklist LIFO que
ya gobierna unary, postfix y primary, por lo que la profundidad binaria válida no consume pila
nativa.

## Tabla canónica de operadores

`binaryOperatorInfo` es la única fuente de binding powers binarios:

| binding power izquierdo | operadores | binding power derecho | RHS | AST |
|---:|---|---:|---|---|
| 5 | `||` | 6 | expresión | `BinaryExpression` |
| 6 | `&&` | 7 | expresión | `BinaryExpression` |
| 7 | `|` | 8 | expresión | `BinaryExpression` |
| 8 | `^` | 9 | expresión | `BinaryExpression` |
| 9 | `&` | 10 | expresión | `BinaryExpression` |
| 10 | `==`, `!=` | 11 | expresión | `BinaryExpression` |
| 11 | `<`, `<=`, `>`, `>=` | 12 | expresión | `BinaryExpression` |
| 11 | `is` | 12 | tipo | `TypeTestExpression` |
| 11 | `as` | 12 | tipo | `SafeCastExpression` |
| 12 | `<<`, `>>` | 13 | expresión | `BinaryExpression` |
| 13 | `+`, `-` | 14 | expresión | `BinaryExpression` |
| 14 | `*`, `/`, `%` | 15 | expresión | `BinaryExpression` |

El binding power derecho es siempre el izquierdo más uno, preservando asociación izquierda.
`is` y `as` comparten nivel relacional, pero clasifican explícitamente su RHS como
`TypeSyntax`; no se reinterpretan como operadores de dos expresiones.

## Diseño iterativo

`parseLogicalOrExpression` se conserva como fachada privada para no alterar la gramática ni los
call sites. Su implementación crea una `PostfixIterationMachine` y llama `parseBinary`, cuya
raíz es un trabajo `Expression` con precedencia mínima 5.

La máquina usa `parseRoot` como único drain para sus tres entradas actuales: postfix, unary y
binary. Esto evita duplicar el bucle de despacho y mantiene una sola sesión de métricas. Para
cada operador binario:

1. `continueExpression` consulta una vez `binaryOperatorInfo`;
2. guarda el operando izquierdo y el token en una continuación;
3. agenda el RHS con el binding power derecho;
4. al volver, `finishBinary` construye exactamente un nodo y reanuda el nivel exterior;
5. para `is`/`as`, `finishTypeRelation` consume un tipo y conserva el comportamiento histórico
   cuando `parseType` no produce resultado.

Los binding powers 1 a 4 siguen reservados para assignment, conditional, null-coalescing y
range. La entrada binaria con mínimo 5 no los absorbe; cuando la gramática superior reentra a la
máquina completa, esos niveles usan sus continuaciones preexistentes.

Cada transición consume un token, completa un slot o agenda un trabajo distinto. La ruta tiene
O(T) tiempo, O(N) almacenamiento explícito y O(1) pila nativa respecto de la cantidad de
operadores binarios. No hay búsquedas lineales por nivel ni reconstrucción repetida de la
escalera.

## Auditoría de equivalencia

- Los diez niveles conservan exactamente su orden histórico.
- Los operadores ordinarios conservan `BinaryExpression`, `tokenKind`, `first`, `second` y
  rango unido de izquierda a derecha.
- La asociación ordinaria sigue siendo izquierda; los paréntesis siguen imponiendo el árbol.
- `is` conserva `TypeTestExpression`, token, operando izquierdo y `typeSyntax`.
- `as` conserva `SafeCastExpression`, token, operando izquierdo y `typeSyntax`.
- Range, null-coalescing, conditional y assignment no cambian de propietario ni son absorbidos
  por `parseBinary`.
- Un RHS ausente conserva progreso de cursor y `ExpectedExpression`.
- Las declaraciones y definiciones de la escalera antigua, sus diez tablas y el helper de
  búsqueda lineal fueron retirados; no quedan símbolos residuales en `Parser.h` ni
  `ExpressionParser.cpp`.

## Contrafactual puntual

`TP-PAR-565` (`CP-PAR-036`, `Host.Parser`, `NIGHTLY`) ejecuta cuatro probes aislados bajo un
solo caso seleccionable:

1. árbol dorado que cruza los diez niveles y verifica además `is`/`as` con RHS de tipo;
2. 32,768 operadores alternados, con recorrido iterativo del AST, cardinalidad, rango completo,
   cursor EOF, cero diagnósticos, profundidad activa uno y cero slots pendientes;
3. asociación izquierda, agrupación por paréntesis y frontera superior
   `a + b ?? c ? d : e = f`;
4. RHS ausente en cada uno de los diez niveles, con `ExpectedExpression`, progreso a EOF y
   cierre del scheduler.

La infraestructura registra el módulo en CMake, `TestRegistry`, `main`, manifest, diseño y
allocation. El selector focal es `--case TP-PAR-565`; no autoriza ningún caso adicional.

La primera ejecución focal detectó una expectativa incorrecta del nuevo probe de frontera: la
gramática histórica produce `ConditionalExpression` como raíz y coloca el assignment en la rama
false. Se corrigió únicamente el oráculo para representar ese contrato; no se cambió el producto
después de ese resultado. La repetición focal pasó.

## Evidencia

- Configuración focal: PASS con las herramientas nativas oficiales; todos los artefactos,
  temporales, cachés y logs permanecieron en A:.
- Producto seleccionado: `A:\nsc-devel\build-test-infrastructure\bin\nsc.exe`.
- Identidad seleccionada:
  `A:\nsc-devel\build-test-infrastructure\generated\nativesharp\Basic\BuildId.h`.
- Runtime seleccionado:
  `A:\nsc-devel\build-test-infrastructure\generated\NativeSharp.Runtime.Win32.lib`.
- NSLink seleccionado: `A:\nslink-devel\bin\release\nslink.exe`.
- Target único: PASS `nsc_component_tests`; build inicial `325/325` y rebuild de la corrección
  del oráculo `2/2`.
- Selector único final: PASS `--case TP-PAR-565`; `duration_us=234886`, cero failures.
- Registro de fuentes: PASS `validate_source_registration`.
- Catálogo canónico: PASS, 6,024 casos, 30 componentes, 682 contratos y una sola ocurrencia de
  `TP-PAR-565`.
- Logs reservados:
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c3-configure.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c3-build.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c3-build-retry.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c3-tp-par-565.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c3-tp-par-565-retry.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c3-source-registration.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c3-canonical-catalog.log`.

## Alcance exacto de la validación

La evidencia demuestra compilación del target focal, registro consistente y paridad de los
contratos observados por TP-PAR-565. No se ejecutaron otros casos, CTest, suite, baseline,
instalación ni consumidores externos. La batería amplia queda reservada al usuario después del
cierre de todas las hojas.

## Criterio de cierre

R03C3 queda listo para integración cuando el target exacto compila, TP-PAR-565 pasa, el catálogo
contiene una sola fila por registro, el diff no conserva la escalera binaria recursiva y el
commit aislado no contiene trabajo de R03A, R03B, ABI, NSMETA o tracking global.
