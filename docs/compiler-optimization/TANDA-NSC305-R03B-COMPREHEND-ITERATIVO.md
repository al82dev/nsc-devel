# NSC 3.0.5 — R03B: `comprehend` iterativo

## SCOPE LOCK

- Objetivo: retirar exclusivamente las reentradas recursivas de `comprehend` entre expresión,
  builder, bloque y `yield`, reutilizando la agenda explícita compartida del parser.
- Revisión base: `ecb51bc7dfd11a09592c1cad9b5fff8c2bab2237`.
- Dependencias presentes: Pratt integrado R03C5C/R03C4/R03C3 y `loop` iterativo R03A.
- Archivos productivos autorizados: `Parser.h`, `ParserContext.cpp`, `ExpressionParser.cpp`,
  `StatementParser.cpp` y `ParserContinuationCoordinator.cpp`.
- Contratos protegidos: sintaxis, AST, rangos, orden de evaluación, builder único, contextos LIFO,
  `yield`, recovery, nombres sintéticos, diagnósticos, API, ABI y NSMETA.
- Casos históricos protegidos: `TP-PAR-387` a `TP-PAR-394`; no se modificaron ni ejecutaron.
- Fronteras laterales protegidas: HIR, MIR, driver, LLVM, metadata y tracking global.
- Validación autorizada: compilar sólo `nsc_component_tests` y ejecutar sólo `TP-PAR-571`.
- Prohibido: otros casos, CTest, suite, minibaseline, baseline, `nsc.exe`, deploy, instalación,
  red, cambios en `development` o en `tests/unit/ProjectMethodBodyModelTests.cpp`.

## Causa

La máquina Pratt detectaba `comprehend`, pero salía de su worklist mediante
`parseComprehensionExpression`. Esa función volvía a entrar de manera síncrona a `parseExpression`
para el builder y a `parseBlock` para el body. Dentro del body, la ruta histórica de
`parseComprehensionYieldStatement` volvía a llamar `parseExpression` para el valor producido.

Por tanto, una cadena válida de comprehensions anidados representaba su profundidad lógica con la
pila nativa. Además, `comprehensionExpressionStartsHere` recorría desde cada candidato hasta su
paréntesis de cierre, por lo que candidatos repetidos podían introducir barridos cuadráticos.

## Diseño implementado

`ComprehensionExpression` es ahora una raíz privada de la continuación Pratt. Al reconocerla, la
máquina realiza estas transiciones:

1. consume `comprehend (` y agenda el builder como un único child expression;
2. recibe exactamente ese nodo, consume `)` y sólo entonces reserva el ordinal y construye los
   nombres sintéticos;
3. apila un único `ComprehensionContext`;
4. suspende Pratt con `NeedsBlock` y entrega el body al coordinador compartido;
5. recibe el bloque terminado, desapila exactamente su contexto y materializa el AST histórico;
6. completa el slot Pratt original para continuar postfix, binary, conditional o assignment.

No se agregó otra agenda. Los frames de expresión permanecen en `std::deque` y el orden LIFO del
coordinador conserva el contexto activo durante todo el body. El builder se almacena en el work
original y no se vuelve a lexear, parsear, clonar ni evaluar.

La entrada directa `parseComprehensionExpression` pide la raíz explícita al mismo coordinador. La
ruta privada histórica de `parseComprehensionYieldStatement` también quedó reducida a esa fachada;
la implementación productiva vive en `startComprehensionYield` y `finishComprehensionYield`.
Antes de agendar el valor, esa continuación copia los nombres del contexto superior, por lo que una
inserción posterior en el vector no invalida referencias ni cambia el builder de destino.

## Reconocedor amortizado

El constructor del parser crea una tabla de pares de paréntesis mediante una pasada iterativa. Para
cada `(` almacena su `)` correspondiente o el sentinel `tokens_.size()`. El reconocedor contextual
de `comprehend` conserva las mismas condiciones —identificador contextual, `(` balanceado y `{`
inmediatamente después del cierre—, pero cada consulta usa acceso directo.

Los fragmentos de interpolación guardan, reconstruyen y restauran este índice junto al índice de
preguntas condicionales. De ese modo el cache siempre corresponde al span de tokens activo.

Complejidad:

- construcción del índice: tiempo O(T), espacio O(T);
- consulta contextual: O(1);
- builder/body/yield: una transición constante por frontera más el costo lineal de sus hijos;
- profundidad lógica: almacenada en worklists/deques, no en la pila nativa productiva.

