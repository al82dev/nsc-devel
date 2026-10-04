# NS3-T09-F05 — Generadores síncronos / Synchronous generators

## Español

### Resultado

El compilador dispone de un plan de lowering determinista para convertir un generador síncrono en
una máquina de estados nativa. La entrada es el resultado del análisis semántico —locals vivos a
través de cada `yield`, suspensiones, préstamos y profundidad de cleanup— y la salida congela:

- estados de suspensión contiguos y un estado terminal;
- tipo único de item producido;
- slot, offset, tamaño, alineación y rango de vida de cada local hoisted;
- mapa exacto de locals vivos por suspensión;
- profundidad de `defer`/`finally` que debe conservar cada estado;
- orden inverso de destrucción al cancelar;
- necesidad de pinning para self-references o items prestados;
- storage elegido por caller: stack, buffer del caller, arena o heap explícito.

El planner jamás cambia la política de storage ni introduce una reserva implícita. Un frame pinned
puede vivir en storage estable elegido por el usuario; “pinned” significa que no se mueve después
de iniciarse, no que deba alojarse en heap.

### Reglas cerradas

- El estado cero representa entrada; los yields productivos usan `1..N`; `N+1` es completed.
- Todos los yields deben resolver al mismo tipo de item.
- Un yield por referencia sólo es válido si borrow analysis lo enlazó al frame o a una captura
  cuya vida está transportada por el frame.
- Sólo un local vivo a través de al menos un yield ocupa un slot.
- El intervalo de vida del slot es inclusivo y debe referirse a estados reales.
- Cancelar destruye únicamente slots activos con drop, en orden inverso.
- Reanudar un estado terminal o desconocido no produce transición.
- Tamaños, alineamientos e overflow se validan antes de crear HIR.

### Archivos

- `include/nativesharp/Language/IterationSemantics.h`: contrato del plan, slots, estados, storage,
  yields y operaciones de cancelación/reanudación.
- `src/Language/IterationSemantics.cpp`: validación, layout, liveness, pinning y transiciones.
- `tests/unit/NativeSharp3T09SynchronousGeneratorTests.cpp`: prueba puntual del contrato completo.
- `CMakeLists.txt`: target aislado `nsc_ns3_t09_sync_generator_tests`.

### Validación dirigida

La prueba puntual cubre múltiples yields, layout, locals move-only/RAII, profundidad de cleanup,
yield prestado, pinning, cancelación/drop, storage explícito, transiciones, tipos incompatibles,
borrow inválido, estados discontinuos e overflow. No se ejecutó baseline ni CTest general.

## English

### Result

The compiler now owns a deterministic lowering plan that turns a synchronous generator into a
native state machine. Its input is semantic-analysis output—locals live across yields,
suspensions, borrows and cleanup depth—and its output freezes:

- contiguous suspension states plus one terminal state;
- one yielded item type;
- slot, offset, size, alignment and live-state interval for every hoisted local;
- the exact live-local map at each suspension;
- the `defer`/`finally` cleanup depth preserved by each state;
- reverse destruction order on cancellation;
- pinning for self references or borrowed items;
- caller-selected storage: stack, caller buffer, arena or explicit heap.

The planner never changes storage policy and never performs an implicit allocation. A pinned frame
may reside in any stable user-selected storage; pinned means non-movable after start, not
heap-required.

### Closed rules

- State zero is entry, productive yields use `1..N`, and `N+1` is completed.
- Every yield must resolve to the same item type.
- A reference yield is valid only when borrow analysis ties it to frame-owned storage or to a
  captured source whose lifetime is carried by the frame.
- Only locals live across at least one yield receive frame slots.
- Slot liveness intervals are inclusive and must name real suspension states.
- Cancellation drops only active drop-bearing slots, in reverse order.
- Completed or unknown states cannot resume.
- Size, alignment and overflow are validated before HIR construction.

### Focused validation

The dedicated test covers multiple yields, layout, move-only/RAII locals, cleanup depth, borrowed
yields, pinning, cancellation/drop, explicit storage, transitions, mismatched item types, invalid
borrows, discontinuous states and frame overflow. No baseline or general CTest suite was run.
