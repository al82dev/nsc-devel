# R03C2C1 - trampoline assignment/conditional directo

## SCOPE LOCK

- **Objetivo autorizado:** eliminar la SCC recursiva directa entre assignment y conditional.
- **Naturaleza:** refactor algorítmico interno del parser, sin cambios de lenguaje.
- **Revisión correcta conocida:** `ac6cc43df7d6`, cierre GATE-F01.
- **Símbolos productivos modificables:** `parseAssignmentExpression`,
  `parseConditionalExpression` y el estado privado que comparten.
- **Infraestructura modificable:** registro completo del contrafactual TP-PAR-554.
- **Protegido:** lambda con cuerpo bloque, niveles binarios/unarios/postfix, sintaxis, AST público,
  diagnósticos, recovery, ABI y NSMETA.
- **Prohibido:** cerrar R03C2C2, baseline, suite completa y cambios LLVM/MIR/HIR.
- **Terminación:** costo O(N), stack nativo O(1) en ramas conditional true/false y cadenas
  assignment/conditional, con TP-PAR-554 registrado.

## Causa

`parseAssignmentExpression` ya usaba continuaciones para assignment y lambdas de cuerpo
expresión, pero delegaba en `parseConditionalExpression`. Este último volvía a entrar al nivel
assignment tanto por la rama situada entre `?` y `:` como por la rama false. Una cadena profunda
era lineal en tiempo, pero mantenía una SCC con O(N) frames del stack nativo.

La SCC independiente de lambda con cuerpo bloque se conserva deliberadamente: esa ruta llama
`parseBlock`, cuyos statements pueden volver a entrar a expresiones. Su migración pertenece a
R03C2C2 y no se oculta dentro de esta hoja.

## Diseño iterativo

`Parser::ExpressionIterationMachine` mantiene una worklist con tres trabajos: assignment,
conditional y entrega de resultado. Sus continuaciones representan:

1. inspección de cola assignment después de producir el operando izquierdo;
2. materialización de assignment después de producir el derecho;
3. cierre de lambda de cuerpo expresión;
4. transición de la rama true al consumo obligatorio de `:`;
5. materialización conditional después de producir la rama false.

El orden de consumo se conserva: primero null-coalescing, después `?`, rama true como expresión
completa, `expect(':')` y rama false como assignment. Los nodos conservan kind, token, hijos,
flag de referencia y rangos. La frontera lambda-block sigue llamando al `parseBlock` canónico.

Complejidad: O(N) tiempo, O(N) memoria auxiliar y O(1) stack nativo para las aristas directas
assignment/conditional cubiertas por R03C2C1.

## Contrafactual TP-PAR-554

El caso NIGHTLY ejecuta cuatro procesos aislados:

1. 32,768 conditional anidados por la rama false;
2. 32,768 conditional anidados por la rama true;
3. 32,768 pares assignment/conditional enlazados por la rama false;
4. contrato de assignment `ref`, lambda de cuerpo expresión, frontera lambda-block y recovery
   finito ante rama true o rama false ausentes. La clasificación de un `?` sin `:` pertenece al
   índice contractual ya cubierto por TP-PAR-552 y no se reinterpreta en esta hoja.

## Estado de validación

- Build `nsc`: PASS. Log: `A:\temp\r03c2c1-build-nsc.log`.
- Targets `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS. Log: `A:\temp\r03c2c1-build-suite.log`.
- TP-PAR-554: PASS, incluidos cuatro procesos aislados y profundidad 32,768. Log:
  `A:\temp\r03c2c1-tp-par-554.log`.
- Host.Parser: PASS 554/554. Log: `A:\temp\r03c2c1-host-parser.log`.
- TP-PAR-427…456 y TP-PAR-551…553: PASS 33/33. Log:
  `A:\temp\r03c2c1-parser-regressions.log`.
- CTest `nsc.ns3.t01.null-aware-operators` y `nsc.ns3.t09.lazy-ranges`: PASS 2/2. Log:
  `A:\temp\r03c2c1-ctest-null-range.log`.
- TL-LANG-314/316/317/318/320/321/327: PASS 7/7 con `nsc` construido en A y `nslink.exe`
  resuelto por nombre a `A:\nslink-devel\bin\release\nslink.exe`. Log:
  `A:\temp\r03c2c1-language-lambda.log`.
- `validate_source_registration.py`: PASS.
- CanonicalCatalog: PASS, 6,009 casos, 30 componentes y 682 contratos; TP-PAR-554 presente.
- Cero referencias al estado anterior `AssignmentContinuation`; los wrappers productivos no se
  llaman entre sí y la única ruta `parseBlock` permanece en la frontera protegida R03C2C2.
- Umbral: funciones productivas entre 4 y 25 líneas físicas; funciones del módulo TP-PAR-554
  entre 9 y 48 líneas físicas.
- Registro de módulo único, declaración/llamada/definición consistentes y
  `git diff --check`: PASS; sólo se informan avisos preexistentes de normalización LF/CRLF.
