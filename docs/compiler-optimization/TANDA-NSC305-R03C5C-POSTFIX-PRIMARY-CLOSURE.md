# NSC 3.0.5 — R03C5C: cierre integrado postfix/primary

## SCOPE LOCK

- Objetivo: cerrar exclusivamente la integración de R03C5A y R03C5B sobre una sola agenda Pratt.
- Revisión base: `3fd42e99a18f265c498ce54fa82ea1e1bba51ab1`.
- Archivos productivos autorizados: instrumentación focal dentro de `ExpressionParser.cpp`.
- Contratos protegidos: sintaxis, AST, precedencia, cursor, rangos, diagnósticos, recovery, API,
  ABI y NSMETA.
- Fronteras protegidas: unary pertenece a R03C4, binary a R03C3 y `loop`/`comprehend` a R03A/B.
- Validación autorizada: compilar sólo `nsc_component_tests` y ejecutar sólo `TP-PAR-569`.
- Prohibido: ejecutar 567/568, otros casos, Host.Parser, CTest, suite, baseline, instalar,
  desplegar, usar red, tocar main o modificar el WIP C4.

## Estado integrado auditado

R03C5A y R03C5B ya convergen físicamente en `PostfixIterationMachine`. La máquina contiene una
worklist LIFO, slots estables de resultado y frames de interpolación. Los hijos de call, index,
with, grouping, tuple, collection, `nameof`, overflow e interpolación se agendan mediante
`scheduleChild`; ninguno abre otra `PostfixIterationMachine::parse()` dentro de esos handlers.

La ruta de generic receiver usa `parsePostfixBaseExpression` sólo para materializar el tipo ya
reconocido por lookahead; no analiza una expresión hija ni drena otra agenda. Los helpers legacy
de call/index permanecen como fachadas externas, pero la máquina integrada no los invoca.
`match`, `loop`, `comprehend` y unary conservan sus fronteras explícitas para las hojas pendientes;
C5C no las reescribe ni presenta al parser completo como no recursivo.

## Instrumentación focal

`NSC_PARSER_SCHEDULER_TEST_HOOKS` se define únicamente al compilar `nsc_component_tests`. Bajo
esa definición, la máquina registra por thread:

- profundidad activa de `PostfixIterationMachine::parse()`;
- máxima profundidad observada desde el último reset;
- cantidad de result slots creados que no fueron completados al publicar el resultado raíz.

El producto normal no compila campos, contadores ni funciones del hook. No cambia el layout de
`Parser`, la superficie instalada ni el resultado funcional. El contador de slots se actualiza en
los dos puntos canónicos `makeSlot` y `complete`, evitando instrumentar cada handler.

## Invariantes y complejidad

- Durante cada probe integrado, la máxima profundidad activa debe ser exactamente uno.
- Al devolver el resultado raíz, los slots pendientes deben ser cero.
- Cada child slot se crea una vez, se completa una vez y se consume sólo después de estar listo.
- Primary entrega el receiver a postfix; cada suffix vuelve a encolar postfix con el receiver nuevo.
- Los hijos conservan orden fuente, identidad, metadata de named/ref/spread y evaluación única.
- Cada transición consume token, completa slot o agenda un trabajo distinto.
- Las aristas cubiertas conservan O(T) tiempo, O(N) heap y O(1) stack nativo por profundidad.

## Prueba focal TP-PAR-569

El único caso autorizado usa cuatro subprocess probes:

1. `cross-family`: 32,768 niveles alternados de call, collection, overflow, `nameof`, index y
   with. La inspección del AST también es iterativa.
2. `contracts`: generic receiver/call, type arguments, named/ref arguments, conditional `?`,
   propagation `?`, null-conditional member/index y postfix encadenado.
3. `recovery`: delimitadores y miembro incompletos deben conservar el `;` centinela; un primary
   inesperado consume exactamente su token y reporta `ExpectedExpression` una vez.
4. `entry`: las entradas directas externas de postfix y primary crean una sola sesión y terminan
   con cero slots pendientes.

El caso está registrado una vez en CMake, `TestRegistry`, `main`, manifest, diseño y allocation.
NSC Builder lo descubre desde el catálogo canónico sin cambios en Python o GUI.

## Evidencia

- Configuración focal: PASS con `clang-cl`, `lld-link` y `llvm-rc` resueltos por el `PATH`
  nativo; `LLVM_DIR` apunta al toolchain oficial de sólo lectura.
- Producto seleccionado: `A:\nsc-devel\build-test-infrastructure\bin\nsc.exe`.
- Identidad seleccionada:
  `A:\nsc-devel\build-test-infrastructure\generated\nativesharp\Basic\BuildId.h`.
- Runtime seleccionado:
  `A:\nsc-devel\build-test-infrastructure\generated\NativeSharp.Runtime.Win32.lib`.
- NSLink seleccionado: `A:\nslink-devel\bin\release\nslink.exe`.
- Target único: PASS `nsc_component_tests` (`544/544`, enlace correcto).
- Selector único: PASS `--case TP-PAR-569`; `duration_us=1425051`, cero failures.
- Registro de fuentes: PASS `validate_source_registration`.
- Catálogo canónico: PASS, 6,022 casos, 30 componentes, 682 contratos y una sola
  ocurrencia de `TP-PAR-569`.
- Logs reservados:
  - `A:\NSC-devel-paralelo\logs\agente-4\r03c5c-configure.log`;
  - `A:\NSC-devel-paralelo\logs\agente-4\r03c5c-build.log`;
  - `A:\NSC-devel-paralelo\logs\agente-4\r03c5c-tp-par-569.log`;
  - `A:\NSC-devel-paralelo\logs\agente-4\r03c5c-source-registration.log`;
  - `A:\NSC-devel-paralelo\logs\agente-4\r03c5c-canonical-catalog.log`.

## Criterio de cierre

C5C queda listo para integración cuando el target exacto compila, TP-PAR-569 pasa con máxima
profundidad uno y cero slots pendientes, cada registro es único y el commit no incluye unary,
binary, loop, comprehend, ABI, NSMETA ni trabajo ajeno. La batería amplia queda para la GUI del
usuario después de cerrar todas las hojas.
