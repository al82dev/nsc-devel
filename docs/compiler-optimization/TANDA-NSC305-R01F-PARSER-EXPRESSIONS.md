# NSC 3.0.5 - R01F: extraccion fisica del parser de expresiones

## Proposito

R01F separa fisicamente de `Parser.cpp` la familia que reconoce y construye expresiones. El
producto conserva `Parser` como fachada y mantiene sin cambios observables la gramatica, la
precedencia, el AST, el cursor, el recovery, los diagnosticos, ABI, NSMETA y los contratos
congelados de 3.0.4.

La nueva unidad es `src/Parser/ExpressionParser.cpp`. `Parser.cpp` conserva la entrada privada
`parseExpression()`, que delega en `parseExpressionImpl()`. Esta tanda no convierte el parser a
Pratt iterativo ni cambia los algoritmos recursivos existentes; esas migraciones pertenecen a
R03. Tampoco absorbe pattern, generic syntax ni attributes, reservados para R01G, R01H y R01I.

## SCOPE LOCK

- Objetivo: extraer fisicamente el parser de expresiones mediante movimiento puro y subdividir
  internamente los cuerpos que excedian el limite de la campana.
- Naturaleza: refactor no funcional.
- Revision de trabajo: rama `development`, despues de R01E.
- Archivos funcionales autorizados: `Parser.h`, `Parser.cpp`, `ExpressionParser.cpp`, CMake raiz
  y shim privado de compiler-suite.
- Comportamiento protegido: tokens consumidos, precedencia y asociatividad, recovery, rangos,
  diagnosticos, topologia AST, temporales monotonicos, API, ABI, NSMETA y artefactos emitidos.
- Acciones prohibidas: sintaxis nueva, correcciones semanticas, migracion Pratt, cambios HIR/MIR,
  cambios de metadata, baseline y suite completa.
- Archivos ajenos protegidos: toda la tanda M01F concurrente y
  `tests/unit/ProjectMethodBodyModelTests.cpp`.
- Criterio: targets focales compilables, casos Host.Parser dirigidos PASS, infraestructura
  completa, cero definiciones duplicadas y ninguna funcion R01F por encima de 60 statements.

## Inventario extraido

La unidad contiene 106 definiciones unicas de `Parser`, agrupadas en:

1. expresiones `match`, `loop` y comprehensions;
2. assignment, conditional, null coalescing y ranges;
3. deteccion, captures, parametros y contratos de lambdas;
4. niveles binarios de precedencia;
5. unary, casts, `sizeof`, `alignof`, `offsetof`, `default`, `new`, placement, `delete`,
   `destroy` y `stackalloc`;
6. argumentos y postfix calls, indices, members, null-conditional y result propagation;
7. `with`, collection expressions, `nameof` y overflow policies;
8. primary expressions, tuples e interpolacion estatica/runtime;
9. helper generico de niveles binarios.

Los cuerpos grandes no se trasladaron como monolitos. Match, lookahead y materializacion de
lambdas, unary/new, postfix y primary/interpolation se dividieron en helpers privados cohesivos.
`clang-tidy` con `readability-function-size` y umbral de 60 statements no reporta cuerpos de
`ExpressionParser.cpp`; el unico aviso restante pertenece al header preexistente
`DiagnosticId.h`.

## Conservacion algoritmica

La extraccion conserva:

- el orden exacto del dispatch por token y keyword contextual;
- la precedencia y asociatividad de assignment, conditional y niveles binarios;
- el lookahead de lambdas, sus captures, atributos, effects y calling convention;
- la evaluacion unica y la cobertura textual de cada brazo de `match`;
- los nombres y contadores monotonicos de match y comprehensions;
- la distincion entre cast parentizado, agrupacion, tuple, target-typed `new` y placement `new`;
- el orden de dimensiones, initializers y opciones de allocation;
- el encadenamiento izquierdo de calls, indexers, members, postfix y result propagation;
- la separacion entre interpolacion estatica y lowering runtime;
- identificadores, severidad, texto y rango de diagnosticos.

