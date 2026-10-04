# NS3-T10-F01 — Funciones async nativas / Native async functions

## Español

### Resultado

El compilador construye un plan nativo de función `async` a partir de sus puntos `await` y del
análisis de liveness. El tipo devuelto es un Future opaco y determinista; la ejecución empieza de
forma lazy. El frame usa exactamente el mismo contrato de slots y estados que el generador
síncrono, evitando dos ABIs de coroutine incompatibles.

El caller/runtime selecciona explícitamente stack, arena o heap. El planner no promueve storage a
heap ni reserva memoria. Captures y locals vivos a través de `await` determinan tamaño, alineación,
pinning y cleanup depth del frame. Un borrow sólo cruza `await` cuando se prueban simultáneamente
la vida del source y la estabilidad de su dirección.

Una función `async` sin `await` conserva el tipo Future opaco pero se marca como
`completesSynchronously`; no crea estados de suspensión ni acepta falsos locals live-across-await.

### Archivos y prueba puntual

- `CoroutineSemantics.h/.cpp`: `AwaitSuspension`, `AsyncFunctionPlan` y builder determinista.
- `NativeSharp3T10AsyncFunctionTests.cpp`: nueve comprobaciones de start, layout, estados,
  cleanup, pinning, identidad, storage, borrows y función sin await.
- `CMakeLists.txt`: target aislado `nsc_ns3_t10_async_function_tests`.

No se ejecutó baseline, CTest general ni matriz de optimización.

## English

### Result

The compiler builds a native `async` function plan from its await points and liveness analysis.
The returned type is an opaque deterministic Future and execution starts lazily. Its frame uses
the exact same slot/state contract as synchronous generators, preventing incompatible coroutine
ABIs.

The caller/runtime explicitly selects stack, arena or heap storage. The planner neither promotes
storage to heap nor allocates memory. Captures and locals live across `await` determine frame size,
alignment, pinning and cleanup depth. A borrow crosses `await` only when both source lifetime and
address stability are proven.

An `async` function without `await` retains its opaque Future type but is marked
`completesSynchronously`; it has no suspension states and rejects impossible live-across-await
locals.

### Focused validation

The dedicated nine-check executable covers start policy, layout, states, cleanup, pinning, opaque
identity, explicit storage, borrows and no-await completion. No baseline, general CTest suite or
optimization matrix was run.
