# NSC 3.0.5 · R03C2C2B · bloque y statements de valor simples

Fecha: 2026-10-18
Rama: `development`
Base: `860a4fc06374`
Estado: `CERRADO; VALIDACIÓN FOCAL PASS`

## SCOPE LOCK

### Objetivo autorizado

Extender el coordinador compartido creado por R03C2C2A para que la primera ruta profunda
`expression → lambda-block → block → simple statement → expression` use una worklist iterativa. La hoja cubre
bloques anidados y los statements simples cuyo único hijo recursivo es una expresión: expression statement,
return, throw, break con valor, yield return y static assert. También conserva yield break, yield simple,
continue, labels y `must_tail return` dentro del mismo dispatcher.

### Naturaleza

Refactor interno no funcional y optimización algorítmica del parser. No cambia la gramática ni la superficie
pública.

### Revisión conocida y diferencial

- Base inmediata: `860a4fc06374`, R03C2C2A.
- Último contrato aplicable: las fachadas `parseExpression`, `parseBlock` y `parseStatement` ya delegan en un
  coordinador compartido, pero el coordinador todavía invoca recursivamente sus implementaciones directas.
- Diferencial permitido: coordinador, detector de lambda con cuerpo block, prueba TP-PAR-555, infraestructura y
  este expediente.

### Comportamiento que debe permanecer idéntico

- AST, orden de hijos, flags de lambda/must-tail/reference return, labels y texto de static assert.
- Cursor, rangos, recovery, diagnósticos y orden de consumo de tokens.
- Lowering temprano de `defer`, incluida su reescritura LIFO mediante el destino mutable del bloque.
- Precedencia del dispatcher: scope, control, yield contextual, static local, jump, keyword, declaration y
  expression statement.
- API, ABI, NSMETA, sintaxis, semántica y artefactos emitidos.

### Archivos productivos autorizados

- `include/nativesharp/Parser/Parser.h`
- `src/Parser/ExpressionParser.cpp`
- `src/Parser/ParserContinuationCoordinator.cpp`

### Infraestructura, prueba y documentación autorizadas

- `tests/compiler-suite/host/modules/ParserLambdaBlockContinuationCounterfactualModule.cpp`
- `tests/compiler-suite/CMakeLists.txt`
- `tests/compiler-suite/host/include/nsc_suite/TestRegistry.h`
- `tests/compiler-suite/host/src/main.cpp`
- `docs/compiler-testing-reengineering/inventory/test-case-design.csv`
- `docs/compiler-testing-reengineering/inventory/suite-case-allocation.csv`
- `tests/compiler-suite/manifest/cases.jsonl`
- este expediente.

### Archivos y superficies protegidos

- `tests/unit/ProjectMethodBodyModelTests.cpp`, trabajo ajeno.
- Cambios F02A y cualquier archivo MIR, LLVM, Driver, ABI o NSMETA.
- R03C2C2C–F, R03A, R03B y R03C3–C5.
- Fixtures, expectativas y diagnósticos existentes.

### Acciones prohibidas en esta ventana

- No compilar ni ejecutar pruebas mientras F02A utiliza el productor compartido.
- No usar el índice ni crear commit.
- No cerrar tracking ni reducir el conteo pendiente.
- No migrar declaraciones, control estructurado, match/switch, defer/try/using, comprehension-yield o inline asm.
- No ejecutar baseline ni suite completa.

### Criterio de terminación de la hoja

La hoja sólo podrá cerrarse después de compilar `nsc` y los tres targets suite, ejecutar TP-PAR-555 y la
regresión focal acordada, pasar source registration, catálogo, umbral de funciones, símbolos y diff-check, y
registrar evidencia y hash. Esta versión del expediente no afirma esos gates.

## Problema causal

R03C2C2A compartió una instancia coordinadora entre las tres fachadas, pero cada request seguía llamando de
forma síncrona a `parseExpressionImpl`, `parseBlockImpl` o `parseStatementImpl`. Una entrada de esta forma:

