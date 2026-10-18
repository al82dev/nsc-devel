# NSC 3.0.5 - R01E: extraccion fisica del parser de statements

## Proposito

R01E separa fisicamente de `Parser.cpp` la familia que reconoce y construye statements. El
producto conserva `Parser` como fachada y mantiene sin cambios observables la gramatica, el AST,
el cursor, el recovery, los diagnosticos, ABI, NSMETA y los contratos congelados de 3.0.4.

La nueva unidad es `src/Parser/StatementParser.cpp`. `Parser.cpp` conserva las entradas privadas
`parseBlock()` y `parseStatement()`, que delegan en `parseBlockImpl()` y `parseStatementImpl()`.
Esta tanda no agrega sintaxis ni migra recorridos a algoritmos iterativos; esas responsabilidades
permanecen en R02/R03.

## SCOPE LOCK

- Objetivo: extraer fisicamente el parser de statements mediante movimiento puro y subdivision
  interna de los cuerpos que excedian el limite de la campana.
- Naturaleza: refactor no funcional.
- Revision de trabajo: rama `development`, despues de R01D y del hotfix focal S37F02.
- Archivos funcionales autorizados: `Parser.h`, `Parser.cpp`, `StatementParser.cpp`, CMake raiz y
  shim privado de compiler-suite.
- Comportamiento protegido: tokens consumidos, precedencia, recovery, rangos, diagnosticos, AST,
  API, ABI, NSMETA y artefactos emitidos.
- Pruebas protegidas: todos los casos Host.Parser existentes; no se modifican para hacer pasar el
  refactor.
- Acciones prohibidas: baseline, suite completa, sintaxis nueva, correcciones semanticas, cambios
  HIR/MIR, cambios de metadata y cambios en consumidores.
- Archivo ajeno protegido: `tests/unit/ProjectMethodBodyModelTests.cpp`.
- Criterio: ambos targets focales compilan, los 113 casos dirigidos pasan, la infraestructura
  queda completa, no hay definiciones duplicadas y ninguna funcion R01E supera 60 statements.

## Inventario extraido

La unidad contiene 68 definiciones unicas de `Parser`. Se agrupan en:

1. bloques y dispatch de statements;
2. scopes de floating point y `unsafe`;
3. `yield`, suspension, saltos y expression statements;
4. `static_assert` e inline assembly;
5. declaraciones locales, deconstruction y aggregate initializers;
6. control estructurado y continuaciones de `if`, `while`, `do`, `for`, `switch` y `match`;
7. lowering sintactico de patterns condicionales;
8. `foreach` sobre rangos, arrays, spans y protocolo enumerator;
9. `return`, `defer`, `try`, `catch`, `throw` y `using`.

Los monolitos encontrados no se trasladaron intactos. `parseStatementImpl`, inline assembly,
declaraciones locales, el dispatcher de control estructurado y el lowering de `foreach` quedaron
divididos en helpers cohesionados. `clang-tidy` con `readability-function-size` no reporta cuerpos
de `StatementParser.cpp` por encima del umbral de 60 statements.

## Conservacion algoritmica

La extraccion conserva:

- el orden exacto del dispatch por token y keyword contextual;
- la forma en que un bloque registra y anida `defer` en orden LIFO;
- las fronteras de recovery y avance de cursor ante statements incompletos;
- los nombres, constraints, operands, clobbers y placeholders de inline assembly;
- modifiers y flags de locals, deconstruction y aggregate initializers;
- la topologia AST de control estructurado, `match` y cleanup;
- la evaluacion unica de la fuente de `foreach` y el orden de sus candidates;
- los temporales monotonicamente generados para `match`, comprehensions, `defer` y `foreach`;
- identificadores, severidad, texto y rango de diagnosticos.

El cambio no modifica expression parsing, pattern parsing ni type parsing. Las llamadas entre esas
familias conservan la misma interfaz privada de `Parser` y el mismo estado compartido.

## Frontera de compilacion

### Producto

- `src/Parser/StatementParser.cpp` aparece exactamente una vez en `nsc_frontend`.
- `Parser.cpp` conserva las fachadas `parseBlock()` y `parseStatement()`.
- No se agrega un target nuevo.

### Compiler suite

- El CMake de compiler-suite no compila `StatementParser.cpp` directamente.
- `ParserProductTestShim.cpp` incluye `StatementParser.cpp` una vez dentro de la vista privada que
  ya integra `Parser.cpp`, `ParserContext.cpp`, `DeclarationParser.cpp`, `MemberParser.cpp` y
  `TypeParser.cpp`.
- `validate_source_registration.py` reconoce la fuente productiva y la inclusion del shim.

R01E no agrega una prueba, fixture, caso, target ni `add_test`; por eso no requiere una fila nueva
en design, allocation, manifest, catalogo o Builder. Reutiliza la infraestructura y cobertura
canonica existentes sin modificaciones.

## Cobertura focal existente

Se ejecutaron 113 casos exactos:

- bloques, statements, declarations, aggregates, inline assembly y control:
  `TP-PAR-297..348`;
- `match`, `loop`, comprehensions, `foreach` y cleanup: `TP-PAR-369..426`;
- monotonicidad y restauracion de estado de `match`, `defer` y comprehension:
  `TP-PAR-527..529`.

El resultado fue 113/113 PASS, exit 0. La regresion focal de identidad de constructor estatico
tambien permanece cubierta por TL-LANG-295 y Stage37; su causa y commit independiente pertenecen
al expediente S37F02, no al movimiento R01E.

## Evidencia

- Build `nsc_frontend`: PASS, exit 0, 7.419 s.
- Build y enlace `nsc_component_tests`: PASS, exit 0, 2.079 s.
- Host.Parser dirigido: 113/113 PASS, exit 0, 6.202 s.
- `validate_source_registration.py`: PASS, exit 0.
- `CanonicalCatalog`: 5,995 casos, 30 componentes y 682 contratos.
- `clang-tidy readability-function-size`: cero avisos en `StatementParser.cpp` con threshold 60.
- Definiciones: 68 en `StatementParser.cpp`, 68 nombres unicos y cero duplicados con
  `Parser.cpp`.
- Registro: raiz directo una vez, shim una vez y CMake de suite directo cero veces.
- Lineas nuevas por encima de 120 caracteres: cero.
- `git diff --check`: PASS.

Logs persistentes:

- `A:\temp\r01e-nsc-frontend-build-final.log`;
- `A:\temp\r01e-nsc-component-tests-build-final.log`;
- `A:\temp\r01e-host-parser-113.log`;
- `A:\temp\r01e-validate-source-registration.log`;
- `A:\temp\r01e-canonical-catalog.log`;
- `A:\temp\r01e-function-size-after-range-split.log`.

Evidencia focal de la regresion aislada S37F02:

- `A:\temp\s37f02-tl-lang-295.log`;
- `A:\temp\s37f02-stage37-direct.log`;
- `A:\temp\s37f02-ctest-stage37.log`.

No se ejecuto baseline ni suite completa.

## Criterio de cierre

R01E queda listo para integracion cuando el indice contiene exclusivamente sus seis archivos, el
archivo ajeno `ProjectMethodBodyModelTests.cpp` no entra al commit, los registros producto/shim
aparecen una sola vez y la evidencia anterior permanece verde. El tracking se reduce de 61 a 60
hojas solo despues del commit local.
