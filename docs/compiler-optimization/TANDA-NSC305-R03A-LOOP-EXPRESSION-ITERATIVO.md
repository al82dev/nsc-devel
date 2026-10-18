# NSC 3.0.5 — R03A: `loop` expression iterativo

## SCOPE LOCK

- Objetivo: retirar exclusivamente la arista recursiva `loop expression -> block -> statement ->
  expression` y ejecutarla sobre la agenda explícita compartida del parser.
- Revisión base: `4624cbfb315f2ef151e1746c57dd609096810102`.
- Dependencias presentes en la base: R03C5C, R03C4 y R03C3, incluido el Pratt unificado para
  assignment, conditional, range, binary, unary, postfix y primary.
- Archivos productivos autorizados: `Parser.h`, `ExpressionParser.cpp` y
  `ParserContinuationCoordinator.cpp`.
- Contratos protegidos: gramática, reconocimiento contextual, AST, body, label, rangos, cursor,
  diagnósticos, recovery, API, ABI y NSMETA.
- Casos históricos protegidos: `TP-PAR-385` y `TP-PAR-386`; no se modificaron.
- Fronteras laterales protegidas: `comprehend` pertenece a R03B; HIR, MIR, driver, LLVM y metadata
  quedan fuera de esta hoja.
- Validación autorizada: compilar sólo `nsc_component_tests` y ejecutar sólo `TP-PAR-570`.
- Prohibido: otros casos, Host.Parser completo, CTest, suite, baseline, deploy, instalación, red,
  cambios en `development` o en `tests/unit/ProjectMethodBodyModelTests.cpp`.

## Causa

El reconocimiento de `loop` ya ocurría dentro de la máquina Pratt, pero la transición productiva
salía de la worklist: `PostfixIterationMachine::startReservedPrimary` llamaba de forma síncrona a
`Parser::parseLoopExpression`; ésta llamaba a `parseBlock`; el bloque podía programar un statement
que volvía a pedir una expresión. Cada `loop { loop { ... } }` agregaba por ello otra cadena de
marcos C++ antes de completar el body exterior.

La profundidad lógica era válida para el lenguaje, pero la implementación la representaba con la
pila nativa. El problema no estaba en el AST ni en la gramática de `loop`, sino en la ausencia de
una continuación capaz de suspender Pratt mientras el coordinador compartido procesaba el bloque.

## Contrato preservado

La forma contextual continúa reconociéndose únicamente cuando el token actual es `loop` y le sigue:

- `{`; o
- `Identifier {`.

El identificador sólo se consume como label en la segunda forma. Una entrada directa malformada,
como `loop value;`, conserva el primer diagnóstico exacto
`expected '{' after loop expression`, progresa hasta EOF y devuelve un `LoopExpression` de recovery
con body `BlockStatement`.

El nodo final conserva:

- kind `LoopExpression`;
- body en `first`;
- label en `text`;
- rango producido por `tokenRangeFrom(beginIndex)` después de terminar el body.

No se cambió ninguna palabra clave, forma sintáctica, layout de AST, ABI ni metadata.

## Diseño de la continuación

`PostfixIterationMachine` implementa ahora la interfaz privada `ExpressionContinuation`. La interfaz
expone únicamente dos operaciones internas:

1. `advance(completedBlock)`, que drena trabajos Pratt hasta completar la expresión o solicitar un
   bloque;
2. `result()`, que entrega el nodo una vez completado su slot raíz.

La máquina conserva su worklist Pratt y, al reconocer `loop`, realiza una de dos transiciones:

- en la fachada no resumible heredada, mantiene el fallback acotado para callers privados que aún
  no forman parte del coordinador;
- en la ruta productiva coordinada, guarda el trabajo `Loop`, marca `NeedsBlock` y devuelve el
  control sin llamar a `parseBlock`.

`ParseContinuationCoordinator` mantiene el propietario estable de cada continuación en un
`ExpressionFrame`. Cuando Pratt solicita un bloque, el coordinador agenda en orden LIFO:

1. `Block`;
2. todos los `Statement` que contiene;
3. sus expresiones, cada una con su propia continuación Pratt;
4. `ResumeExpression` para entregar el bloque concluido al `loop` suspendido.

