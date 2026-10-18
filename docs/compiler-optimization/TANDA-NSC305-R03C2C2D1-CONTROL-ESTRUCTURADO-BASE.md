# NSC 3.0.5 — R03C2C2D1: control estructurado base en el coordinador compartido

## SCOPE LOCK

Esta tanda migra exclusivamente `if`, `while`, `do` y `for` ordinarios, junto con los cuerpos
embedded que pertenecen a `if`, a la worklist de `Parser::ParseContinuationCoordinator`.

Quedan expresamente fuera:

- `foreach` (R03C2C2D2);
- `try`/`catch`/`finally` y `catch` standalone (R03C2C2D3);
- `using` (R03C2C2D4);
- `defer` (R03C2C2D5);
- `switch`, `match` y conditional patterns (R03C2C2E);
- las familias especiales y el cierre integral del SCC (R03C2C2F);
- `loop`, `comprehend` y las capas Pratt R03A/R03B/R03C3–C5;
- sintaxis, ABI, NSMETA, diagnósticos nuevos o cambios de recovery.

No se modifica `tests/unit/ProjectMethodBodyModelTests.cpp`.

## Causa y contrato

Antes de esta hoja el coordinador clasificaba toda la familia de control como `Direct`. Al llegar
a un `if`, `while`, `do` o `for`, ejecutaba `parseStatementImpl`, que creaba un
`StructuredControlParser`. Sus headers y cuerpos volvían a las fachadas `parseExpression`,
`parseStatement` o `parseBlock`; con un coordinador activo esas fachadas abrían otro `parse` y otro
`drain` sobre la misma worklist. La estructura heap-backed de R03C2C2A existía, pero esta familia
todavía conservaba la reentrada nativa.

El contrato preservado es:

- mismo AST y mismos slots `first`/`second`/`third`/`children`;
- mismo orden de evaluación y de consumo del cursor;
- condition declarations de `if` y `while`, incluida la forma lowered histórica;
- `else` unido al `if` abierto más cercano;
- `while`, `do` y `for` mantienen el requisito histórico de cuerpo `{ ... }`;
- `do` conserva `while (condition);` y su diagnóstico de punto y coma;
- `for` conserva initializer, condition e increment, listas separadas por coma y rangos;
- los embedded statements siguen envolviéndose en `BlockStatement` y las declaraciones sin llaves
  siguen emitiendo el diagnóstico histórico;
- recuperación finita y primer diagnóstico sin reinterpretar la gramática.

## Diseño aplicado

`ParseContinuationCoordinator` clasifica las cuatro formas ordinarias antes del fallback `Direct`.
Cada header posee un frame estable en `std::deque`; cada expresión, declaración o body publica en
un `ResultSlot`; y cada transición se representa con un `WorkKind` explícito. Las declaraciones de
condición reutilizan la máquina iterativa de declaradores con un policy `expectSemicolon=false`.
Las listas de expresiones de `for` usan un frame lineal que consume un elemento por paso y mantiene
la forma `BlockStatement` histórica únicamente cuando existe una coma.

No se llama a una fachada Parser ni se abre un `drain` anidado desde las rutas D1. La construcción
es O(N) en tokens/nodos visitados y O(N) heap en la profundidad pendiente, con stack nativo O(1).

## Contrafactual permanente

`TP-PAR-556` se registra como caso `Host.Parser`/`CP-PAR-028`, perfil `NIGHTLY`, y ejecuta cuatro
probes aislados:

1. cadena mixta de 32,768 controles `if`/`while`/`do`/`for`;
2. condition declaration y dangling else;
3. contratos `while`/`do`/`for`, incluidas listas del header;
4. recovery malformado de las tres familias de loop.

La infraestructura comprende fuente del módulo, CMake de compiler-suite, declaración en
`TestRegistry.h`, registro default en `main.cpp`, design CSV, allocation CSV y manifest JSONL.

## Evidencia de cierre

- Reconfiguración de producto y compiler-suite: PASS. Logs:
  `A:\temp\r03c2c2d1-configure-nsc.log` y
  `A:\temp\r03c2c2d1-configure-suite.log`.
- Build de `nsc`: PASS, siete pasos. Log:
  `A:\temp\r03c2c2d1-build-nsc.log`.
- Build de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS, 293/293 pasos. Log:
  `A:\temp\r03c2c2d1-build-suite.log`.
- `TP-PAR-556`: PASS, un caso con cuatro probes aislados, incluida la cadena mixta de
  profundidad 32,768. Log: `A:\temp\r03c2c2d1-tp-par-556.log`.
- Regresión dirigida TP-PAR-297/300/333/334/336/337/338/339/345/346/347/348:
  PASS 12/12. Log: `A:\temp\r03c2c2d1-parser-regressions.log`.
- Host.Parser completo: PASS 556/556. Log:
  `A:\temp\r03c2c2d1-host-parser.log`.
- TL-LANG-300: PASS, 131,597 microsegundos. El PATH se reconstruyó sólo para el
  proceso desde Machine+User y `where.exe nslink.exe` resolvió exactamente
  `A:\nslink-devel\bin\release\nslink.exe`. Log:
  `A:\temp\r03c2c2d1-tl-lang-300.log`.
- CTest `nsc.compiler-suite.host-parser-controller`: PASS 1/1 en 28.36 segundos.
  Log: `A:\temp\r03c2c2d1-ctest-host-parser.log`.
- `validate_source_registration.py`: PASS. Log:
  `A:\temp\r03c2c2d1-source-registration.log`.
- `CanonicalCatalog.load()`: PASS, 6,011 casos, 30 componentes y 682 contratos;
  `TP-PAR-556` está presente. Log:
  `A:\temp\r03c2c2d1-canonical-catalog.log`.
- La fila de manifiesto de TP-PAR-556 quedó alineada al dominio canónico
  `in-process-reset`; el valor preliminar no canónico fue rechazado por el propio CTest antes de
  ejecutar el parser y se corrigió sin cambiar producto, fixture ni expectativa.
- Auditoría de umbral: todas las funciones nuevas o modificadas quedan por debajo de 60 líneas
  ejecutables; el máximo tocado es `execute`, con 35 líneas ejecutables y 42 físicas. No se
  trasladó un monolito.
- Auditoría de símbolos e infraestructura: una definición, una declaración y un registro default
  para el módulo; una fuente en CMake; una fila en design, allocation y manifest; no existen
  llamadas D1 a fachadas Parser ni `drain` anidados.
- `git diff --check`: PASS, con sólo avisos esperados de normalización LF/CRLF.

No se ejecuta baseline, suite completa, deploy ni install.