```text
handler0 => { return handler1 => { return ... handler32767 => { return terminal; }; ... }; }
```

forma la SCC:

```text
parse expression
  → parse lambda head
  → parse block
  → parse return/expression statement
  → parse expression
  → parse lambda head
  → ...
```

El vector de continuaciones de expresiones no eliminaba esa recursión porque el cuerpo block abandonaba la
máquina de expresiones y entraba sincrónicamente en el parser de statements. El coordinador tampoco resolvía el
problema mientras cada request se drenara mediante otra llamada C++ anidada.

## Diseño implementado

### Worklist única

`ParseContinuationCoordinator` conserva resultados y frames en `std::deque`, cuyas direcciones permanecen
estables mientras crecen. Una pila explícita de `Work` contiene estados para:

- iniciar una expresión;
- iniciar, continuar y terminar un bloque;
- iniciar y anexar un statement;
- terminar lambda-block, expression statement, return, throw, jump, yield return y static assert.

Cada dependencia crea un `ResultSlot`. El productor se apila después de su continuación para que el orden LIFO
ejecute primero al hijo y después ensamble el padre. Ninguna ruta B llama recursivamente a `parse()`.

### Lambda-block

`lambdaBlockBodyStartsHere` es un lookahead sin efectos laterales. Primero exige que el cursor ya satisfaga el
detector canónico de lambda; luego localiza la flecha gruesa fuera de paréntesis y corchetes y confirma que el
siguiente token sea `{`. Sólo esa forma se extrae del `ExpressionIterationMachine`; la lambda de cuerpo
expresión conserva su implementación existente.

El coordinador consume el head mediante `parseLambdaExpressionHead`, agenda un bloque como hijo y finaliza con
`finishLambdaExpression(lambda, body, true)`. Así conserva parámetros, genéricos, captures, atributos, efectos,
calling convention, tipo de retorno, flag de block-body y range total.

### Bloques

El frame de bloque conserva:

- índice inicial;
- índice anterior para la guarda de progreso;
- slot del resultado;
- nodo block original;
- destino mutable utilizado por el lowering de defer.

`ContinueBlock` termina ante `}` o EOF, o agenda exactamente un statement y su posterior append. El append usa
el helper productivo `appendParsedStatement`, por lo que mantiene la expansión inline de conditional patterns y
el cambio de destino que produce defer. Si el statement no consume tokens, conserva la guarda que consume uno
para garantizar progreso.

### Dispatcher de statements

La clasificación no consume tokens y reproduce la precedencia productiva. Las familias todavía no migradas se
delegan completas a `parseStatementImpl`; no se reinterpretan ni se parten parcialmente.

Rutas B iterativas:

- nested block;
- expression statement;
- return y `must_tail return`, incluidos retornos `ref`;
- throw con o sin valor;
- break con label y/o valor, y continue con label;
- yield return, yield break y yield simple fuera de comprehension;
- static assert con mensaje opcional.

Fallback protegido para C–F:

- static locals, deconstruction, await-expression statements, funciones y variables locales;
- if, while, do, for, foreach, switch, match y conditional patterns;
- scopes fp/unsafe, inline asm;
- defer, try, using y comprehension-yield.

## Contratos preservados por construcción

- Expression statement crea primero el nodo con el range de la expresión, consume `;` y luego extiende el range.
- Return, throw y jump consumen `;` antes de crear su nodo, como las implementaciones canónicas.
- Yield crea el nodo y su range antes de consumir `;`, también como la implementación canónica.
- Static assert conserva mensaje, diagnósticos y range posterior a `);`.
- `must_tail` inválido conserva el diagnóstico exacto, sincronización y placeholder block; el caso válido conserva
  el range iniciado en `return` y marca `flag=true`.
- `yield` dentro de una comprehension no entra en B y permanece en `parseComprehensionYieldStatement`.

## Complejidad

Para una cadena B de `T` tokens y profundidad `N`:

- tiempo: `O(T)`;
- espacio heap explícito: `O(N)` en work items, frames, slots y AST;
- stack nativo atribuible a la SCC B: `O(1)`.