Al reanudar, `finishLoopBody` construye exactamente un nodo `LoopExpression`, completa el slot
pendiente y Pratt continúa desde el mismo punto: postfix, binary, conditional, assignment o la
frontera que haya contenido el `loop`. Por eso la forma queda disponible en toda posición de
expresión sin agregar caminos especiales por contexto.

La entrada privada directa `parseLoopExpression` también usa `ContinuationRequestKind::LoopExpression`
y fuerza la raíz `Loop` de la misma máquina. De ese modo el recovery histórico no depende de que el
reconocedor contextual haya aceptado antes la entrada.

## Complejidad e invariantes

- Cada token de los bodies se consume una vez por la gramática correspondiente.
- Cada transición completa un slot, consume entrada o agrega una cantidad constante de trabajo.
- La profundidad `loop -> block -> statement -> expression` se almacena en deques y vectores del
  coordinador; no crece la pila nativa con la profundidad del programa.
- Los punteros a frames permanecen estables porque sus propietarios viven en `std::deque`.
- Una continuación completada libera inmediatamente su máquina Pratt; el frame conserva sólo el
  slot estable necesario para la contabilidad del coordinador.
- No hay estado mutable global, threads nuevos ni paralelismo dentro del parser.

## Contrafactual puntual

`TP-PAR-570` (`CP-PAR-033`, `Host.Parser`, `NIGHTLY`) ejecuta tres probes aislados bajo un único
caso seleccionable:

1. posiciones: verifica `loop` como expression statement, initializer, RHS de assignment,
   argumento de call y ambas ramas conditional; valida además label, bodies, rangos, EOF y cero
   diagnósticos;
2. profundidad: construye 32,768 enlaces `LoopExpression -> BlockStatement -> ExpressionStatement`,
   recorre el árbol de manera iterativa y exige terminal call, rangos válidos, EOF y cero
   diagnósticos;
3. recovery: invoca directamente `parseLoopExpression` con `loop value;` y exige kind, body, label
   vacío, rango, progreso, EOF y el primer diagnóstico exacto histórico.

La infraestructura registra el módulo en CMake, `TestRegistry`, `main`, manifest, diseño y
allocation. `TP-PAR-385` y `TP-PAR-386` permanecen intactos como cobertura histórica.

El caso se ejecuta en `nsc_component_tests` porque la autorización de esta hoja limita la
compilación a ese único target. El runner enlaza los archivos productivos reales del parser; no
duplica el algoritmo. No se reconstruyó ni ejecutó `nsc.exe`, por lo que la evidencia no se presenta
como prueba externa del compilador completo.

## Evidencia

- Target único final: PASS `nsc_component_tests`.
- Primer build focal: detectó exclusivamente una declaración adelantada ausente, `<cassert>` ausente
  y el campo `beginIndex` ausente en el nuevo work item; se corrigieron esas causas locales.
- Rebuild focal: PASS, `102/102`, enlace de `nsc_component_tests.exe` completado.
- Selector único: PASS `--case TP-PAR-570`, `duration_us=303937`, cero failures.
- Registro de fuentes: PASS `validate_source_registration`.
- Catálogo canónico: PASS, 6,025 casos, 30 componentes, 682 contratos y una sola ocurrencia de
  `TP-PAR-570`.
- Logs:
  - `A:\NSC-devel-paralelo\logs\agente-2\r03a-build.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03a-build-retry.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03a-tp-par-570.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03a-source-registration.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03a-canonical-catalog.log`.

## Alcance exacto de la validación

La evidencia demuestra que el target focal compila, que TP-PAR-570 conserva los contratos que
observa y que su infraestructura está registrada de manera consistente. No se ejecutaron otros
casos, CTest, suite, baseline, instalación, `nsc.exe` ni consumidores externos. La batería amplia
queda reservada para el cierre global solicitado por el usuario.

## Criterio de cierre

R03A queda listo para integración cuando el target exacto compila, TP-PAR-570 pasa, el catálogo
contiene una sola fila por registro, `parseLoopExpression` no llama síncronamente a `parseBlock`, el
archivo protegido permanece intacto y el commit no incluye R03B, ABI, NSMETA o tracking global.
