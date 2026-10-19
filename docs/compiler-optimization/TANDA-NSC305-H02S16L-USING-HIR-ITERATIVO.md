# NSC 3.0.5 — H02S16L · `using` HIR iterativo

## SCOPE LOCK

Esta hoja migra exclusivamente `UsingStatement` a la worklist HIR compartida.
No cambia sintaxis, semántica, diagnósticos, ABI, NSMETA, MIR ni LLVM. No
modifica `try/catch/finally` ni coroutines, reservados para H02S16M/N.

## Contrato preservado

- El binding de `using` y su región de cleanup se validan antes de visitar el
  inicializador.
- `UsingRegion` conserva local, nombre, tipo del recurso, ordinal de cleanup,
  contrato/implementación de `Dispose` y modo de dispatch.
- El inicializador se baja en contexto de valor, aplica una sola vez las
  postcondiciones de frontera y después su conversión implícita congelada.
- La región de cleanup se activa únicamente después de completar el
  inicializador y permanece activa durante todo el cuerpo.
- El cuerpo conserva forma `Block` y orden de statements.
- La continuación normal se publica como tercer hijo, después del
  inicializador y del cuerpo.
- Éxito y error restauran exactamente la profundidad de cleanup exterior.
- No se publica un `UsingRegion` parcial.

## Cierre del call graph

La misma variante `UsingStatement` se programa desde la entrada raíz y desde
todos los routers statement que podían volver a `lowerStatement`: bloques,
secciones `switch`, source de `foreach`, alternativa directa de `if` y
declaraciones sintéticas de `match`. La cadena profunda
`using -> Block -> using` ya no consume pila C++.

Complejidad:

- tiempo `O(N)`;
- memoria explícita `O(D)`;
- pila C++ de las aristas migradas `O(1)`.

## Evidencia focal

- Target exacto: `nsc_project_oop_lowering_tests`, PASS.
- Selector único: `hir-using-statement-depth`, PASS 6/6, exit 0.
- Contrafactual profundo: 32,768 `using` anidados.
- Satélites: metadata completa, orden de hijos, continuación normal,
  conversión implícita, precedencia del binding ausente y rollback exacto del
  stack de cleanup ante cuerpo inválido.
- No se ejecutaron CTest, suites, baseline, instalación ni deploy.

## Definition of done

H02S16L queda cerrada cuando este cambio se integra como commit aislado, el
target puntual compila, el selector focal pasa y el tracking deja únicamente
H02S16M/N pendientes.
