# NSC 3.0.5 — R03C2C2D2: `foreach` en el coordinador compartido

## SCOPE LOCK

Esta tanda migra exclusivamente `foreach` y su cuerpo de bloque a la worklist de
`Parser::ParseContinuationCoordinator`.

Quedan expresamente fuera:

- `try`/`catch`/`finally` y `catch` standalone (R03C2C2D3);
- `using` (R03C2C2D4);
- `defer` (R03C2C2D5);
- `switch`, `match` y conditional patterns (R03C2C2E);
- las familias especiales y el cierre integral del SCC (R03C2C2F);
- `loop`, `comprehend` y las capas Pratt R03A/R03B/R03C3–C5;
- sintaxis, ABI, NSMETA, diagnósticos nuevos o cambios de recovery.

No se modifica `tests/unit/ProjectMethodBodyModelTests.cpp`.

## Causa y contrato

Después de R03C2C2D1, `foreach` todavía se clasificaba como control `Direct`. El parser entraba a
la ruta histórica, que analizaba dos veces el rango de la colección —el AST ejecutable y un AST
distinto usado sólo como probe semántico— y después volvía a la fachada para el bloque. En una
cadena profunda, esa reentrada conservaba profundidad de stack nativo aunque el coordinador
compartido ya dispusiera de slots y frames heap-backed.

El contrato preservado es:

- mismo AST `ForeachStatement`, mismos candidatos array/range/enumerator y mismo lowering;
- una sola evaluación ejecutable de la colección y un probe estructural distinto que nunca se
  baja ni ejecuta;
- mismo cursor: el probe se analiza desde el inicio de la colección y después se restaura el fin
  alcanzado por la colección real antes de consumir `)`;
- mismos bindings value, `ref`, `ref mut`, `raii`, tipo explícito, `var` y deconstruction pattern;
- mismos nombres temporales y ordinal global monotónico, incluso con `foreach` anidados;
- mismos rangos, recovery y diagnósticos, incluido el cuerpo obligatorio entre llaves;
- mismo orden de construcción y misma selección semántica posterior del candidato.

## Diseño aplicado

El coordinador reconoce `KwForeach` antes del fallback `Direct`. Un `ForeachFrame` estable conserva
`ForeachParseState`, slot de resultado y fronteras de tokens de la colección. La transición se
divide en tres continuaciones explícitas:

1. `FinishForeachCollection` recibe el AST ejecutable, guarda el cursor final y rebobina al inicio;
2. `FinishForeachProbe` recibe el segundo AST, restaura el cursor real, valida `)` y programa el
   bloque;
3. `FinishForeachBody` entrega el bloque al `lowerForeachStatement` canónico.

El encabezado histórico se subdividió en inicialización, prefijo de binding y elemento. La ruta
antigua conserva esos helpers y el mismo doble parse para los consumidores todavía no migrados.
La nueva ruta no abre una fachada ni un `drain` anidado: cada colección y cada cuerpo se publica en
un `ResultSlot`. El recorrido queda O(N) en tokens/nodos y O(N) heap para trabajo pendiente, con
stack nativo O(1) en la familia cubierta.

## Contrafactual permanente

`TP-PAR-557` se registra como caso `Host.Parser`/`CP-PAR-034`, perfil `NIGHTLY`, y ejecuta cuatro
probes aislados:

1. cadena de 32,768 `foreach` con comprobación iterativa del candidato y del cuerpo;
2. colección con lambda-block, AST real/probe distintos, rangos válidos y cursor final EOF;
3. bindings value, `ref`, `ref mut`, `raii`, tipo explícito y deconstruction, además de ordinales
   temporales monotónicos;
4. colección malformada con diagnóstico y recuperación que conserva el statement siguiente.

La infraestructura comprende fuente del módulo, CMake de compiler-suite, declaración en
`TestRegistry.h`, registro default en `main.cpp`, design CSV, allocation CSV y manifest JSONL.

## Evidencia de cierre

- Reconfiguración de producto y compiler-suite: PASS. Logs:
  `A:\temp\r03c2c2d2-configure-nsc.log` y
  `A:\temp\r03c2c2d2-configure-suite.log`.
- Build de `nsc`: PASS, 21/21 pasos. Log:
  `A:\temp\r03c2c2d2-build-nsc.log`.
- Build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS, 296/296 pasos. Log:
  `A:\temp\r03c2c2d2-build-suite.log`.
- `TP-PAR-557`: PASS, un caso con cuatro probes aislados, incluida profundidad 32,768. Log:
  `A:\temp\r03c2c2d2-tp-par-557.log`.
- Regresión dirigida TP-PAR-395..410 y TP-PAR-556: PASS 17/17. Log:
  `A:\temp\r03c2c2d2-parser-regressions.log`.
- Host.Parser completo: PASS 557/557. Log:
  `A:\temp\r03c2c2d2-host-parser.log`.
- CTest `nsc.compiler-suite.host-parser-controller`: PASS 1/1 en 29.03 segundos. Log:
  `A:\temp\r03c2c2d2-ctest-host-parser.log`.
- TL-LANG-391/440/441/443: PASS 4/4, respectivamente 398,900 / 308,241 / 317,790 /
  742,463 microsegundos. El PATH se reconstruyó sólo para esos procesos desde Machine+User y
  `where.exe nslink.exe` resolvió exactamente `A:\nslink-devel\bin\release\nslink.exe`. Logs:
  `A:\temp\r03c2c2d2-nslink-path.log` y `A:\temp\r03c2c2d2-tl-lang.log`.
- `validate_source_registration.py`: PASS. Log:
  `A:\temp\r03c2c2d2-source-registration.log`.
- `CanonicalCatalog.load()`: PASS, 6,012 casos, 30 componentes y 682 contratos;
  `TP-PAR-557` está presente. Log:
  `A:\temp\r03c2c2d2-canonical-catalog.log`.
- Auditoría de umbral: todos los helpers productivos nuevos o modificados quedan por debajo de 60
  líneas físicas —un límite más estricto que 60 ejecutables—; el máximo es
  `parseForeachBindingPrefix`, con 52 líneas físicas. Los probes nuevos también quedan por debajo
  de 60; el máximo tiene 38 líneas físicas. No se trasladó un monolito.
- Auditoría de símbolos e infraestructura: cada helper tiene una definición; los helpers Parser
  poseen una declaración; el módulo tiene una declaración, una definición y un registro default;
  la fuente aparece una vez en CMake; existe una fila en design, allocation y manifest.
- La ruta nueva programa `Expression`/`Block` por worklist y no contiene autorrecursión, llamadas a
  `parseForeachStatement`, `parseStatement`, `parseBlock` ni `drain` anidados.
- `git diff --check`: PASS, con sólo avisos esperados de normalización LF/CRLF.

La validación ejecutada fue más amplia que la política puntual posteriormente reiterada; no se
repitió ninguna prueba al recibir esa instrucción. No se ejecutó baseline, suite completa, deploy
ni install.
