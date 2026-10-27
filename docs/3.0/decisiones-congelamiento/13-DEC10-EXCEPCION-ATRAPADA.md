# Congelamiento 3.0 DEC-10 / D-02 — la excepción atrapada pertenece a su catch

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `01e37532e`.

## Propósito y problema

C-01 hizo al `catch` dueño de la excepción que maneja: la destruye al terminar el handler (salida normal,
`break`, `continue`, `return` o una excepción nueva) y la entrega en un rethrow. Hallazgo D-02 de la auditoría
(`X1CatchSaved`):

```
catch (Boom problem) { holder.Saved = problem; }
```

El programa compilaba, el catch destruía la excepción y `holder.Saved.Code` se leía de memoria liberada (exit
107).

Decisiones de Alberto Huerta Luna (2026-10-26):

- DEC-10: la excepción atrapada pertenece al contexto de unwind durante todo el catch, y solo se destruye cuando
  termina el handler y ya no queda ninguna referencia válida.
- Respuesta de diseño: guardar el préstamo fuera del handler es error de compilación; solo `move(e)` la saca; si
  no se mueve, se destruye al final del handler.

## SCOPE LOCK

- **Objetivo.** Ningún programa que compile puede leer la excepción atrapada después de que su catch la destruya.
  `move(e)` la transfiere sin doble destrucción.
- **Naturaleza.** Corrección de contrato (C-01) por decisión explícita.
- **Debe quedar idéntico.**
  - Destrucción al final del handler y entrega en `throw;`/`throw e;`.
  - Lectura, paso como argumento y alias dentro del handler.
  - Los casos de `C01CatchOwnership`.
- **Modificable.**
  - Análisis semántico: catch, asignación, return, move, lectura, `throw;`.
  - `MethodLocalSymbol.h` y `ProjectMethodBodyModel.h`.
  - En LLVM, el `move` de un valor puntero dentro de un catch (`OopLlvmExpressionDispatch.cpp`,
    `OopLlvmCleanup.cpp`, `OopLlvmImplementation.h`).
  - `Freeze30ContractTests.cpp`.
- **Prohibido.** Volver a la fuga histórica; cambiar el layout del frame del catch.
- **Terminación.** Sondas de abajo en `-O0` y `-O2`.

## Contrato

1. La variable del catch y cada local del mismo handler que la copia (`T alias = e;`, o `alias = e;` si `alias`
   se declaró dentro del handler) nombran la excepción atrapada.
2. Guardar cualquiera de ellos donde sobrevive al handler es error: un campo, un static, un elemento, un
   parámetro, un local declarado fuera del handler o un `return`. Mensaje: «the caught exception in 'x' belongs to
   its catch until the handler ends…».
3. Leerla, pasarla como argumento y lanzarla (`throw e;`) siguen permitidos.
4. `move(e)` sobre la variable del catch la saca. El backend deja nulo el slot de dueño del catch cuyo objeto es
   ése, así el catch no la destruye al terminar. Después:
   - leer `e` es «use of moved-from caught exception»;
   - moverla otra vez es error;
   - `throw;` es «throw; rethrows the caught exception, but 'e' was moved out of its catch».
5. Si `move(e)` ocurre en un solo camino, la decisión es dinámica (compara el puntero con el slot de dueño). Cada
   camino destruye la excepción exactamente una vez.

Límite documentado: un método al que se le pasa la excepción como argumento y que la guarda en un campo no dueño
no se detecta (`x10`). Es la misma regla que para cualquier referencia no dueña de la base histórica; el análisis
cubre los almacenamientos visibles en el handler.

## Cambio

- `MethodLocalSymbol`: `isCatchVariable` y `caughtExceptionScopeOrdinal`. Se fijan en `addCatchLocal` y en los
  locales cuyo inicializador nombra la excepción atrapada.
- `ProjectMethodBodyModel` agrega cinco funciones:
  - `caughtExceptionScopeOfValue` y `scopeIsWithin`;
  - `rejectCaughtExceptionAssignment`, desde `prepareAssignmentValue`;
  - `reportCaughtExceptionReturn`, en `reportDirectOwnerReturnWithoutMove` y en el return de lambda;
  - `reportRethrowOfMovedCatch`, en `throw;`.
- `analyzeNamedLocalMove` acepta la variable del catch como fuente que requiere move.
  `validateLocalIdentifierRead` y `validateLocalMoveState` la siguen como movida y nombran «caught exception».
- LLVM: `emitCatchOwnershipHandover` (extraída de `emitCatchOwnershipOnThrow`) deja nulo el slot de dueño que
  contiene el valor. `MoveValue` la llama para cada catch activo cuando mueve un puntero.
- `Freeze30ContractTests`: `testCaughtExceptionStaysInCatch`, con 7 rechazos y 7 ejecuciones.

## Validación

nsc Release recompilado (exit 0). Sondas en `A:\temp\nsc-dec\d10`:

| Sonda | Antes | Ahora (`-O0` y `-O2`) |
|---|---|---|
| `x1` (`X1CatchSaved`, campo) | exit 107 (lectura de memoria liberada) | rechazo |
| `x3` (local exterior), `x7` (static), `x4` (return) | leía memoria liberada | rechazo |
| `x12` (alias local guardado en campo) | — | rechazo |
| `x2` (`holder.Saved = move(problem)`, campo `owned`) | no compilaba | 17: 0 en el handler, 1 al destruir `holder` |
| `x8` (`raii Boom keep = move(problem)`) | no compilaba | 1107: se destruye al salir `keep`, no al final del handler |
| `x9` (`return move(problem)` a `owned Boom`) | no compilaba | 7 |
| `x14` (move en un solo camino) | — | 1207: una destrucción por llamada |
| `x18` (`throw move(problem)`) | — | 1: se destruye solo en el catch exterior |
| `x13` (`throw;` después de move), `x17` (leer después de move) | — | rechazo |
| `x5`, `x11`, `x16` (lectura, alias y argumento dentro del handler) | 114 | 114, 120, 107 |
| `C01CatchOwnership` | 0 | 0 |

Proyectos con `catch` del repositorio verificados con `--verify-project-oop-llvm`, todos exit 0:

- `NativeSharp.Core.Collections.Generic`;
- book_examples 01, 06, 10 y 27;
- tutoriales 12 y 19;
- cc1 c4, c5, c6, c7, c8 y c65;
- integration debug_pdb y debug_values_t37;
- interop c1_14.

`26_deitel_live_code_native` no se pudo verificar: su referencia `NativeSharp.Core.IO` no está construida en este
árbol, sin relación con este cambio. El caso histórico `Stage56C7DefiniteAssignmentTests`
(`Exception again = e;` dentro del handler) es `x11` y compila.

## No verificado

- Unitarias (`Freeze30ContractTests`, Stage45, Stage56) y batería completa: cierre final.
