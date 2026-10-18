# NSC 3.0.5 — R03C2C2D5: `defer` en el coordinador compartido

## SCOPE LOCK

Esta hoja migra exclusivamente `defer` desde la ruta `Direct` al
`Parser::ParseContinuationCoordinator`. Quedan fuera switch/match/patrones condicionales
(E1–E3), el cierre F, Pratt, HIR, MIR, LLVM, sintaxis, ABI y NSMETA.

No se modifica `appendParsedStatement`, el contrato de lowering de cleanup ni el parser
histórico todavía requerido por familias no migradas. El checkout principal y ambos trackings
permanecen fuera de este commit.

## Contrato preservado

- Gramática: `defer [` capturas explícitas `ref`/`move` `]` bloque obligatorio.
- Diagnósticos, rangos y orden histórico de `expect` permanecen idénticos.
- `this` sólo se captura por referencia; nombres repetidos conservan `DuplicateSymbol`.
- El sentinel de recovery continúa siendo `__invalid_defer_capture`.
- El body conserva `auxiliaryText="defer-capture-boundary"` y los nombres permitidos en `text`.
- `nextDeferTemporary_` avanza exactamente una vez por registro y los nombres ocultos siguen el
  patrón `__nsc_defer_capture_<ordinal>_<name>`.
- Capturas move/ref conservan sus shapes, ownership text y orden de evaluación.
- El registro conserva `defer-registration`; el try generado conserva `defer-lowered`.
- `appendParsedStatement` sigue siendo la única autoridad que avanza `destination` y forma el
  anidamiento LIFO de los statements posteriores.

## Implementación

`StatementKind::Defer` clasifica el keyword contextual antes del fallback `Direct`.
`DeferFrame`, almacenado en `std::deque`, mantiene inicio, rango, slot y capturas. Las etapas
`startDefer`, `parseDeferCaptures`, `parseDeferCapture`, `scheduleDeferBody` y
`finishDeferBody` separan header, recovery y body. El body se solicita exclusivamente como
`WorkKind::Block`, sin abrir otra fachada ni otro `drain`.

El lowering se divide en helpers acotados para configurar la frontera, crear tipos/moves,
materializar cada captura y ensamblar el registro/try. Todas las funciones nuevas permanecen
por debajo de 60 líneas ejecutables.

## Prueba puntual e infraestructura

`TP-PAR-560` (`Host.Parser`, `CP-PAR-035`, `NIGHTLY`) contiene cuatro probes aislados dentro de
un único selector:

1. 32,768 `defer` anidados con recorrido iterativo del AST y ordinal exacto;
2. capturas `move`, `ref`, `ref this`, nombres ocultos monotónicos y dos registros LIFO;
3. frontera statement→block con try/finally dentro del body y cursor posterior;
4. recovery de modificador, nombre, `move this`, duplicado y body ausente.

La cadena se registra una sola vez en módulo, CMake de compiler-suite, `TestRegistry.h`,
registro default, design CSV, allocation CSV y manifest JSONL, quedando visible al catálogo de
Builder 3.

## Evidencia

- Reconfigure aislado de compiler-suite: PASS.
- Build exclusivo `cmake --build A:\NSC-devel-paralelo\builds\agente-2 --target
  nsc_component_tests --parallel 16`: PASS, 287/287 pasos, exit 0.
- Única prueba ejecutada, `nsc_component_tests.exe --case TP-PAR-560`: PASS, exit 0,
  306,286 microsegundos; pasaron los cuatro probes, incluido el contrafactual 32,768.
- Source-registration, CanonicalCatalog 6,015 con TP-PAR-560, símbolos, límite de funciones y
  `git diff --check`: PASS.

Logs persistentes:

- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2d5-configure.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2d5-build-nsc-component-tests.log`
- `A:\NSC-devel-paralelo\logs\agente-2\r03c2c2d5-tp-par-560.log`

No se ejecutarán Host.Parser completo, CTest, TL-LANG, regresiones, baseline, deploy ni
instalación.
