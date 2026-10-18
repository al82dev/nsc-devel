# R01I — Extracción física de `AttributeParser`

## Alcance

R01I separa del monolito del parser las superficies que reconocen o aplican atributos y
modificadores de declaración. Es un movimiento físico y una subdivisión interna: no agrega
sintaxis, no cambia el AST observable y no modifica diagnósticos, recovery, ABI ni NSMETA.

Las superficies trasladadas son:

- prefijos y atributos de declaraciones globales (`repr`, `pack`, `align`, `size`, visibilidad,
  exportación, política de inline y modificadores de tipo);
- despacho de la declaración después de sus prefijos y aplicación ordenada de sus marcadores;
- `[FrameStorage(...)]` en namespace y miembros;
- layout de tipos anidados y offsets explícitos de campos;
- desambiguación entre atributo y lista de captura en lambdas.

`Parser.cpp`, `DeclarationParser.cpp`, `MemberParser.cpp` y `ExpressionParser.cpp` conservan las
fachadas y los puntos de llamada. `AttributeParser.cpp` contiene 23 definiciones cohesionadas.
No se detectaron definiciones duplicadas dentro del módulo ni contra los demás módulos Parser.

## Contratos preservados

- El orden de consumo de prefijos globales es idéntico al histórico. Los helpers se consultan en
  el mismo orden y repiten la iteración hasta que ninguno consume un token.
- Se mantienen lookahead contextual, rollback de `index_`, rangos, severidad, texto y orden de
  diagnósticos, así como `synchronizeGlobal` y los retornos anticipados.
- Un bloque `extern C` sigue retornando directamente el nodo del bloque; un delegate `extern C`
  sigue atravesando la cola común que aplica accesibilidad.
- Los marcadores de `auxiliaryText` conservan delimitadores y orden, incluidos `async`,
  `framestorage`, `visibility`, `copy`, layout y `repr`.
- Los layouts anidados, `at(N)` y `[FrameStorage(...)]` conservan sus validaciones y recovery.
- La lambda conserva la distinción contextual entre `[Attr]` y capturas `copy`/`move`/`ref`.

`parseGlobalDeclaration` queda como una fachada acotada. Los prefijos se dividen por familia y el
despacho en reference/value/extern/fallback. `clang-tidy` con
`readability-function-size.StatementThreshold=60` no reporta funciones R01I por encima del
umbral; el único aviso pertenece al `diagnosticIdName` preexistente de `DiagnosticId.h`.

## Infraestructura

- Producción registra `src/Parser/AttributeParser.cpp` exactamente una vez en `nsc_frontend`.
- `nsc_component_tests` no compila esa fuente directamente: la incluye exactamente una vez desde
  `ParserAttributeProductTestShim.cpp`, bajo la misma vista privada de los demás módulos Parser.
- Los targets, `add_test`, registro de módulos, ruta default, catálogo, manifest y Builder ya
  existentes ejercen estos casos; R01I no crea ni renombra pruebas ni modifica expectativas.
- `validate_source_registration.py` termina PASS.
- `CanonicalCatalog.load()` termina PASS con 5,995 casos, 30 componentes y 682 contratos.

## Validación focal

Builds sobre el árbol final:

- `nsc` en `build-test-infrastructure`: PASS;
- `nsc` exacto configurado por compiler-suite en `build-clang`: PASS;
- `nsc_component_tests`, `nsc_component_tests_destructive` y
  `nsc_component_tests_source_extended`: PASS.

Casos Host.Parser reutilizados:

- `TP-PAR-128..143`: prefijos, atributos, despacho y recovery global;
- `TP-PAR-159`: layout de agregado;
- `TP-PAR-201`: layout anidado;
- `TP-PAR-207`: offset explícito de campo;
- `TP-PAR-209`: layout de agregado anónimo.

El conjunto dirigido termina 20/20 PASS. Los smokes productivos `TL-LANG-327` (atributo de lambda)
y `TL-LANG-437` (`FrameStorage(Heap)`) terminan 2/2 PASS y ejecutan con exit 42.

La primera corrida focal detectó una regresión local de la subdivisión en `TP-PAR-139`: el helper
booleano de `extern C` convertía el puntero del bloque a `bool` sin copiarlo al parámetro de salida.
El arreglo causal asigna el nodo a `declaration` antes de retornar `true`. Después de reconstruir,
los 20 casos y ambos smokes pasan; no se relajó ningún oracle.

## Evidencia persistente

- `A:\temp\r01i-build-targets.log`;
- `A:\temp\r01i-build-nsc-final.log`;
- `A:\temp\r01i-build-host-final.log`;
- `A:\temp\r01i-build-suite-nsc.log`;
- `A:\temp\r01i-host-parser-focused-final.log`;
- `A:\temp\r01i-language-smokes.log`;
- `A:\temp\r01i-clang-tidy.log`.

Esta validación es focal. No se ejecutó baseline ni la suite completa.