El cambio no modifica pattern parsing, type parsing, generic syntax ni statement parsing. Las
llamadas entre familias conservan la misma instancia de `Parser` y el mismo estado compartido.

## Frontera de compilacion

### Producto

- `src/Parser/ExpressionParser.cpp` aparece exactamente una vez en `nsc_frontend`.
- `Parser.cpp` conserva la fachada `parseExpression()`.
- No se agrega un target nuevo.

### Compiler suite

- El CMake de compiler-suite no compila `ExpressionParser.cpp` directamente.
- `ParserProductTestShim.cpp` incluye `ExpressionParser.cpp` exactamente una vez dentro de la
  vista privada del parser productivo.
- El validador de source registration debe reconocer la fuente productiva y su inclusion por el
  shim, igual que las unidades R01A-R01E.

R01F no agrega pruebas, fixtures, casos, targets ni `add_test`. Reutiliza los casos Host.Parser
existentes y por ello no requiere filas nuevas en design, allocation, manifest, catalogo o
Builder.

## Cobertura focal obligatoria

La seleccion dirigida cubre las expresiones que la unidad extraida posee:

- `match`, `loop` y comprehensions: `TP-PAR-369..394`;
- precedencia, lambdas, unary, postfix, primary e interpolacion: `TP-PAR-427..512`;
- robustez y restauracion de estado relevantes: `TP-PAR-525..540`.

El gate externo adicional NS3-T07 NeverType se ejecuta contra el `nsc.exe` reconstruido. La causa
del crash original estaba despues de OOP MIR: el particionador LLVM dereferenciaba un child MIR
`nullptr` valido. El hotfix productivo quedo aislado en `e595f2aff`; R01F reproduce despues el
fixture NeverType exacto fuera del repositorio, sin cache y sin mezclar esa correccion en el
refactor del parser.

## Evidencia

- Build `nsc_frontend`: PASS, exit 0.
- Build y enlace `nsc_component_tests`: PASS, exit 0.
- Host.Parser dirigido: 128/128 PASS, exit 0, 5.371 s.
- TP-PAR-455 aislado: PASS despues de restaurar la semantica original en la que una coma exige
  otra iteracion del parser de parametros y, por tanto, `(x, ) => x` produce diagnostico.
- `clang-tidy readability-function-size`: cero avisos en `ExpressionParser.cpp` con threshold 60.
- Definiciones: 106 en `ExpressionParser.cpp`, 106 nombres unicos y cero duplicados con
  `Parser.cpp`.
- Registro estatico: raiz directo una vez, shim una vez y CMake de suite directo cero veces.
- `validate_source_registration.py`: PASS, exit 0.
- `CanonicalCatalog`: 5,995 casos, 30 componentes y 682 contratos.
- NS3-T07 NeverType directo con `-O0 -j 1 --no-artifact-cache`: compilacion y enlace PASS,
  exit 0; ejecucion PASS con codigo esperado 42.

Logs persistentes disponibles:

- `A:\temp\r01f-nsc-frontend-build-stable.log`;
- `A:\temp\r01f-nsc-component-tests-build-final.log`;
- `A:\temp\r01f-tp-par-455.log`;
- `A:\temp\r01f-host-parser-128-final.log`;
- `A:\temp\r01f-validate-source-registration.log`;
- `A:\temp\r01f-canonical-catalog.log`;
- `A:\temp\r01f-function-size-after-split.log`;
- `A:\temp\r01f-nevertype-no-cache-j1.log`;
- `A:\temp\r01f-nevertype-execution.log`.

No se ejecuta baseline ni suite completa.

## Criterio de cierre

R01F queda listo para integracion cuando el indice contiene exclusivamente sus seis archivos, el
hunk de CMake pertenece unicamente a `ExpressionParser.cpp`, los cambios concurrentes M01F y el
archivo ajeno `ProjectMethodBodyModelTests.cpp` quedan fuera del commit, y el tracking baja de 61
a 60 hojas solo despues del commit local.
