# NSC 3.0.5 — H02S16M · `try/catch/finally` HIR iterativo

## SCOPE LOCK

Esta hoja migra exclusivamente `TryStatement` a la worklist HIR compartida.
No cambia sintaxis, semántica, diagnósticos, ABI, NSMETA, MIR ni LLVM. No
modifica `suspend` ni `yield`, reservados para H02S16N.

## Contrato preservado

- `TryRegion` reserva su ordinal HIR antes de validar o bajar cualquier hijo.
- La región de cleanup de `finally`, cuando existe, se activa antes del bloque
  protegido.
- El handler del `try` se activa únicamente durante el bloque protegido y
  retiene la profundidad de cleanup que incluye la región del propio `try`.
- Filtros y cuerpos `catch` se bajan en orden fuente con el cleanup del `try`
  activo, pero sin el handler del mismo `try`.
- Cada `CatchRegion` conserva tipo, ordinal, wildcard, filtro, exhaustividad,
  errores cubiertos y local capturado.
- El filtro precede al cuerpo dentro del `CatchRegion`.
- El cleanup del `try` se retira antes de bajar `finally`.
- `FinallyRegion` conserva ordinal de cleanup y publica una continuación normal
  después de su cuerpo.
- La prioridad histórica de diagnósticos se conserva: bloque protegido,
  bindings `catch` en orden y, finalmente, presencia de cleanup para `finally`.
- Éxito y error restauran exactamente las pilas exteriores de cleanup y
  handlers; no se publica una región parcial.

## Cierre del call graph

La variante `TryStatement` se programa desde la entrada raíz y desde todos los
routers statement que podían volver a `lowerStatement`: bloques, secciones
`switch`, source de `foreach`, alternativa directa de `if` y declaraciones
sintéticas de `match`. La cadena profunda `try -> Block -> try` ya no consume
pila C++.

Complejidad:

- tiempo `O(N + C)`, donde `C` es el total de cláusulas `catch`;
- memoria explícita `O(D)`;
- pila C++ de las aristas migradas `O(1)`.

## Evidencia focal

- Target exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector único: `hir-try-statement-depth`, PASS 10/10, exit 0.
- Contrafactual profundo: 32,768 `try` anidados.
- Satélites: orden y ordinales de regiones, metadata completa de dos `catch`,
  filtro, local, handler del bloque protegido, cleanup activo en `catch`,
  cleanup retirado en `finally`, continuación normal, diagnósticos exactos y
  rollback conjunto de cleanup/handlers.
- No se ejecutaron CTest, suites, baseline, instalación ni deploy.

## Definition of done

H02S16M queda cerrada cuando este cambio se integra como commit aislado, el
target puntual compila, el selector focal pasa y el tracking deja únicamente
H02S16N pendiente.
