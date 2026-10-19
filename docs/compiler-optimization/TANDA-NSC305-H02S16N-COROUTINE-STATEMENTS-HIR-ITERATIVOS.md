# NSC 3.0.5 — H02S16N · statements coroutine HIR iterativos

## SCOPE LOCK

Esta hoja migra exclusivamente la arista recursiva de
`YieldReturnStatement` con valor a la worklist HIR compartida y conserva como
hojas `SuspendStatement`, `YieldBreakStatement` y el `yield return` sin hijo.
No cambia sintaxis, semántica, diagnósticos, ABI, NSMETA, MIR ni LLVM.

## Contrato preservado

- `YieldValue` conserva tipo `builtin:void`, estado inicial cero y un único
  hijo cuando existe valor.
- Las regiones de cleanup activas se congelan antes de bajar el valor y se
  conservan en orden.
- La continuación de `YieldValue` permanece `Return`.
- `Iterator<ref T>` y `Iterator<ref mut T>` bajan el operando en contexto de
  dirección; los iteradores por valor conservan contexto de valor.
- La misma regla de referencia se mantiene para `AsyncIterator<T, E>`.
- El valor aplica exactamente una vez las postcondiciones de frontera de
  expresión.
- `SuspendPoint` permanece hoja con estado de reanudación uno.
- `YieldBreak` conserva el structured exit, su scope, cleanup y continuación
  `Return`.
- El `yield return` sin hijo conserva su representación hoja histórica.
- Éxito y error no alteran la pila exterior de cleanup.

## Cierre del call graph

La variante productiva `YieldReturnStatement` se programa desde la entrada
raíz y desde todos los routers statement que podían volver a
`lowerStatement`: bloques, secciones `switch`, source de `foreach`, alternativa
directa de `if` y declaraciones sintéticas de `match`. La cadena profunda
`yield return -> LoopExpression -> Block -> yield return` ya no consume pila
C++. `HirAsyncLowering.cpp` queda sin llamadas a `lowerExpression`.

Complejidad:

- tiempo `O(N)`;
- memoria explícita `O(D)`;
- pila C++ de las aristas migradas `O(1)`.

## Evidencia focal

- Target exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector único: `hir-coroutine-statement-depth`, PASS 8/8, exit 0.
- Contrafactual profundo: 32,768 aristas productivas `yield return`.
- Satélites: cleanup capturado, continuación, yield por referencia y por valor,
  superficie async-iterator, `SuspendPoint`, `YieldBreak` y yield vacío.
- La primera compilación focal detectó exclusivamente un nombre incorrecto de
  enum en la prueba (`IdentifierNameExpression`); se corrigió a su nombre
  canónico `IdentifierExpression` y se repitió sólo el mismo target.
- No se ejecutaron CTest, suites, baseline, instalación ni deploy.

## Definition of done

H02S16N queda cerrada cuando este cambio se integra como commit aislado, el
target puntual compila, el selector focal pasa y ambos trackings llegan a cero
hojas pendientes.
