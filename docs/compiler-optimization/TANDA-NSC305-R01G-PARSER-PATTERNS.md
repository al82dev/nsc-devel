# NSC 3.0.5 - R01G: extraccion fisica del parser de patrones

## Proposito

R01G separa fisicamente de `Parser.cpp` la familia que reconoce patrones y construye su
representacion sintactica y su lowering interno. `Parser` permanece como la fachada unica y
conserva el mismo cursor, arena, diagnosticos, contadores monotonicos y contexto de tipos.

La nueva unidad es `src/Parser/PatternParser.cpp`. La tanda no agrega sintaxis, no cambia la
gramatica ni convierte estas rutas a los algoritmos iterativos de R02/R03. Tampoco modifica HIR,
MIR, ABI, NSMETA, runtime, proyectos consumidores, fixtures ni expectativas.

## SCOPE LOCK

- Objetivo: extraer fisicamente el parser de patrones mediante movimiento puro y subdividir los
  cuatro cuerpos que excedian el limite de 60 statements.
- Naturaleza: refactor no funcional.
- Revision de trabajo: rama `development`, despues de R01F y M01F.
- Archivos funcionales autorizados: `Parser.h`, `Parser.cpp`, `PatternParser.cpp`, CMake raiz,
  CMake de compiler-suite y los shims privados del producto Parser.
- Comportamiento protegido: tokens consumidos, precedencia `not`/`and`/`or`, recovery, rangos,
  diagnosticos, topologia AST, temporales monotonicos, evaluacion unica, API, ABI, NSMETA y
  artefactos emitidos.
- Acciones prohibidas: sintaxis nueva, correcciones semanticas, migracion iterativa R02/R03,
  cambios HIR/MIR, cambios de metadata, cambios de pruebas o expectativas, baseline y suite
  completa.
- Archivos ajenos protegidos: la tanda M02 concurrente y
  `tests/unit/ProjectMethodBodyModelTests.cpp`.
- Criterio: producto y tres targets focales de la suite compilables; casos Host.Parser de
  patrones PASS; infraestructura completa; cero definiciones duplicadas; ninguna funcion R01G
  por encima de 60 statements.

## Inventario extraido

La unidad contiene 41 definiciones unicas de `Parser`, agrupadas en cinco responsabilidades:

1. reconocimiento primario: literales, rangos, bindings, listas, tuples, structs, variants y
   patrones relacionales;
2. composicion de precedencia: `not`, `and` y `or`;
3. construccion de condiciones sintacticas para patrones escalares y compuestos;
4. lowering de patrones que requieren temporales, deconstruction o seleccion por alternativas;
5. emision de bindings, referencias, rest views, payloads y guards de brazos.

Los cuatro monolitos originales se dividieron en helpers privados cohesivos:

- `parsePatternPrimary` delega cada familia primaria;
- `makePatternCondition` delega cada clase de condicion compuesta;
- `lowerPattern` despacha a lowerings por forma;
- `emitPatternBindings` despacha a emisores por forma.

`clang-tidy` con `readability-function-size` y umbral de 60 statements no reporta cuerpos de
`PatternParser.cpp`. El unico aviso del comando pertenece al header preexistente
`DiagnosticId.h`.

## Conservacion algoritmica

La extraccion conserva:

- el orden exacto del dispatch y de los lookaheads;
- la asociatividad y precedencia de `not`, `and` y `or`;
- los mismos identificadores, severidad, texto y rango de diagnosticos;
- el mismo recovery y las mismas fronteras de consumo del cursor;
- la evaluacion unica de fuentes que necesitan deconstruction, payload o rest view;
- el orden de declaracion de temporales y bindings;
- los nombres monotonicos derivados de `nextMatchTemporary_`;
- el orden de alternativas y la seleccion del binding del primer brazo;
- la composicion de guards para patrones posicionales;
- la mutabilidad de rest views y referencias de patrones.

La recoleccion recursiva que ya existia dentro de la lambda local se conserva como helper
nombrado para no mezclar una migracion algoritmica con el movimiento fisico. Su sustitucion
iterativa pertenece a R02/R03 y no se presenta como realizada por R01G.

## Frontera de compilacion del producto

- `src/Parser/PatternParser.cpp` aparece exactamente una vez en `nsc_frontend`.
- `Parser.cpp` no conserva ninguna de las 41 definiciones movidas.
- `Parser.h` declara exclusivamente los helpers privados necesarios para subdividir los cuerpos.
- No se agrega una biblioteca ni un target de producto.

## Reparacion completa de la infraestructura Parser