Las rutas C–F todavía pueden reentrar mediante su implementación histórica. Este documento no declara cerrada
la SCC completa expression/block/statement.

## TP-PAR-555

El caso se registra como `Host.Parser`, contrato `CP-PAR-036`, perfil `NIGHTLY`, y ejecuta cuatro probes en
procesos aislados:

1. 32,768 lambda-blocks enlazadas mediante `return`.
2. 32,768 lambda-blocks enlazadas mediante expression statements.
3. Forma exacta de nested block, return ref, throw, jump labels/values, yield y static assert.
4. Recovery por `;` y `}` ausentes, y frontera shallow de defer sin migrar su familia.

Los oráculos recorren el AST iterativamente, comprueban kinds, hijos, flags, labels, mensajes, rangos y ausencia
de diagnósticos en entradas válidas. Los probes de recovery exigen diagnóstico y terminación finita.

## Infraestructura completa

- Fuente del módulo incluida en `tests/compiler-suite/CMakeLists.txt`.
- Declaración incluida en `TestRegistry.h`.
- Registro invocado desde `host/src/main.cpp`.
- Diseño incluido en `test-case-design.csv`.
- Asignación incluida en `suite-case-allocation.csv`.
- Caso canónico incluido en `cases.jsonl`.
- Descubrimiento esperado: `CanonicalCatalog` y Builder mediante el manifiesto canónico.

## Validación

### Evidencia ejecutada

- Reconfiguración del producto: PASS. Log:
  `A:\temp\r03c2c2b-configure-nsc.log`.
- Reconfiguración de compiler-suite: PASS; el cache conserva literalmente
  `NSC_NSLINK_EXECUTABLE:STRING=nslink.exe`. Log:
  `A:\temp\r03c2c2b-configure-suite.log`.
- Build `nsc`: PASS, incluido el contrato de host runtime estático. Log:
  `A:\temp\r03c2c2b-build-nsc.log`.
- Build `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS 541/541. Log:
  `A:\temp\r03c2c2b-build-suite.log`.
- TP-PAR-555 aislado: PASS; sus cuatro procesos cubren ambas cadenas de 32,768,
  contratos simples y recovery/defer. Log: `A:\temp\r03c2c2b-tp-par-555.log`.
- Host.Parser completo: PASS 555/555. Log: `A:\temp\r03c2c2b-host-parser.log`.
- Regresión dirigida de fachadas y statements: PASS 22/22, incluidos TP-PAR-015/016/017,
  297/298/300/314, 411/423/426/427, 445/446/449/450/452/455/456 y 551–554. Log:
  `A:\temp\r03c2c2b-parser-regressions.log`.
- TL-LANG-314/316/317/318/320/321/327: PASS 7/7. El PATH se renovó sólo para el proceso
  desde Machine+User y `where.exe nslink.exe` resolvió exactamente
  `A:\nslink-devel\bin\release\nslink.exe`. Log:
  `A:\temp\r03c2c2b-language-lambda.log`.
- `validate_source_registration.py`: PASS.
- `CanonicalCatalog`: PASS 6,010 casos, 30 componentes y 682 contratos; TP-PAR-555 aparece
  exactamente una vez.
- Paridad de infraestructura: una fuente CMake y tres referencias del símbolo de registro
  (declaración, definición e invocación).
- Umbral: ninguna función nueva o modificada excede 60 líneas físicas; el máximo observado es
  32 líneas.
- `git diff --check`: PASS; sólo aparecen avisos esperados de normalización LF/CRLF.

El primer build detectó únicamente inicializadores parciales de `Work` rechazados por `/WX`.
Se corrigió en una sola frontera mediante el constructor `Work(kind, resultSlot)`, que conserva
los valores de clase para el resto del frame; el build repetido quedó PASS.

Conclusión proporcional: R03C2C2B queda validado y cerrado. La afirmación de stack O(1) se limita
a las rutas B enumeradas; R03C2C2C–F y el cierre integral de la SCC permanecen pendientes.
