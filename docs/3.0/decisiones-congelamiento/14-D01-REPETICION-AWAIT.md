# Congelamiento 3.0 D-01 — un await reanudado no repite los operandos de su sentencia

Fecha: 2026-10-26. Rama: `development-release-candidate`. Revisión de partida: `93bda0690`.

## Propósito y problema

C-02 hizo de cada poll una corrutina con frame: al reanudar, el despacho salta al inicio de la sentencia más
interna que rodea al punto que suspendió y la vuelve a ejecutar. Los `await` ya completados de esa sentencia leen
su resultado guardado. Pero todo lo demás que corre antes del `await` se ejecutaba otra vez. Hallazgo D-01 de la
auditoría:

```
int x = Count() + await Slow();      // Count() corría dos veces
values[Next()] = await Slow();       // el índice se tomaba dos veces y se escribía values[1]
```

Al revisar los ciclos aparecieron dos casos más del mismo defecto, ahora con la sentencia de control como destino
de la reanudación:

- un `await` en la condición o el paso de un `for` reanudaba en el `for` y volvía a correr el inicializador. Con
  `for (int i = 0; await Less(i); ...)` el ciclo no terminaba;
- un `await` en la condición de un `do … while` reanudaba en el `do` y volvía a correr el cuerpo.

Decisión de Alberto Huerta Luna: corregir D-01 antes del congelamiento.

## SCOPE LOCK

- **Objetivo.** En un poll, cada operando de la sentencia reanudada corre una vez por ejecución de la sentencia.
  Una condición de `for` o `do` con `await` reanuda en la condición.
- **Naturaleza.** Corrección del contrato C-02.
- **Debe quedar idéntico.**
  - Ids de estado, despacho, slots de los `await` y orden del frame anterior.
  - Generadores síncronos (su `yield` es sentencia y no tiene operandos hermanos).
  - El corpus de 51 programas con `await`.
- **Modificable.**
  - MIR: `MirFunctionLowerer.cpp` (frame del poll), `ProjectOopMirNode.h`, `MirCache.cpp` y
    `ProjectOopMirCacheIdentity.cpp`.
  - LLVM: `OopLlvmCoroutines.cpp`, `OopLlvmCoroutineState.h`, `OopLlvmImplementation.h`,
    `OopLlvmExpressionDispatch.cpp`, `OopLlvmExpressionMachine.cpp`, `LlvmExpressionFrame.h` y
    `OopLlvmControlFlow.cpp`.
  - `Freeze30ContractTests.cpp`.
- **Prohibido.**
  - Volver al protocolo de repetición desde arriba.
  - Cambiar la semántica de los programas sin `await`.
- **Terminación.** Sondas R1–R13 en `-O0` y `-O2`; corpus de `await` sin cambios.

## Contrato

1. MIR (`assignReplaySpillSlotNodes`, en `buildPollFrame`). Para cada `await` que puede suspender, el compilador
   sube de nodo en nodo hasta su sentencia. En cada nivel, todo hijo fuera del camino recibe un slot propio del
   frame (`replaySpillOffset`).
   - El ascenso se corta antes de una sentencia de control: bloque, `if`, ciclos, `switch`, `try`, `using`,
     `match` y `loop`.
   - Se corta después de una sentencia simple: declaración, sentencia de expresión, `return`, `yield`.
   - Las constantes, `this` y la dirección fija de una variable no reciben slot, porque repetirlas no cambia
     nada. Tampoco el almacenamiento de la pila del propio poll.
   - Una dirección ocupa 8 bytes. Un valor ocupa lo que dice su layout, con un mínimo de 8. Si el layout no se
     conoce, no hay slot.
2. LLVM. Al evaluar un operando con slot, el código mira el estado de reanudación.
   - Si nombra un `await` para el que este operando está fuera del camino, carga el slot.
   - Si no, lo evalúa y lo guarda. Un `phi` une los dos caminos.
   - Un operando que corre después del `await` también se guarda, pero no se carga nunca. La regla no depende
     del orden de evaluación de cada nodo.
3. Se guarda solo lo que sobrevive al poll:
   - un valor de su propio tipo LLVM;
   - una dirección cuya raíz es el frame, una celda capturada, el heap o almacenamiento del módulo.

   Cualquier otra forma, por ejemplo un struct representado por la dirección de un temporal, vuelve a evaluarse
   como antes de D-01. También se vuelve a evaluar un operando cuya evaluación nunca llega al `await`.
