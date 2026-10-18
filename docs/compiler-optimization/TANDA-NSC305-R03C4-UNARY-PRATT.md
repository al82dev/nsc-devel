# NSC 3.0.5 — R03C4: unary Pratt iterativo

## SCOPE LOCK

- Objetivo: migrar exclusivamente las familias unary y allocation propiedad de R03C4 a la
  worklist Pratt compartida de R03C5A/R03C5B.
- Revisión base original del WIP: `dfd1864f821acfa397fa3ec6ed3b636d2318f4c6`.
- Dependencia integrada antes del cierre: R03C5C desde
  `06ea4a692d89ca1b85e2a4823aff0923814b96b8` (`4b9cc7888` en el clon aislado).
- Archivos productivos autorizados: `Parser.h` y `ExpressionParser.cpp`.
- Contratos protegidos: AST, precedencia, cursor, rangos, recovery, diagnósticos, evaluación
  única, API, ABI y NSMETA.
- Fronteras protegidas: binary pertenece a R03C3; `loop` y `comprehend` pertenecen a R03A/R03B;
  HIR, MIR, driver y metadata quedan fuera de esta hoja.
- Validación autorizada: compilar sólo `nsc_component_tests` y ejecutar sólo `TP-PAR-566`.
- Prohibido: otros casos, Host.Parser completo, CTest, TL, minisuite, baseline, deploy,
  instalación, red o cambios en `development`.

## Causa y diseño

La ruta anterior acumulaba únicamente prefijos ordinarios homogéneos. Las combinaciones de
`await`, `^`, casts parentetizados, `delete` y `destroy` volvían a entrar en
`parseUnaryExpression`; `move`, named casts, `sizeof`/`alignof` en forma expresión,
`stackalloc` y los hijos de `new` abrían además otro parseo completo. La profundidad válida del
lenguaje podía crecer la pila nativa hasta provocar stack overflow.

`PostfixIterationMachine` incorpora trabajos unary en la misma worklist estable que ya gobierna
postfix y primary. Cada prefijo reserva un slot de resultado, apila una continuación y agenda el
operando. Los nodos se reconstruyen LIFO, conservando asociación a la derecha, token, tipo,
flags, texto auxiliar y política histórica de rangos. Las formas delimitadas agendan una
expresión completa sin crear otra máquina ni otro drain.

Las variantes de `new` reutilizan el mismo scheduler para argumentos, storage, `trailing`,
dimensiones e inicializadores. El estado de cada lista vive en el frame: cursor de fase, hijo
pendiente, nodos parciales, rangos omitidos, contador de dimensiones y metadatos de placement.
Cada reanudación consume un resultado o un token, por lo que no existe un ciclo sin progreso.

## Auditoría de equivalencia

- `await` conserva su reconocimiento contextual y el rango histórico limitado al keyword.
- `^` conserva `FromEndExpression`, `Caret` y rango unido al operando.
- Los ocho prefijos ordinarios conservan `UnaryExpression`, token, orden LIFO y rango.
- Los casts parentetizados y con nombre conservan tipo, operando y `cast:<nombre>`.
- `move`, `sizeof`, `alignof` y `stackalloc` conservan delimitadores y kinds.
- `offsetof` y `default(T)` permanecen en sus constructores atómicos previos.
- Object, target-typed, placement y array `new` conservan tipos, flags, orden de hijos,
  initializer, failure profile y resultado `Result<T, AllocationFailure>`.
- `delete[]` y `destroy` conservan operación, flag y operando.
- La rama de recovery de placement sin `(` conserva también el hijo `trailing` y el rango final;
  esta equivalencia fue corregida durante la auditoría del diff parcial.
- Los argumentos `ref`/`out`/`in` de object y placement `new` conservan el inicio del rango en
  el keyword `new`; el frame separa ese rango exterior del rango temporal del argumento.
- Las entradas postfix y unary cierran sus métricas simétricamente; `sizeof(T)` y `alignof(T)`
  no reservan slots de hijo cuando la forma reconocida es de tipo.

No quedan llamadas desde las fachadas unary hacia su implementación recursiva: todas delegan a
una sola entrada iterativa. Las aristas de expresión delimitada se resuelven como trabajos de la
misma máquina. El costo es O(T + F) en tiempo y O(F) en heap, con O(1) pila nativa respecto de la
profundidad unary.

## Contrafactual puntual

`TP-PAR-566` (`CP-PAR-038`, `Host.Parser`, `NIGHTLY`) ejecuta cuatro probes aislados bajo un solo
caso seleccionable:

1. 32,768 casts parentetizados, con verificación iterativa de kind, tipo, cursor y diagnósticos;
2. 32,768 operandos `move(...)`, que prueban que la arista hacia expression no abre otro drain;
3. contratos de flags, tipos, metadata, rangos y orden para unary, casts, size/alignment,
   stackalloc, object `new` con argumento `ref` y placement `new` con `trailing` y
   `failure: result`;
4. recovery de delimitadores ausentes y conservación del hijo trailing en placement incompleto.

La infraestructura registra el módulo en CMake, `TestRegistry`, `main`, manifest, diseño y
allocation. El selector focal es `--case TP-PAR-566`; no autoriza ningún caso adicional.

## Evidencia

- Configuración focal: PASS con `clang-cl`, `lld-link` y `llvm-rc` resueltos por el `PATH`
  nativo; todos los artefactos, temporales, cachés y logs permanecieron en A:.
- Producto seleccionado: `A:\nsc-devel\build-test-infrastructure\bin\nsc.exe`.
- Identidad seleccionada:
  `A:\nsc-devel\build-test-infrastructure\generated\nativesharp\Basic\BuildId.h`.
- Runtime seleccionado:
  `A:\nsc-devel\build-test-infrastructure\generated\NativeSharp.Runtime.Win32.lib`.
- NSLink seleccionado: `A:\nslink-devel\bin\release\nslink.exe`.
- Target único: PASS `nsc_component_tests` (`545/545`, enlace correcto).
- Selector único: PASS `--case TP-PAR-566`; `duration_us=2186529`, cero failures.
- Registro de fuentes: PASS `validate_source_registration`.
- Catálogo canónico: PASS, 6,023 casos, 30 componentes, 682 contratos y una sola
  ocurrencia de `TP-PAR-566`.
- Logs reservados:
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c4-configure.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c4-build.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c4-tp-par-566.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c4-source-registration.log`;
  - `A:\NSC-devel-paralelo\logs\agente-2\r03c4-canonical-catalog.log`.

## Criterio de cierre

R03C4 se cierra cuando el target exacto compila, TP-PAR-566 pasa, el catálogo contiene una sola
fila por registro, el diff no conserva reentrada unary recursiva y el commit aislado no contiene
trabajo de R03C3, R03A, R03B, ABI o NSMETA. La batería amplia queda reservada al usuario.
