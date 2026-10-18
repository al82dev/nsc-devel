# NSC 3.0.5 — R03C5B: primary Pratt iterativo

## SCOPE LOCK

- Objetivo: migrar exclusivamente las familias primary propiedad de R03C5B a la worklist Pratt
  compartida introducida por R03C5A.
- Revisión base: `b013554cdc3e80eb46f7e81676f810d8b95d7dcd`.
- Archivos productivos autorizados: `Parser.h` y `ExpressionParser.cpp`.
- Contratos protegidos: AST, precedencia, cursor, rangos, recovery, diagnósticos, metadata,
  evaluación única, ABI y NSMETA.
- Fronteras protegidas: `match` pertenece a E2; `loop` y `comprehend` pertenecen a R03A/R03B;
  R03C3, R03C4 y R03C5C permanecen pendientes.
- Validación autorizada: compilar sólo `nsc_component_tests` y ejecutar sólo `TP-PAR-568`.
- Prohibido: ejecutar otros casos, Host completo, CTest, TL, minisuite, baseline, deploy,
  instalación o cualquier operación de red.

## Diseño implementado

`PostfixIterationMachine` incorpora trabajos primary para grouping/tuple, collection/spread,
`nameof`, políticas de overflow, tokens atómicos e interpolación. Cada hijo expression reserva un
slot estable y vuelve a la misma worklist; grouping, tuple y collection dejan de abrir una cadena
de fachadas Parser por nivel.

La interpolación conserva una pila de frames en heap. Cada hueco guarda el estado exterior del
parser, instala temporalmente los tokens del fragmento, programa el hijo en la misma máquina y
restaura cursor, índice de `?` condicional y deuda de `>` genérico antes de ensamblar el resultado.
Una interpolación anidada añade otro frame, pero no otro drain ni otro Parser nativo.

El lookahead de `nameof` conserva la ruta atómica anterior para operandos de tipo genérico
calificado. El resto suspende su operando como expression. Las políticas `checked`, `saturating`,
`unchecked` y `wrapping` hacen lo mismo. Los átomos conservan el clasificador y constructor
canónicos existentes.

Complejidad de las aristas propiedad de C5B: O(T) tiempo, O(N) heap para continuaciones y O(1)
stack nativo. Todos los helpers nuevos o modificados quedan en 60 líneas físicas o menos.

## Límites expresos

- `match` conserva la implementación cerrada por E2 y no se modifica en esta hoja.
- `loop` y `comprehend` conservan sus fachadas hasta R03A y R03B.
- C5B no cierra la migración integral de binary/unary ni el cierre conjunto C5C.
- La ruta genérica calificada de `nameof` conserva el parser canónico para no alterar su contrato
  de tipos; no se presenta como deuda oculta de primary ordinario.

## Contrafactual puntual

`TP-PAR-568` (`CP-PAR-040`, `Host.Parser`, `NIGHTLY`) contiene cuatro probes aislados:

- 32,768 niveles mixtos de grouping, tuple, collection, spread, `nameof` y `checked`;
- contratos de AST y metadata para tuple, collection, `nameof` genérico y átomo calificado;
- un hueco interpolado con 32,768 grouping y una interpolación anidada;
- recovery de delimitadores ausentes y token primary inesperado.

La infraestructura incluye fuente, CMake de compiler-suite, `TestRegistry`, `main`, manifest,
design y allocation, por lo que el Builder puede descubrir el caso desde el catálogo canónico.

## Evidencia

- Target único: `nsc_component_tests` — PASS, enlace completado en 117 pasos en la repetición
  incremental después de corregir exclusivamente un include faltante del fixture.
- Selector único: `--case TP-PAR-568` — PASS, 27,535,305 us, cero fallos.
- `validate_source_registration.py` — PASS.
- `CanonicalCatalog.load()` — PASS: 6,021 casos, 30 componentes, 682 contratos y una sola
  aparición de `TP-PAR-568`.
- NSLink resolvió por nombre exactamente a `A:\nslink-devel\bin\release\nslink.exe` mediante el
  PATH nativo reconstruido para el proceso.
- Logs:
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c5b-configure.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c5b-build.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c5b-build-retry.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c5b-tp-par-568.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c5b-source-registration.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c5b-canonical-catalog.log`.

No se ejecutaron otras pruebas, Host.Parser completo, CTest, TL, minisuite, baseline, deploy ni
instalación.

## Criterio de cierre

C5B queda cerrado porque todas las familias primary autorizadas suspenden sus hijos expression en
la worklist compartida, el contrafactual mixto de 32,768 niveles termina sin stack overflow, la
interpolación anidada no abre otro drain y las fronteras E2, R03A/B, C3, C4 y C5C permanecen
explícitamente fuera de esta hoja.