La evidencia de otra configuracion Debug mostro `undefined symbol` masivos para `current`,
`tokenRangeFrom`, `peek`, `consume`, `expect`, synchronizers y declaraciones. La causa comun no
era una ausencia de cuerpos del producto: todos los modulos estaban incluidos como texto dentro
de un unico `ParserProductTestShim.cpp`, pero una build incremental conservaba el objeto anterior
y relinkaba sin materializar los nuevos includes.

La solucion mantiene la vista privada requerida por el ABI Microsoft y convierte cada modulo
fisico extraido en una fuente explicita del target:

- `ParserProductTestShim.cpp` compila solamente `Parser.cpp` y conserva los wrappers de helpers
  anonimos;
- siete shims uno-a-uno compilan `ParserContext.cpp`, `DeclarationParser.cpp`,
  `MemberParser.cpp`, `TypeParser.cpp`, `StatementParser.cpp`, `ExpressionParser.cpp` y
  `PatternParser.cpp`;
- `tests/compiler-suite/CMakeLists.txt` registra exactamente una vez cada shim en
  `nsc_component_tests`;
- ninguna fuente Parser extraida se compila directamente dentro de la suite, porque en ABI
  Microsoft el acceso forma parte del nombre decorado;
- el CMake raiz registra directamente cada fuente productiva exactamente una vez.

Los shims no registran casos y por ello no requieren nuevas filas de design, allocation,
manifest, catalogo o Builder. Se incorporan al target existente `nsc_component_tests`, cuya ruta
default y `add_test` ya permanecen registradas. Los otros dos targets del comando externo,
`nsc_component_tests_destructive` y `nsc_component_tests_source_extended`, no consumen Parser,
pero se construyen en el mismo gate para demostrar que el cambio no degrada su infraestructura.

## Cobertura focal

R01G reutiliza pruebas existentes y no crea ni modifica fixtures o expectativas:

- `TP-PAR-349..368`: continuacion y semantica primaria de patrones;
- `TP-PAR-369..384`: lowering de patrones en `match`;
- `TP-PAR-385..394`: expresiones `loop` y `comprehend` que transfieren control al parser;
- `TP-PAR-395..410`: patrones y bindings de `foreach`;
- `TP-PAR-525..540`: restauracion de estado y regresiones finales del Parser.

El conjunto dirigido contiene 78 casos. No sustituye una suite completa ni un baseline; verifica
unicamente la familia movida y sus fronteras inmediatas.

## Evidencia

- Build `nsc_frontend`: PASS, exit 0.
- Reconfiguracion de `build-compiler-suite-v3/debug`: PASS, exit 0.
- Build exacto de `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS, exit 0.
- Host.Parser dirigido sobre el binario Debug recien enlazado: 78/78 PASS, exit 0.
- Host.Parser dirigido sobre el host focal de infraestructura: 78/78 PASS, exit 0.
- `clang-tidy readability-function-size`: cero avisos en `PatternParser.cpp`, threshold 60.
- Definiciones: 41 en `PatternParser.cpp`, cero duplicados internos y cero duplicados con
  `Parser.cpp`.
- Registro mecanico: ocho fuentes Parser productivas aparecen una vez en CMake raiz, cero veces
  directas en el CMake de suite y una vez mediante su shim; los ocho shims aparecen una vez en
  el target.
- `validate_source_registration.py`: PASS, exit 0.
- `CanonicalCatalog`: 5,995 casos, 30 componentes y 682 contratos.

Logs persistentes:

- `A:\temp\r01g-nsc-frontend-build-coherent.log`;
- `A:\temp\r01g-nsc-component-tests-build.log`;
- `A:\temp\r01g-suite-debug-reconfigure.log`;
- `A:\temp\r01g-suite-debug-three-targets-build.log`;
- `A:\temp\r01g-host-parser-78.log`;
- `A:\temp\r01g-host-parser-78-debug.log`;
- `A:\temp\r01g-function-size-after-split.log`;
- `A:\temp\r01g-symbol-duplication-audit.log`;
- `A:\temp\r01g-parser-source-registration-audit.log`;
- `A:\temp\r01g-validate-source-registration.log`;
- `A:\temp\r01g-canonical-catalog.log`.

No se ejecuto baseline ni suite completa.

## Criterio de cierre

R01G queda listo para integracion cuando el indice contiene exclusivamente su extraccion,
declaraciones, CMake, shims y expediente; `MirLayoutImporter.cpp`, su documento M02 y
`ProjectMethodBodyModelTests.cpp` permanecen fuera. Despues del commit local se registra su hash
en el tracking externo y se reduce exactamente una hoja desde el conteo vigente releido.