4. El dueño oculto de un temporal `owned` (`OwnedTemporaryBorrow`) vive en el frame como los demás locales. La
   repetición lo conserva y el temporal se destruye una vez.
5. Una condición de `for` o de `do` que contiene `await` es destino de reanudación propio, y también un paso de
   `for` que no es bloque. La reanudación ya no vuelve a correr el inicializador ni el cuerpo.

## Cambio

- `ProjectOopMirNode::replaySpillOffset`, serializado en la caché MIR y en la identidad de caché.
- `MirFunctionLowerer.cpp` agrega estas funciones y asigna los slots después de los `await` y antes de los
  locales:
  - `replayClimbStopsBefore`, `replayClimbStopsAfter`;
  - `replaySpillCandidateKind`, `replaySpillAddressKind`;
  - `assignReplaySpillSlotNodes`.

  `appendCoroutineFrameLocalNodes` incluye `OwnedTemporaryBorrow`.
- `collectCoroutineSuspensions` anota, para cada operando con slot, los ids cuya repetición lo carga
  (`OopLlvmCoroutineState::replayIds`).
- Funciones nuevas en `OopLlvmCoroutines.cpp`:
  - `beginCoroutineReplaySpill` y `finishCoroutineReplaySpill`;
  - `coroutineReplaySpillable` y `coroutineReplayAddressStable`.

  `lowerExpression` las usa alrededor de un nodo. La máquina iterativa (`lowerExpressionIterative`) las usa con un
  frame marcador (`LlvmExpressionFrame::replayJoin`). `replayEntered` evita envolver dos veces el mismo nodo.
- `lowerFor` y `lowerDoWhile` llaman a `enterCoroutineResumeTarget` sobre la condición (y sobre el paso del
  `for`).
- `Freeze30ContractTests`: `testAwaitReplayRunsOperandsOnce`, 11 ejecuciones.

## Validación

nsc Release, `nsc_component_tests` Debug y Release recompilados, todos con exit 0. Sondas en
`A:\temp\nsc-dec\d01`, iguales en `-O0` y `-O2`:

| Sonda | Forma | Antes | Ahora |
|---|---|---|---|
| R1 | `Count() + await Slow()` | `Count` 2 veces | 106 |
| R2 | `await A() * 10 + Count() + await B()` | repetía | 113 |
| R3 | `Count() == 1 && await Slow() == 5` | repetía | 107 |
| R4 | `values[Next()] = await Slow()` | escribía `values[1]` | 150 |
| R5 | `total + Count() * await Slow()` en `for` | repetía | 306 |
| R6 | generador con `yield return Count()` | 212 | 212 |
| R7 | `for (int i = 0; await Less(i); …)` | no terminaba | 33 |
| R8 | `do { … } while (await Less(n))` | 44 (cuerpo repetido) | 33 |
| R9 | `Add3(Count(), await Slow(), Count())` | — | 207 |
| R11 | `Flag() ? Count() + await Slow() : 0` | — | 206 |
| R12 | `Sum(Make(), await Slow())`, `Make` devuelve struct | — | 108 |
| R13 | `Pick(h).X = await Slow()` | — | 107 |

- R1–R5: los valores de antes son los de la auditoría.
- R7 y R8: contrafactual medido el mismo día. Se reconstruyó `nsc` con el `OopLlvmControlFlow.cpp` de `HEAD`; luego
  se restauró el archivo (`fc` idéntico) y se reconstruyó.

Corpus de `await` (`A:\temp\nsc-dec\aw`, 51 programas, run `d01` contra `d04`): mismos resultados de compilación y
mismos códigos de salida en los 44 ejecutables.

## Observaciones

- `Use(new Box(3))` no ejecuta el destructor de `Box` antes de la sentencia siguiente, tampoco en un `Main`
  síncrono (`R10Sync`, `Freed` = 0). D-01 conserva ese comportamiento (`R10Temp`, `R10After`: 108). Queda
  registrado como hallazgo aparte, sin relación con la reanudación.
- Un `await` anidado en el operando de otro `await` (`await F(await G())`) y una expresión de bloque con `await`
  en su valor final siguen el modelo de C-02, sin cambios.
- Un operando `owned` guardado en su slot, con la corrutina abandonada mientras está suspendida, no se destruye.
  Antes de D-01 la repetición creaba otro objeto y perdía el primero; ahora existe uno solo.

## No verificado

- Unitarias: `Freeze30ContractTests` no está configurado en los árboles locales; sus 11 casos son las sondas de
  arriba.
- Batería completa: queda para el cierre final.
