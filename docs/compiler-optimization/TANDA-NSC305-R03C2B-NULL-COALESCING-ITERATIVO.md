# R03C2B - null-coalescing iterativo con `range` intacto

## SCOPE LOCK

- **Objetivo autorizado:** eliminar la autorrecursión de `parseNullCoalescingExpression` y conservar asociación derecha.
- **Naturaleza:** refactor algorítmico interno del parser, sin cambios de lenguaje.
- **Revisión correcta conocida:** `b56fcfe50d81`, cierre R03C2A.
- **Símbolo productivo modificable:** `Parser::parseNullCoalescingExpression`.
- **Infraestructura modificable:** registro completo del contrafactual TP-PAR-553.
- **Protegido:** `parseRangeExpression`, R03C2C, sintaxis, AST público, diagnósticos, recovery, ABI y NSMETA.
- **Prohibido:** baseline, suite completa, cambios LLVM/MIR/HIR y modificaciones al test unitario ajeno.
- **Terminación:** costo O(N), stack nativo O(1), AST asociativo a derecha idéntico y TP-PAR-553 registrado.

## Causa y contrato preservado

La implementación anterior consumía un operando `range` y se llamaba recursivamente por cada `??`.
La profundidad del stack nativo era O(N), aunque el AST requerido es asociativo a derecha. El cambio acumula
operandos y rangos de operador de izquierda a derecha y luego reduce en orden LIFO. Cada nodo conserva:

- `AstNodeKind::NullCoalescingExpression`;
- `TokenKind::QuestionQuestion`;
- el mismo hijo izquierdo y derecho;
- el mismo `SourceRange`, incluido el fallback al rango del operador cuando el operando derecho es nulo;
- el mismo cursor, precedencia, diagnóstico y recovery producido por `parseRangeExpression`.

`parseRangeExpression` no se modifica: delimitadores `..`/`..=`, endpoints omitidos, `step`, validación
constante y diagnósticos `InvalidIndex`, `ExpectedExpression` e `InvalidOperand` permanecen en su ruta canónica.

## Algoritmo

1. Parsear el primer operando mediante `parseRangeExpression`.
2. Por cada `??`, guardar el operando y el rango del operador, y parsear el siguiente `range`.
3. Reducir los operadores desde el último al primero para reconstruir exactamente la asociación derecha.

Complejidad: O(N) tiempo, O(N) memoria auxiliar y O(1) stack nativo para una cadena de N operadores.

## Contrafactual TP-PAR-553

El caso NIGHTLY ejecuta tres procesos aislados:

1. cadena de 32,768 operadores `??`, validada iterativamente hasta el terminal;
2. asociación derecha y forma exacta de un árbol de tres operandos;
3. precedencia y forma de `range`, endpoint inicial omitido y diagnósticos de inclusive-end/step ausentes.

## Estado de validación

- Build `nsc`: PASS. Log: `A:\temp\r03c2b-build-nsc.log`.
- Targets `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS. Log: `A:\temp\r03c2b-build-suite.log`.
- TP-PAR-553: PASS, incluidos los tres procesos aislados y la profundidad 32,768. Log:
  `A:\temp\r03c2b-tp-par-553.log`.
- Host.Parser: PASS 553/553. Log: `A:\temp\r03c2b-host-parser.log`.
- TP-PAR-430/431/443/444/478/481/395: PASS 7/7. Log:
  `A:\temp\r03c2b-parser-regressions.log`.
- CTest `nsc.ns3.t09.lazy-ranges`: PASS. CTest `nsc.ns3.t01.null-aware-operators`: PASS al
  proveer el runtime focal generado en A; el primer intento fue `Not Run` hasta construir sus targets y el
  siguiente expuso únicamente que el proceso heredaba el runtime global de C. Logs:
  `A:\temp\r03c2b-ctest-null-range.log`, `A:\temp\r03c2b-build-language-targets.log`,
  `A:\temp\r03c2b-ctest-null-range-rerun.log` y `A:\temp\r03c2b-ctest-null-aware-rerun.log`.
- `validate_source_registration.py`: PASS.
- CanonicalCatalog: PASS, 6,008 casos, 30 componentes y 682 contratos; TP-PAR-553 presente.
- `parseRangeExpression`: texto idéntico a la revisión base.
- Autorrecursión directa de `parseNullCoalescingExpression`: cero.
- Umbral: función productiva 35 líneas físicas; máximo del módulo TP-PAR-553, 28 líneas físicas.
- Registro de módulo único, declaración/llamada/definición consistentes y `git diff --check`: PASS.