## Contratos preservados

- El nodo raíz sigue siendo `LoopExpression` con `text=$__comprehension_loop_N` y
  `auxiliaryText=comprehension:explicit-builder`.
- Su primer nodo sigue siendo un `BlockStatement` sintético con exactamente tres hijos: declaración
  del builder, body original y `BreakStatement` final.
- La declaración conserva `local:comprehension-builder`; el resultado conserva
  `comprehension-result`.
- `yield return value;` y la variante histórica `yield value;` producen exactamente una llamada
  `builder.Add(value)` marcada `comprehension-builder-add`.
- `yield break;` produce el break al label vigente y mueve el builder vigente.
- Fuera de una comprehension, `yield return` conserva `YieldReturnStatement`.
- Los contextos anidados son LIFO y cada yield captura inmediatamente los nombres del contexto
  superior.
- Un comprehension dentro del builder recibe un ordinal menor que su contenedor; los ordinales no
  se reutilizan después de recovery.
- Un body faltante conserva el primer diagnóstico exacto
  `expected '{' after comprehension builder`, fabrica el bloque con `keywordRange`, retira sólo el
  contexto interno y conserva el contexto exterior.
- El builder aparece una sola vez en el AST y el body permanece como hijo completo, sin aplanarse.

## Prueba focal TP-PAR-571

`TP-PAR-571` pertenece a `CP-PAR-033`, componente `Host.Parser`, perfil `NIGHTLY`. Un solo selector
ejecuta cuatro probes aislados:

1. golden: forma exacta, builder con efecto una vez, `yield return`, `yield break`, yield ordinario
   externo y control `for`/`if` con la variante histórica `yield value`;
2. LIFO y ordinales: comprehension en el builder, comprehension anidado en el body, tres builders
   distintos, yields ligados al contexto correcto y pila vacía al terminar;
3. profundidad: 32,768 enlaces `comprehend -> body -> statement -> comprehend`, recorrido iterativo,
   terminal Add, EOF, cero diagnósticos y limpieza de contextos;
4. recovery: entrada directa sin `{`, primer diagnóstico exacto, preservación del contexto exterior,
   ordinal no reutilizado y construcción válida posterior.

La infraestructura quedó registrada en CMake, declaración del registro, `main`, manifest, diseño,
allocation y plan de Host.Parser. El runner enlaza los fuentes reales del parser; no duplica el
algoritmo productivo.

## Evidencia

- Target único: PASS `nsc_component_tests`.
- Primer selector focal: FAIL sólo en el probe profundo porque el fixture dejaba un `;` después de
  la expresión exterior; no fue un fallo del parser.
- Corrección focal: el fixture conserva `;` únicamente para las expresiones que son statements de
  un body y cierra la raíz sin delimitador sobrante.
- Selector único final: PASS `TP-PAR-571`, `duration_us=531949`, cero failures.
- Registro de fuentes: PASS `validate_source_registration`.
- Catálogo canónico: PASS, 6,026 casos, 30 componentes, 682 contratos y una sola ocurrencia de
  `TP-PAR-571`.
- Whitespace y archivo protegido: auditados sin cambios.
- Logs:
  - `A:\NSC-devel-paralelo\logs\agente-2\R03B-build-nsc_component_tests.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\R03B-TP-PAR-571.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\R03B-build-nsc_component_tests-repeat.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\R03B-TP-PAR-571-repeat.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\R03B-build-nsc_component_tests-final.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\R03B-TP-PAR-571-final.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\R03B-source-registration.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\R03B-canonical-catalog.log`.

## Alcance exacto de validación

La evidencia demuestra compilación del target focal, preservación de los contratos observados por
TP-PAR-571 a profundidad 32,768 y consistencia estática de la infraestructura. No demuestra la
batería amplia del compilador: no se ejecutaron casos históricos, otros selectores, CTest, suite,
minibaseline, baseline, `nsc.exe`, instalación ni consumidores externos. Esa batería queda reservada
para la GUI del usuario al cerrar la tanda completa.

## Criterio de cierre

R03B queda listo para integración cuando el commit contiene sólo este ítem, TP-PAR-571 pasa, el
catálogo contiene una sola fila por registro, las rutas productivas de comprehension y yield no
reentran de forma síncrona a expression/block, el archivo protegido permanece intacto y el patch se
publica únicamente bajo `A:\NSC-devel-paralelo\handoff\agente-2`.
