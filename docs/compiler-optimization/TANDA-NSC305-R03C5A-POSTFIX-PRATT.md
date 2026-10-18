# NSC 3.0.5 — R03C5A: postfix Pratt iterativo

## Alcance cerrado

Esta tanda migra exclusivamente la familia postfix de expresiones a una worklist con resultados
por slot. `call`, `index`, `?[` y `with` suspenden cada hijo assignment y lo reanudan en la misma
máquina; ningún handler de esas familias llama a `parseArgument`, `parseIndexArgument`,
`parseCallSuffix`, `parseIndexSuffix`, `parseWithExpression` ni abre otro drain.

El cambio preserva la forma pública del AST, precedencia, asociación, rangos, cursor, recovery,
diagnósticos, metadata de argumentos, ABI y NSMETA. También conserva la validación constante de
rangos mediante `language::lazyRangeCount`.

## Diseño implementado

- `Parser::PostfixIterationMachine` mantiene una pila de trabajos y slots estables en heap.
- El puente Pratt mínimo cubre assignment, conditional, null-coalescing, range, binary y unary
  sólo para que los hijos propios de postfix no reingresen a fachadas recursivas.
- Calls procesan un argumento a la vez y conservan nombre, `ref`/`out`/`in`, orden y marca builtin.
- Índices conservan receiver, alias `second == children[0]`, rangos abiertos y orden fuente.
- `with` conserva source en `first`, nombre de miembro, trailing comma, rangos y diagnósticos.
- Miembros, arrow, null-conditional, postfix update y result propagation siguen siendo pasos
  atómicos y regresan al loop postfix.
- Todas las funciones nuevas o modificadas de la máquina quedan acotadas a 60 líneas físicas o
  menos; los handlers se separan por start/resume/finish.

Complejidad de las aristas propiedad de C5A: O(T) tiempo, O(N) heap para continuaciones pendientes
y O(1) stack nativo.

## Límites expresos y deuda preservada

Esta tanda no cierra ni marca R03C3, R03C4 o R03C5B:

- R03C3 conserva su migración integral de precedencia binaria fuera de los hijos postfix.
- R03C4 conserva las familias unary complejas. `move`, casts, `offsetof`, `sizeof`/`alignof`,
  `default`, `new`, `delete`, `destroy` y `stackalloc` delegan temporalmente en sus parsers
  canónicos; C5A no cambia su semántica.
- R03C5B conserva la migración de primary y sus reentradas internas. La base postfix todavía se
  obtiene mediante `parsePostfixBaseExpression`, que mantiene el lookahead genérico existente.
- No se afirma el cierre integrado/recovery de R03C5C.

El bridge no absorbe los casos propios de esas hojas: existe sólo para garantizar que call/index/
with nunca creen una cadena de stack nativo al analizar sus hijos assignment.

## Contrafactual puntual

Se añadió únicamente `TP-PAR-567` (`CP-PAR-039`, `Host.Parser`, `NIGHTLY`) con cuatro probes
aislados:

- 32,768 calls anidados con expresiones binarias mixtas `f(1 + g(...))`;
- 32,768 continuaciones alternadas call/index/range/with/null-conditional;
- metadata de generic calls, named arguments y `ref`/`out`/`in`;
- recovery y progreso finito de delimitadores postfix incompletos.

La infraestructura quedó registrada en CMake de compiler-suite, `TestRegistry`, `main`, manifest,
design y allocation. No se creó otro case ID.

## Evidencia ejecutada

- Target único: `nsc_component_tests` — PASS.
- Selector único ejecutado: `--case TP-PAR-567` — PASS, 389,317 us, sin fallos.
- NSLink se resolvió por nombre a `A:\nslink-devel\bin\release\nslink.exe` usando el PATH nativo
  reconstruido para el proceso.
- Logs: `A:\NSC-devel-paralelo\logs\agente-2\r03c5a-final-build.log` y
  `A:\NSC-devel-paralelo\logs\agente-2\r03c5a-tp-par-567-final.log`.
- `validate_source_registration.py`: PASS. Log:
  `A:\NSC-devel-paralelo\logs\agente-2\r03c5a-source-registration.log`.
- `CanonicalCatalog.load()`: PASS, 6,020 casos, 30 componentes y 682 contratos;
  `TP-PAR-567` está presente una vez. Log:
  `A:\NSC-devel-paralelo\logs\agente-2\r03c5a-canonical-catalog.log`.
- Auditoría de umbral: el máximo productivo nuevo es `continueExpression`, con 58 líneas físicas;
  las funciones del módulo focal también quedan por debajo de 60 líneas.

No se ejecutaron Host.Parser completo, CTest, TL-LANG, minisuite, baseline, deploy ni instalación.

## Criterio de cierre

C5A se considera cerrado porque los hijos assignment de call/index/with usan una sola worklist,
el contrafactual profundo termina sin desbordamiento de stack, la infraestructura viaja con el
cambio y los límites C3/C4/C5B permanecen explícitos y pendientes.
